// Phase 1 probe: read-only instrumentation of GTA 2 9.6 input/heading state.
// Nothing in here writes game memory or changes game behaviour.
#pragma once

class CPlayerPed;

namespace gmc {
namespace probe {

// Called from the plugin constructor (inside DllMain): opens the log and
// records module info and the code-patch-site dumps. Only reads memory.
void OnLoad();

// Logs the bytes at the 0x4A7683 call site once our hook is registered.
void OnHookInstalled();

// Per-frame callback, after the game processed the player's keys.
void OnProcessPlayerKeys(CPlayerPed* player);

void OnShutdown();

} // namespace probe
} // namespace gmc
