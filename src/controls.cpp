#include "plugin.h"
#include "CGame.h"
#include "CHud.h"
#include "CObject.h"
#include "CPed.h"
#include "CPlayerPed.h"
#include "CReplay.h"

#include "controls.h"

#include "aim.h"
#include "gameutil.h"
#include "log.h"
#include "memutil.h"
#include "pad.h"
#include "settings.h"

#include <windows.h>
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

// DirectInput codes of the function keys handled by ProcessKeyPresses.
const unsigned short kDikF1 = 0x3B; // previous radio station
const unsigned short kDikF2 = 0x3C; // next radio station
const unsigned short kDikF7 = 0x41; // repeat the last message
const unsigned short kDikF9 = 0x43; // show the district name

pad::State g_prevPad = {};
Mode g_prevMode = MODE_NONE;
// Special (B) is rebuilt from the pad with our own edge tracking: the change
// flag the decode leaves for it can claim a new press every frame while B is
// held (play-test: holding B in a police car never honked, releasing toggled
// the siren), which keeps re-arming the horn / siren logic (0x4260C0).
unsigned g_frame = 0;
unsigned g_specialFrame = 0;
bool g_prevSpecial = false;
unsigned g_spuriousSpecial = 0;
DWORD g_lastSpuriousLog = 0;
// R2 held while leaving a car must be released before it shoots on foot, so
// that accelerating does not turn into shooting.
bool g_r2Latched = false;
Context g_context = CONTEXT_NONE;

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

void SetSpecial(CPlayerPed* p, bool value) {
    bool trackedLastFrame = g_specialFrame + 1 == g_frame;
    bool previous = trackedLastFrame ? g_prevSpecial : p->m_bButtonSpecial != p->m_bOldButtonSpecial;
    // Diagnostics: the decode reports a change although B stayed held.
    if (trackedLastFrame && value && g_prevSpecial && p->m_bButtonSpecial && p->m_bOldButtonSpecial)
        ++g_spuriousSpecial;
    p->m_bButtonSpecial = value;
    p->m_bOldButtonSpecial = value != previous;
    g_prevSpecial = value;
    g_specialFrame = g_frame;

    DWORD now = GetTickCount();
    if (g_spuriousSpecial && now - g_lastSpuriousLog > 2000) {
        log::Write("controls: %u spurious Special change flags from the decode while B was held", g_spuriousSpecial);
        g_spuriousSpecial = 0;
        g_lastSpuriousLog = now;
    }
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

void Toggle() {
    bool on = !settings::Get().enabled;
    settings::SetEnabled(on);
    ShowBrief(on ? L"Modern controls" : L"Classic controls");
    log::Write("controls: toggled %s", on ? "MODERN" : "CLASSIC");
}

// One-shot buttons, in both modern and classic mode, on the press edge:
//   Select + R3  toggle modern / classic
//   L3           repeat the last message (F7)
//   R3           show the district name (F9)
//   D-pad up / down in a car: next / previous radio station (F2 / F1)
// The function keys go through the game's own handler, ProcessKeyPresses
// (0x4A7010), which takes the DIK code and applies the native checks (radio
// only in civilian cars, message prompts, ...).
void HandleOneShots(CPlayerPed* player, const pad::State& pad, bool inCar) {
    bool l3 = pad.l3 && !g_prevPad.l3;
    bool r3 = pad.r3 && !g_prevPad.r3;
    bool up = pad.dpadUp && !g_prevPad.dpadUp;
    bool down = pad.dpadDown && !g_prevPad.dpadDown;
    g_prevPad = pad;

    if (r3 && pad.select)
        Toggle();
    else if (r3)
        player->ProcessKeyPresses(kDikF9);
    if (l3)
        player->ProcessKeyPresses(kDikF7);
    if (inCar && up)
        player->ProcessKeyPresses(kDikF2);
    if (inCar && down)
        player->ProcessKeyPresses(kDikF1);
}

// Fire truck, tank and gun jeep: the turret is a roof object of the car whose
// angle (tRoof+0x10, heading units) is relative to the car body; the game
// writes it the same way when a script spawns a tank (0x4AE17C), and shots
// use the turret sprite's resulting absolute angle.
struct Turret {
    int carModel;
    int objectModel;
    int angleOffset; // turret angle that points the same way as the car
};
const Turret kTurrets[] = {
    {0x11, 0x72, 720}, // FIRETRUK: water cannon faces backwards
    {0x36, 0x94, 0},   // TANK
    {0x16, 0xF8, 0},   // GUNJEEP
};
const uintptr_t kCarModel = 0x84;
const uintptr_t kCarSprite = 0x50;
const uintptr_t kRoofAngle = 0x10;

// Returns the car's turret angle field, or NULL if the car has no turret.
short* TurretAngle(CCar* car, int& angleOffset) {
    int model = 0;
    if (!mem::Read(reinterpret_cast<uintptr_t>(car) + kCarModel, model))
        return NULL;
    for (const Turret& t : kTurrets) {
        if (t.carModel != model)
            continue;
        // 0x4BE980: finds the car's roof node holding an object of that model.
        uintptr_t node = plugin::CallMethodAndReturn<uintptr_t, 0x4BE980, CCar*, int>(car, t.objectModel);
        if (!mem::IsReadable(node + kRoofAngle, sizeof(short)))
            return NULL;
        angleOffset = t.angleOffset;
        return reinterpret_cast<short*>(node + kRoofAngle);
    }
    return NULL;
}

// Right stick turns the turret. Default (like the original game's Special +
// left / right, but without losing the steering): stick left / right rotates
// it at a fixed speed. TurretAim=1: it points at the stick's direction on
// screen. Returns true if the car has a turret (the right stick is then not a
// camera look); `active` tells whether the stick is driving it this frame.
bool ApplyTurret(CCar* car, const pad::State& pad, const settings::Settings& cfg, bool& active) {
    active = false;
    int angleOffset = 0;
    short* angle = TurretAngle(car, angleOffset);
    if (!angle)
        return false;

    int rx = pad.rightX;
    int ry = pad.rightY;
    int current = NormalizeHeading(*angle);
    if (!cfg.turretAim) {
        if (rx > -cfg.aimDeadZone && rx < cfg.aimDeadZone)
            return true;
        // Left = counter-clockwise (+), as 0x423720 does for the left key.
        *angle = static_cast<short>(NormalizeHeading(current + (rx < 0 ? cfg.turretStep : -cfg.turretStep)));
        active = true;
        return true;
    }

    if (rx * rx + ry * ry <= cfg.aimDeadZone * cfg.aimDeadZone)
        return true; // released: the turret keeps its angle relative to the car

    uintptr_t sprite = 0;
    short carHeading = 0;
    if (!mem::Read(reinterpret_cast<uintptr_t>(car) + kCarSprite, sprite) || !mem::Read(sprite, carHeading))
        return true;

    int target = NormalizeHeading(HeadingFromStick(rx, ry) - carHeading - angleOffset);
    int delta = HeadingDelta(current, target);
    int step = delta > cfg.turretStep ? cfg.turretStep : (delta < -cfg.turretStep ? -cfg.turretStep : delta);
    *angle = static_cast<short>(NormalizeHeading(current + step));
    active = true;
    return true;
}

// Returns true if the right stick drives a turret.
bool ApplyVehicle(CPlayerPed* p, CCar* car, const pad::State& pad, const settings::Settings& cfg) {
    // Steering from the stick only (GInput also steers with the D-pad, which
    // now switches weapons); enter / exit stays as GInput set it.
    p->m_bButtonLeft = pad.leftX < 0;
    p->m_bButtonRight = pad.leftX > 0;
    SetButton(p->m_bButtonForward, p->m_bOldButtonForward, pad.r2 > cfg.triggerThreshold);
    SetButton(p->m_bButtonBackward, p->m_bOldButtonBackward, pad.l2 > cfg.triggerThreshold);
    SetButton(p->m_bButtonAttack, p->m_bOldButtonAttack, pad.square);
    SetButton(p->m_bButtonHandbrakeJump, p->m_bOldButtonHandbrakeJump, pad.cross);
    SetButton(p->m_bButtonPrevWeapon, p->m_bOldButtonPrevWeapon, pad.dpadLeft);
    SetButton(p->m_bButtonNextWeapon, p->m_bOldButtonNextWeapon, pad.dpadRight);

    bool turretActive = false;
    bool turret = ApplyTurret(car, pad, cfg, turretActive);
    // The original game turns the turret with Special + left / right, which
    // eats the steering: drop Special while the stick turns it and we steer.
    bool steering = p->m_bButtonLeft || p->m_bButtonRight;
    SetSpecial(p, pad.circle && !(turretActive && steering));
    return turret;
}

// Entering / leaving a car: the game drives the ped, but GInput's mapping
// would still fire on R2 and switch weapons on L2; keep the triggers inert.
void ApplyTransition(CPlayerPed* p, const pad::State& pad) {
    SetButton(p->m_bButtonAttack, p->m_bOldButtonAttack, false);
    SetButton(p->m_bButtonPrevWeapon, p->m_bOldButtonPrevWeapon, pad.dpadLeft);
    SetButton(p->m_bButtonNextWeapon, p->m_bOldButtonNextWeapon, pad.dpadRight);
}

// Right stick sets the secondary heading used for the sprite and shots;
// released, it follows the walking direction. Standing still, the player
// also turns to face the aim so that walking starts from there. While
// looking around (L2 held) the aim stays where it was.
void ApplyAim(CObject* obj, const pad::State& pad, bool moving, bool looking, const settings::Settings& cfg) {
    aim::SetTarget(obj);
    if (looking)
        return;
    int rx = pad.rightX;
    int ry = pad.rightY;
    bool aiming = rx * rx + ry * ry > cfg.aimDeadZone * cfg.aimDeadZone;
    int heading = NormalizeHeading(obj->m_nRotation);
    if (aiming) {
        heading = HeadingFromStick(rx, ry);
        if (!moving)
            obj->m_nRotation = static_cast<short>(heading);
    }
    obj->field_42 = static_cast<short>(heading);
}

void ApplyOnFoot(CPlayerPed* p, CPed* ped, const pad::State& pad, bool looking, const settings::Settings& cfg) {
    if (ped->m_nPedState == PEDSTATE_FALL || ped->m_nPedState == PEDSTATE_DEAD || ped->m_nPedFlags.bIsArrested) {
        aim::SetTarget(NULL);
        return;
    }
    CObject* obj = ped->m_pObject;
    if (!mem::IsReadable(obj, sizeof(CObject))) {
        aim::SetTarget(NULL);
        return;
    }

    int x = pad.leftX;
    int y = pad.leftY;

    bool walk = false;
    if (x != 0 || y != 0) {
        int current = NormalizeHeading(obj->m_nRotation);
        int delta = HeadingDelta(current, HeadingFromStick(x, y));
        int step = delta > cfg.turnStep ? cfg.turnStep : (delta < -cfg.turnStep ? -cfg.turnStep : delta);
        obj->m_nRotation = static_cast<short>(NormalizeHeading(current + step));
        walk = std::abs(delta - step) <= cfg.walkTolerance;
    }

    if (cfg.aim && aim::IsInstalled())
        ApplyAim(obj, pad, x != 0 || y != 0, looking, cfg);
    else
        aim::SetTarget(NULL);

    bool r2 = TriggerPressed(pad.r2, g_r2Latched, cfg.triggerThreshold);

    SetButton(p->m_bButtonForward, p->m_bOldButtonForward, walk);
    SetButton(p->m_bButtonBackward, p->m_bOldButtonBackward, false);
    p->m_bButtonLeft = false;
    p->m_bButtonRight = false;
    SetButton(p->m_bButtonAttack, p->m_bOldButtonAttack, r2);
    SetButton(p->m_bButtonHandbrakeJump, p->m_bOldButtonHandbrakeJump, pad.cross);
    SetButton(p->m_bButtonPrevWeapon, p->m_bOldButtonPrevWeapon, pad.dpadLeft);
    SetButton(p->m_bButtonNextWeapon, p->m_bOldButtonNextWeapon, pad.dpadRight);
    SetSpecial(p, pad.circle);
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
    g_context = CONTEXT_NONE;
    if (game->m_eGameStatus != GAME_RUN)
        return;
    ++g_frame;
    if (!mem::IsReadable(player, sizeof(CPlayerPed)))
        return;
    if (mem::IsReadable(gReplay, sizeof(CReplay)) && gReplay->IsPlayingBack())
        return;

    pad::State pad;
    if (!pad::Read(pad))
        return;

    CPed* ped = player->GetPed();
    bool pedOk = mem::IsReadable(ped, sizeof(CPed));
    HandleOneShots(player, pad, pedOk && ped->GetCar());
    const settings::Settings& cfg = settings::Get();
    if (cfg.aim)
        aim::Install();

    uint8_t controlsOff = 1;
    if (!cfg.enabled ||
        !mem::Read(reinterpret_cast<uintptr_t>(player) + kControlsOffOffset, controlsOff) || controlsOff ||
        !pedOk) {
        g_prevMode = MODE_NONE;
        aim::SetTarget(NULL);
        return;
    }

    Mode mode = ModeOf(ped);
    if (mode == MODE_FOOT && g_prevMode != MODE_FOOT)
        g_r2Latched = pad.r2 > cfg.triggerThreshold;
    g_prevMode = mode;

    if (mode != MODE_FOOT || !cfg.onFoot)
        aim::SetTarget(NULL);

    switch (mode) {
    case MODE_CAR:
        if (cfg.vehicle)
            g_context = ApplyVehicle(player, ped->GetCar(), pad, cfg) ? CONTEXT_IN_TURRET_CAR : CONTEXT_IN_CAR;
        break;
    case MODE_TRANSITION:
        if (cfg.vehicle || cfg.onFoot)
            ApplyTransition(player, pad);
        break;
    case MODE_FOOT:
        if (cfg.onFoot) {
            // L2 held: the right stick looks around instead of aiming.
            bool looking = pad.l2 > cfg.triggerThreshold;
            ApplyOnFoot(player, ped, pad, looking, cfg);
            g_context = looking ? CONTEXT_ON_FOOT_LOOKING : CONTEXT_ON_FOOT;
        }
        break;
    default:
        break;
    }
}

Context CurrentContext() {
    return g_context;
}

} // namespace controls
} // namespace gmc
