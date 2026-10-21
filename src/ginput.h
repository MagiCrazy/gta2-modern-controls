// Minimal read-only access to GInput (GInputII.asi) for GTA 2.
//
// GInput exports its API twice: with MSVC-mangled names and with plain
// "Class::Method" names (verified by reading the export table of the
// GInputII.asi shipped in gta2-deck). We resolve the plain names first and
// fall back to the mangled ones, so nothing depends on __FUNCTION__.
#pragma once

#include <cstddef>
#include <cstdint>

namespace gmc {
namespace ginput {

// Layout of GInput's controller state: 20 consecutive shorts.
struct ControllerState {
    int16_t leftStickX;
    int16_t leftStickY;
    int16_t rightStickX;
    int16_t rightStickY;
    int16_t leftShoulder1;
    int16_t leftShoulder2;
    int16_t rightShoulder1;
    int16_t rightShoulder2;
    int16_t dpadUp;
    int16_t dpadDown;
    int16_t dpadLeft;
    int16_t dpadRight;
    int16_t start;
    int16_t select;
    int16_t square;
    int16_t triangle;
    int16_t cross;
    int16_t circle;
    int16_t leftShock;
    int16_t rightShock;
};
static_assert(sizeof(ControllerState) == 40, "GInput controller state must be 40 bytes");

// Leading part of GInput's pad object; only what the probe reads.
struct PadPrefix {
    ControllerState newState;
    ControllerState oldState;
    ControllerState tempState;
    int32_t index;
    uint8_t disableControls;
    int8_t mode;
    uint8_t padConnected;
};
static_assert(offsetof(PadPrefix, oldState) == 40, "PadPrefix layout");
static_assert(offsetof(PadPrefix, index) == 120, "PadPrefix layout");
static_assert(offsetof(PadPrefix, padConnected) == 126, "PadPrefix layout");

struct Snapshot {
    bool valid;            // GetPad(0) returned a readable pointer
    bool connectedCall;    // result of CPad::IsConnected (false if not resolved)
    bool connectedField;   // raw padConnected byte
    uintptr_t padAddr;
    ControllerState state; // NewState
};

// Looks up GInputII.asi and its exports. Cheap to call again (re-resolves only
// if the module was not found before). Returns true if GetPad resolved.
bool Resolve();
bool IsModuleLoaded();
bool HasGetPad();

// Logs the module handle and which export names resolved.
void LogResolution();

// Reads pad 0 without writing anything. Returns snapshot.valid.
bool Sample(Snapshot& out);

} // namespace ginput
} // namespace gmc
