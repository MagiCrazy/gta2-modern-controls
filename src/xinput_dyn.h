// XInput loaded at runtime (no import library): xinput1_4 -> xinput1_3 -> xinput9_1_0.
#pragma once

#include <cstdint>

namespace gmc {
namespace xinput {

// Same layout as XINPUT_GAMEPAD / XINPUT_STATE, redeclared to avoid
// depending on <xinput.h> differences between SDKs.
struct Gamepad {
    uint16_t buttons;
    uint8_t leftTrigger;
    uint8_t rightTrigger;
    int16_t thumbLX;
    int16_t thumbLY;
    int16_t thumbRX;
    int16_t thumbRY;
};

struct State {
    uint32_t packetNumber;
    Gamepad gamepad;
};
static_assert(sizeof(State) == 16, "XINPUT_STATE is 16 bytes");

// XINPUT_GAMEPAD_* button bits.
enum : uint16_t {
    BTN_DPAD_UP = 0x0001,
    BTN_DPAD_DOWN = 0x0002,
    BTN_DPAD_LEFT = 0x0004,
    BTN_DPAD_RIGHT = 0x0008,
    BTN_START = 0x0010,
    BTN_BACK = 0x0020,
    BTN_LEFT_THUMB = 0x0040,
    BTN_RIGHT_THUMB = 0x0080,
    BTN_LEFT_SHOULDER = 0x0100,
    BTN_RIGHT_SHOULDER = 0x0200,
    BTN_A = 0x1000,
    BTN_B = 0x2000,
    BTN_X = 0x4000,
    BTN_Y = 0x8000,
};

// Loads the DLL (must NOT be called from DllMain). Safe to call repeatedly.
bool Load();
const char* LoadedDllName();

// Polls the active slot. Scans slots 0..3 at most every 2 s while nothing is
// connected (polling disconnected slots is slow on Windows).
// Returns true and fills state when a controller is connected.
bool Poll(State& state, int& slot);

} // namespace xinput
} // namespace gmc
