// Settings read from GTA2ModernControls.ini next to the .asi. The file is
// created with defaults on first run.
#pragma once

namespace gmc {
namespace settings {

struct Settings {
    bool enabled;          // modern controls on (toggled in game, persisted)
    bool vehicle;          // R2 / L2 driving
    bool onFoot;           // absolute on-foot movement
    int triggerThreshold;  // 0..255: R2 / L2 above this count as pressed
    int turnStep;          // on foot: max heading change per frame (degrees x 4)
    int walkTolerance;     // on foot: walk only once facing within this of the target (degrees x 4)
    bool probeLog;         // write GTA2ModernControls.log (diagnostics)
};

// Safe to call from DllMain (kernel32 only).
void Load();
const Settings& Get();

// Runtime toggle; also written back to the ini.
void SetEnabled(bool on);

} // namespace settings
} // namespace gmc
