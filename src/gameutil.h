// Small helpers shared by the probe and the controls.
#pragma once

#include <cstddef>

class CGame;

namespace gmc {

// *gGame when it points to readable memory, NULL otherwise.
CGame* SafeGame();

// Directory of this .asi with a trailing separator; falls back to the game
// directory. Empty string if both fail.
void SelfDirectory(char* out, size_t size);

} // namespace gmc
