// Camera look on the right stick. The game's own look (Special 2, Left Alt +
// arrows by default) pans the camera target by an offset stored in the
// active CCamera (+0x48 X, +0x4C Y, Fix16) and freezes the player while held.
// We write that offset ourselves, after the game's look / reset step, so the
// look is analog and works while driving or walking:
//   in a car : right stick = look in that direction;
//   on foot  : the camera leads towards the aim (right stick), shorter range.
#pragma once

namespace gmc {
namespace look {

// Hooks the two calls of the game's look step in CPlayerPed::Process, after
// checking that they are the expected 9.6 code. Safe to call from DllMain.
bool Install();

} // namespace look
} // namespace gmc
