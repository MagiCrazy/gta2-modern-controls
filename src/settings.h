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
    bool turretAim;        // turret vehicles: false = right stick X rotates the turret, true = it points at the stick
    int turretStep;        // turret vehicles: turret turn per frame (degrees x 4)
    int turnStep;          // on foot: max heading change per frame (degrees x 4)
    int walkTolerance;     // on foot: walk only once facing within this of the target (degrees x 4)
    bool aim;              // right stick aims independently of the walking direction
    int aimDeadZone;       // 0..255: right stick deflection below this does not aim (nor look)
    bool look;             // right stick looks around (in a car; on foot while L2 is held)
    int lookCarRange;      // % of the game's max look offset, in a car
    int lookFootRange;     // % of the game's max look offset, on foot (L2 held)
    bool probeLog;         // write GTA2ModernControls.log (diagnostics)
};

// Safe to call from DllMain (kernel32 only).
void Load();
const Settings& Get();

// Runtime toggle; also written back to the ini.
void SetEnabled(bool on);

} // namespace settings
} // namespace gmc
