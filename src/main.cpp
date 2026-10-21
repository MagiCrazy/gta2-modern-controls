// GTA2ModernControls - Phase 1 probe entry point.
//
// plugin-sdk convention: a global object whose constructor registers the
// hooks. The constructor runs inside DllMain, so it only reads memory and
// opens the log; LoadLibrary (XInput) is deferred to the first hook call.
#include "plugin.h"
#include "CPlayerPed.h"

#include "probe.h"

namespace {

// Same call site as Frontend Fix: runs after the game processed the frame's
// keys, once per CPlayerPed. injector chains with any hook already installed
// at this CALL (ASI load order is alphabetical, so Frontend Fix comes first).
plugin::ThiscallEvent<plugin::AddressList<0x4A7683, plugin::H_CALL>, plugin::PRIORITY_AFTER,
                      plugin::ArgPickN<CPlayerPed*, 0>, void(CPlayerPed*)>
    onProcessPlayerKeys;

struct GTA2ModernControls {
    GTA2ModernControls() {
        gmc::probe::OnLoad();

        onProcessPlayerKeys += [](CPlayerPed* player) { gmc::probe::OnProcessPlayerKeys(player); };
        gmc::probe::OnHookInstalled();

        plugin::Events::shutdownEngineEvent += []() { gmc::probe::OnShutdown(); };
    }
} gInstance;

} // namespace
