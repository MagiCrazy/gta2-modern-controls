// GTA2ModernControls entry point.
//
// plugin-sdk convention: a global object whose constructor registers the
// hooks. The constructor runs inside DllMain, so it only reads settings and
// memory and opens the log; LoadLibrary (XInput) is deferred to the first
// hook call.
#include "plugin.h"
#include "CPlayerPed.h"

#include "controls.h"
#include "look.h"
#include "probe.h"
#include "settings.h"

namespace {

// Runs after GInput processed the frame's keys (it owns the CALL at
// 0x4A7683, Frontend Fix does not), once per CPlayerPed: the place to
// override the control state before the game consumes it.
plugin::ThiscallEvent<plugin::AddressList<0x4A7683, plugin::H_CALL>, plugin::PRIORITY_AFTER,
                      plugin::ArgPickN<CPlayerPed*, 0>, void(CPlayerPed*)>
    onProcessPlayerKeys;

struct GTA2ModernControls {
    GTA2ModernControls() {
        gmc::settings::Load();
        const bool probe = gmc::settings::Get().probeLog;
        if (probe)
            gmc::probe::OnLoad();

        onProcessPlayerKeys += [probe](CPlayerPed* player) {
            gmc::controls::Apply(player);
            // Logged after the override, so pb= shows what the game consumes.
            if (probe)
                gmc::probe::OnProcessPlayerKeys(player);
        };
        if (probe)
            gmc::probe::OnHookInstalled();

        if (gmc::settings::Get().look)
            gmc::look::Install();

        plugin::Events::shutdownEngineEvent += []() { gmc::probe::OnShutdown(); };
    }
} gInstance;

} // namespace
