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

enum Mode { MODE_NONE, MODE_FOOT, MODE_CAR, MODE_TRANSITION };

bool g_prevR3 = false;
Mode g_prevMode = MODE_NONE;
// A trigger held while leaving a car must be released before it acts on foot,
// so that accelerating does not turn into shooting.
bool g_r2Latched = false;
bool g_l2Latched = false;

// The CALL at 0x4A7683 is the game's button decode (0x4A4AE0), wrapped by
// GInput. For each button it sets the value and a "changed since last frame"
// flag (plugin-sdk names these m_bOld*); the game treats value && changed as
// a new press. The decode ran with GInput's values, so recover last frame's
// real value (value != changed) and recompute the flag for ours.
void SetButton(bool& button, bool& changed, bool value) {
    bool previous = button != changed;
    button = value;
    changed = value != previous;
}

bool TriggerPressed(int value, bool& latched, int threshold) {
    bool pressed = value > threshold;
    if (!pressed)
        latched = false;
    return pressed && !latched;
}

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

void ApplyVehicle(CPlayerPed* p, const pad::State& pad, int threshold) {
    // Left / right (stick X, D-pad) and enter / exit stay as GInput set them.
    SetButton(p->m_bButtonForward, p->m_bOldButtonForward, pad.r2 > threshold);
    SetButton(p->m_bButtonBackward, p->m_bOldButtonBackward, pad.l2 > threshold);
    SetButton(p->m_bButtonAttack, p->m_bOldButtonAttack, pad.square);
    SetButton(p->m_bButtonHandbrakeJump, p->m_bOldButtonHandbrakeJump, pad.cross);
    SetButton(p->m_bButtonPrevWeapon, p->m_bOldButtonPrevWeapon, pad.l1);
    SetButton(p->m_bButtonNextWeapon, p->m_bOldButtonNextWeapon, pad.r1);
    SetButton(p->m_bButtonSpecial2, p->m_bOldButtonSpecial2, pad.l3);
}

// Entering / leaving a car: the game drives the ped, but GInput's mapping
// would still fire on R2 and switch weapons on L2; keep the triggers inert.
void ApplyTransition(CPlayerPed* p, const pad::State& pad) {
    SetButton(p->m_bButtonAttack, p->m_bOldButtonAttack, false);
    SetButton(p->m_bButtonSpecial2, p->m_bOldButtonSpecial2, false);
    SetButton(p->m_bButtonPrevWeapon, p->m_bOldButtonPrevWeapon, pad.l1);
    SetButton(p->m_bButtonNextWeapon, p->m_bOldButtonNextWeapon, pad.r1);
}

void ApplyOnFoot(CPlayerPed* p, CPed* ped, const pad::State& pad, const settings::Settings& cfg) {
    if (ped->m_nPedState == PEDSTATE_FALL || ped->m_nPedState == PEDSTATE_DEAD || ped->m_nPedFlags.bIsArrested)
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

    bool r2 = TriggerPressed(pad.r2, g_r2Latched, cfg.triggerThreshold);
    bool l2 = TriggerPressed(pad.l2, g_l2Latched, cfg.triggerThreshold);

    SetButton(p->m_bButtonForward, p->m_bOldButtonForward, walk);
    SetButton(p->m_bButtonBackward, p->m_bOldButtonBackward, false);
    p->m_bButtonLeft = false;
    p->m_bButtonRight = false;
    SetButton(p->m_bButtonAttack, p->m_bOldButtonAttack, r2);
    SetButton(p->m_bButtonHandbrakeJump, p->m_bOldButtonHandbrakeJump, pad.cross);
    SetButton(p->m_bButtonPrevWeapon, p->m_bOldButtonPrevWeapon, pad.l1);
    SetButton(p->m_bButtonNextWeapon, p->m_bOldButtonNextWeapon, pad.r1);
    SetButton(p->m_bButtonSpecial2, p->m_bOldButtonSpecial2, l2 || pad.l3);
}

Mode ModeOf(CPed* ped) {
    if (ped->m_nPedState == PEDSTATE_ENTER_CAR || ped->m_nPedState == PEDSTATE_EXIT_CAR)
        return MODE_TRANSITION;
    if (ped->GetCar())
        return ped->m_nPedState == PEDSTATE_IN_CAR ? MODE_CAR : MODE_TRANSITION;
    return MODE_FOOT;
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
    uint8_t controlsOff = 1;
    CPed* ped = player->GetPed();
    if (!cfg.enabled ||
        !mem::Read(reinterpret_cast<uintptr_t>(player) + kControlsOffOffset, controlsOff) || controlsOff ||
        !mem::IsReadable(ped, sizeof(CPed))) {
        g_prevMode = MODE_NONE;
        return;
    }

    Mode mode = ModeOf(ped);
    if (mode == MODE_FOOT && g_prevMode != MODE_FOOT) {
        g_r2Latched = pad.r2 > cfg.triggerThreshold;
        g_l2Latched = pad.l2 > cfg.triggerThreshold;
    }
    g_prevMode = mode;

    switch (mode) {
    case MODE_CAR:
        if (cfg.vehicle)
            ApplyVehicle(player, pad, cfg.triggerThreshold);
        break;
    case MODE_TRANSITION:
        if (cfg.vehicle || cfg.onFoot)
            ApplyTransition(player, pad);
        break;
    case MODE_FOOT:
        if (cfg.onFoot)
            ApplyOnFoot(player, ped, pad, cfg);
        break;
    default:
        break;
    }
}

} // namespace controls
} // namespace gmc
