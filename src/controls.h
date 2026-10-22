// Modern controls: rewrites the player's control state for the frame, after
// GInput (or the game) filled it, from the raw pad state.
//
//   On foot : left stick / D-pad = absolute direction, right stick = aim
//             (sprite and shots, see aim.h), R2 attack, A jump,
//             L1 / R1 previous / next weapon.
//   In car  : R2 accelerate, L2 brake / reverse, stick X / D-pad steer,
//             A handbrake, X attack, L1 / R1 weapons.
//   Y (enter / exit) and B (special) keep GInput's mapping. Special 2 (the
//   game's camera-look modifier) is left alone: look.h drives the camera.
//   R2 / L2 do nothing while entering / leaving a car, and a trigger held
//   when leaving must be released before it acts on foot.
//   Select + R3 toggles modern / classic controls.
#pragma once

class CPlayerPed;

namespace gmc {
namespace controls {

// Per-frame callback, after the game processed the player's keys.
void Apply(CPlayerPed* player);

enum Context { CONTEXT_NONE, CONTEXT_ON_FOOT, CONTEXT_IN_CAR };

// What the last Apply() drove for the current player this frame; NONE when
// modern controls are off, paused, replaying, in a cutscene or a transition.
Context CurrentContext();

} // namespace controls
} // namespace gmc
