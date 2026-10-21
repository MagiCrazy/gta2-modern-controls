#include "plugin.h"
#include "CGame.h"
#include "CHud.h"
#include "CObject.h"
#include "CPed.h"
#include "CPlayerPed.h"
#include "CReplay.h"

#include "controls.h"

#include "gameutil.h"
#include "log.h"
#include "memutil.h"
#include "pad.h"
#include "settings.h"

#include <cmath>
#include <cstdlib>

namespace gmc {
namespace controls {

namespace {

// CPlayerPed+0x2F (field_47): 1 while a script has the player's controls off
// (set by SetAllControlStatusOff 0x4A69A0, cleared by SetAllControlStatusOn 0x4A4E50).
const uintptr_t kControlsOffOffset = 0x2F;

// Heading units: degrees x 4, 0 = facing down, increasing counter-clockwise
// (720 = up), as measured by the phase 1 probe.
const int kFullTurn = 1440;
const double kPi = 3.14159265358979323846;

bool g_prevR3 = false;

int NormalizeHeading(int h) {
    h %= kFullTurn;
    return h < 0 ? h + kFullTurn : h;
}

// Shortest signed difference to - from, in -720..719.
int HeadingDelta(int from, int to) {
    int d = NormalizeHeading(to - from);
    return d >= kFullTurn / 2 ? d - kFullTurn : d;
}

// Stick (positive right / up) to heading. Dege's formula: atan2(y, x) + 90 deg.
int HeadingFromStick(int x, int y) {
    double deg = std::atan2(static_cast<double>(y), static_cast<double>(x)) * 180.0 / kPi + 90.0;
    return NormalizeHeading(static_cast<int>(std::lround(deg * 4.0)));
}

void ShowBrief(const wchar_t* text) {
    CHud* hud = NULL;
    if (!mem::Read(reinterpret_cast<uintptr_t>(gHud), hud) || !mem::IsReadable(hud, sizeof(CHud)))
        return;
    hud->m_HudBrief.SetHudBriefStr(text, 0);
}

// Select + R3 (rising edge of R3) toggles modern / classic controls.
void HandleToggle(const pad::State& pad) {
    bool pressed = pad.r3 && !g_prevR3 && pad.select;
    g_prevR3 = pad.r3;
    if (!pressed)
        return;

    bool on = !settings::Get().enabled;
    settings::SetEnabled(on);
    ShowBrief(on ? L"Modern controls" : L"Classic controls");
    log::Write("controls: toggled %s", on ? "MODERN" : "CLASSIC");
}

void ApplyVehicle(CPlayerPed* player, const pad::State& pad, int threshold) {
    // Left / right (stick X, D-pad) and enter / exit stay as GInput set them.
    player->m_bButtonForward = pad.r2 > threshold;
    player->m_bButtonBackward = pad.l2 > threshold;
    player->m_bButtonAttack = pad.square;
    player->m_bButtonHandbrakeJump = pad.cross;
    player->m_bButtonPrevWeapon = pad.l1;
    player->m_bButtonNextWeapon = pad.r1;
    player->m_bButtonSpecial2 = pad.l3;
}

void ApplyOnFoot(CPlayerPed* player, CPed* ped, const pad::State& pad, const settings::Settings& cfg) {
    switch (ped->m_nPedState) {
    case PEDSTATE_ENTER_CAR:
    case PEDSTATE_EXIT_CAR:
    case PEDSTATE_FALL:
    case PEDSTATE_DEAD:
        return;
    default:
        break;
    }
    if (ped->m_nPedFlags.bIsArrested)
        return;
    CObject* obj = ped->m_pObject;
    if (!mem::IsReadable(obj, sizeof(CObject)))
        return;

    int x = pad.leftX;
    int y = pad.leftY;
    if (x == 0 && y == 0) {
        x = (pad.dpadRight ? 255 : 0) - (pad.dpadLeft ? 255 : 0);
        y = (pad.dpadUp ? 255 : 0) - (pad.dpadDown ? 255 : 0);
    }

    bool walk = false;
    if (x != 0 || y != 0) {
        int current = NormalizeHeading(obj->m_nRotation);
        int delta = HeadingDelta(current, HeadingFromStick(x, y));
        int step = delta > cfg.turnStep ? cfg.turnStep : (delta < -cfg.turnStep ? -cfg.turnStep : delta);
        obj->m_nRotation = static_cast<short>(NormalizeHeading(current + step));
        walk = std::abs(delta - step) <= cfg.walkTolerance;
    }

    player->m_bButtonForward = walk;
    player->m_bButtonBackward = false;
    player->m_bButtonLeft = false;
    player->m_bButtonRight = false;
    player->m_bButtonAttack = pad.r2 > cfg.triggerThreshold;
    player->m_bButtonHandbrakeJump = pad.cross;
    player->m_bButtonPrevWeapon = pad.l1;
    player->m_bButtonNextWeapon = pad.r1;
    player->m_bButtonSpecial2 = pad.l2 > cfg.triggerThreshold || pad.l3;
}

} // namespace

void Apply(CPlayerPed* player) {
    CGame* game = SafeGame();
    if (!game || !player || player != game->m_pCurrentPlayer)
        return;
    if (game->m_eGameStatus != GAME_RUN)
        return;
    if (!mem::IsReadable(player, sizeof(CPlayerPed)))
        return;
    if (mem::IsReadable(gReplay, sizeof(CReplay)) && gReplay->IsPlayingBack())
        return;

    pad::State pad;
    if (!pad::Read(pad))
        return;

    HandleToggle(pad);
    const settings::Settings& cfg = settings::Get();
    if (!cfg.enabled)
        return;

    uint8_t controlsOff = 1;
    if (!mem::Read(reinterpret_cast<uintptr_t>(player) + kControlsOffOffset, controlsOff) || controlsOff)
        return;

    CPed* ped = player->GetPed();
    if (!mem::IsReadable(ped, sizeof(CPed)))
        return;

    if (ped->GetCar()) {
        if (cfg.vehicle && ped->m_nPedState == PEDSTATE_IN_CAR)
            ApplyVehicle(player, pad, cfg.triggerThreshold);
        return;
    }
    if (cfg.onFoot)
        ApplyOnFoot(player, ped, pad, cfg);
}

} // namespace controls
} // namespace gmc
