// Modern controls: rewrites the player's control state for the frame, after
// GInput (or the game) filled it, from the raw pad state.
//
//   On foot : left stick / D-pad = absolute direction, R2 attack, A jump,
//             L1 / R1 previous / next weapon, L2 / L3 special 2.
//   In car  : R2 accelerate, L2 brake / reverse, stick X / D-pad steer,
//             A handbrake, X attack, L1 / R1 weapons, L3 special 2.
//   Y (enter / exit) and B (special) keep GInput's mapping.
//   R2 / L2 do nothing while entering / leaving a car, and a trigger held
//   when leaving must be released before it acts on foot.
//   Select + R3 toggles modern / classic controls.
#pragma once

class CPlayerPed;

namespace gmc {
namespace controls {

// Per-frame callback, after the game processed the player's keys.
void Apply(CPlayerPed* player);

} // namespace controls
} // namespace gmc
