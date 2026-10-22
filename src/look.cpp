#include "plugin.h"
#include "CCamera.h"
#include "CGame.h"
#include "CPlayerPed.h"

#include "look.h"

#include "controls.h"
#include "gameutil.h"
#include "log.h"
#include "memutil.h"
#include "pad.h"
#include "settings.h"

#include <cmath>
#include <cstring>

namespace gmc {
namespace look {

namespace {

// CPlayerPed::Process (0x4A7E80) calls the look step 0x4A4CF0(camera) from
// camera mode 0 (normal, m_GameCamera) and mode 2 (m_AuxCamera). The step
// pans the offset while Special 2 is held, or resets it to 0 every frame.
const uintptr_t kLookStep = 0x4A4CF0;
const uintptr_t kCallSites[] = {0x4A7F98, 0x4A8025};

// Look offset in the camera, Fix16 (1.0 = 0x4000): +X right, +Y down.
const uintptr_t kOffsetX = 0x48;
const uintptr_t kOffsetY = 0x4C;
const double kFix16 = 16384.0;
// Game limits and max look speed per frame (0x5E3CD0, 0x5E3E74, 0x5E3EC8).
const double kMaxX = 300.0;
const double kMaxY = 220.0;
const int kMaxStep = static_cast<int>(20.0 * kFix16);

plugin::ThiscallEvent<plugin::AddressList<0x4A7F98, plugin::H_CALL, 0x4A8025, plugin::H_CALL>, plugin::PRIORITY_AFTER,
                      plugin::ArgPick2N<CPlayerPed*, 0, CCamera*, 1>, void(CPlayerPed*, CCamera*)>
    onLookStep;

// Our current offset; the game resets the camera's copy every frame.
int g_x = 0;
int g_y = 0;

bool CallsLookStep(uintptr_t site) {
    uint8_t code[5];
    if (!mem::ReadBytes(site, code, sizeof(code)) || code[0] != 0xE8)
        return false;
    int32_t rel = 0;
    std::memcpy(&rel, code + 1, sizeof(rel));
    return site + 5 + static_cast<uintptr_t>(rel) == kLookStep;
}

int StepTowards(int current, int target) {
    int d = target - current;
    if (d > kMaxStep)
        d = kMaxStep;
    else if (d < -kMaxStep)
        d = -kMaxStep;
    return current + d;
}

// Target offset from the right stick: dead zone removed, then scaled to the
// game's range times rangePercent.
void TargetFromStick(const pad::State& pad, int rangePercent, int& tx, int& ty) {
    tx = 0;
    ty = 0;
    double rx = pad.rightX;
    double ry = pad.rightY;
    double mag = std::sqrt(rx * rx + ry * ry);
    double dz = settings::Get().aimDeadZone;
    if (mag <= dz)
        return;
    double t = (mag - dz) / (255.0 - dz);
    if (t > 1.0)
        t = 1.0;
    double scale = t * rangePercent / 100.0 / mag;
    tx = static_cast<int>(std::lround(rx * scale * kMaxX * kFix16));
    ty = static_cast<int>(std::lround(-ry * scale * kMaxY * kFix16)); // stick up = screen up = -Y
}

void OnLookStep(CPlayerPed* player, CCamera* camera) {
    CGame* game = SafeGame();
    if (!game || player != game->m_pCurrentPlayer)
        return;

    const settings::Settings& cfg = settings::Get();
    int tx = 0;
    int ty = 0;
    controls::Context ctx = controls::CurrentContext();
    pad::State pad;
    if (cfg.enabled && cfg.look && ctx != controls::CONTEXT_NONE && pad::Read(pad))
        TargetFromStick(pad, ctx == controls::CONTEXT_IN_CAR ? cfg.lookCarRange : cfg.lookFootRange, tx, ty);

    // Idle: leave the game's own look (Left Alt + arrows) untouched.
    if (tx == 0 && ty == 0 && g_x == 0 && g_y == 0)
        return;

    g_x = StepTowards(g_x, tx);
    g_y = StepTowards(g_y, ty);

    uintptr_t cam = reinterpret_cast<uintptr_t>(camera);
    if (!mem::IsReadable(cam, sizeof(CCamera)))
        return;
    std::memcpy(reinterpret_cast<void*>(cam + kOffsetX), &g_x, sizeof(g_x));
    std::memcpy(reinterpret_cast<void*>(cam + kOffsetY), &g_y, sizeof(g_y));
}

} // namespace

bool Install() {
    for (uintptr_t site : kCallSites) {
        if (!CallsLookStep(site)) {
            log::Write("look: unexpected code at 0x%08X, right-stick look disabled", static_cast<unsigned int>(site));
            return false;
        }
    }
    onLookStep += OnLookStep;
    log::Write("look: hooked the look step at 0x%08X and 0x%08X", static_cast<unsigned int>(kCallSites[0]),
               static_cast<unsigned int>(kCallSites[1]));
    return true;
}

} // namespace look
} // namespace gmc
