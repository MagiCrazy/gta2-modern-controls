// Twin-stick aiming: port of the secondary heading from Dege's Twin Stick mod
// (GTAMP, public domain). For one object (the player's), the sprite and the
// shooting direction read CObject::field_42 instead of m_nRotation, so the
// player can aim one way and walk another.
#pragma once

class CObject;

namespace gmc {
namespace aim {

// Installs the code patches once; call from the game thread. Returns false
// (and patches nothing) if the game code is not the expected 9.6 code.
bool Install();
bool IsInstalled();

// Object whose sprite / shots use field_42; NULL disables the redirection
// (every object uses m_nRotation, as in the original game).
void SetTarget(CObject* obj);

} // namespace aim
} // namespace gmc
