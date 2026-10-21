#include "pad.h"

#include "ginput.h"
#include "xinput_dyn.h"

#include <windows.h>
#include <cstdlib>
#include <cstring>

namespace gmc {
namespace pad {

namespace {

const DWORD kGInputRetryMs = 5000;
// Matches GInput's default LeftStickDeadZoneP1 (24 %), for the XInput fallback.
const int kXInputDeadZone = 61; // of 255

DWORD g_lastGInputTry = 0;
bool g_xinputLoaded = false;

int ScaleThumb(int16_t v) {
    int s = v / 128; // -256..255
    if (s < -255)
        s = -255;
    return std::abs(s) < kXInputDeadZone ? 0 : s;
}

// GInput reports sticks in -255..255 with its dead zone applied and Y
// positive up (same sign as XInput, observed in the phase 1 probe log);
// buttons are 0 / 255, L2 / R2 are analog 0..255.
void FromGInput(const ginput::ControllerState& s, State& out) {
    out.leftX = s.leftStickX;
    out.leftY = s.leftStickY;
    out.rightX = s.rightStickX;
    out.rightY = s.rightStickY;
    out.l2 = s.leftShoulder2;
    out.r2 = s.rightShoulder2;
    out.cross = s.cross != 0;
    out.square = s.square != 0;
    out.triangle = s.triangle != 0;
    out.circle = s.circle != 0;
    out.l1 = s.leftShoulder1 != 0;
    out.r1 = s.rightShoulder1 != 0;
    out.l3 = s.leftShock != 0;
    out.r3 = s.rightShock != 0;
    out.dpadUp = s.dpadUp != 0;
    out.dpadDown = s.dpadDown != 0;
    out.dpadLeft = s.dpadLeft != 0;
    out.dpadRight = s.dpadRight != 0;
    out.start = s.start != 0;
    out.select = s.select != 0;
}

void FromXInput(const xinput::Gamepad& g, State& out) {
    out.leftX = ScaleThumb(g.thumbLX);
    out.leftY = ScaleThumb(g.thumbLY);
    out.rightX = ScaleThumb(g.thumbRX);
    out.rightY = ScaleThumb(g.thumbRY);
    out.l2 = g.leftTrigger;
    out.r2 = g.rightTrigger;
    out.cross = (g.buttons & xinput::BTN_A) != 0;
    out.square = (g.buttons & xinput::BTN_X) != 0;
    out.triangle = (g.buttons & xinput::BTN_Y) != 0;
    out.circle = (g.buttons & xinput::BTN_B) != 0;
    out.l1 = (g.buttons & xinput::BTN_LEFT_SHOULDER) != 0;
    out.r1 = (g.buttons & xinput::BTN_RIGHT_SHOULDER) != 0;
    out.l3 = (g.buttons & xinput::BTN_LEFT_THUMB) != 0;
    out.r3 = (g.buttons & xinput::BTN_RIGHT_THUMB) != 0;
    out.dpadUp = (g.buttons & xinput::BTN_DPAD_UP) != 0;
    out.dpadDown = (g.buttons & xinput::BTN_DPAD_DOWN) != 0;
    out.dpadLeft = (g.buttons & xinput::BTN_DPAD_LEFT) != 0;
    out.dpadRight = (g.buttons & xinput::BTN_DPAD_RIGHT) != 0;
    out.start = (g.buttons & xinput::BTN_START) != 0;
    out.select = (g.buttons & xinput::BTN_BACK) != 0;
}

} // namespace

bool Read(State& out) {
    std::memset(&out, 0, sizeof(out));

    DWORD now = GetTickCount();
    if (!ginput::HasGetPad() && (g_lastGInputTry == 0 || now - g_lastGInputTry > kGInputRetryMs)) {
        g_lastGInputTry = now;
        ginput::Resolve();
    }

    ginput::Snapshot gi;
    if (ginput::Sample(gi) && (gi.connectedCall || gi.connectedField)) {
        FromGInput(gi.state, out);
        return true;
    }

    if (!g_xinputLoaded) {
        g_xinputLoaded = true;
        xinput::Load();
    }
    xinput::State xs;
    int slot = -1;
    if (!xinput::Poll(xs, slot))
        return false;
    FromXInput(xs.gamepad, out);
    return true;
}

} // namespace pad
} // namespace gmc
