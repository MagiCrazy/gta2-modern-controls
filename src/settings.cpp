#include "settings.h"

#include "gameutil.h"

#include <windows.h>
#include <cstdio>

namespace gmc {
namespace settings {

namespace {

const char kDefaultIni[] =
    "; GTA2 Modern Controls\r\n"
    "[General]\r\n"
    "; 1 = modern controls, 0 = classic. Toggle in game with Select + R3.\r\n"
    "Enabled=1\r\n"
    "; Write GTA2ModernControls.log (diagnostics).\r\n"
    "ProbeLog=1\r\n"
    "\r\n"
    "[Vehicle]\r\n"
    "; R2 accelerates, L2 brakes / reverses.\r\n"
    "Enabled=1\r\n"
    "; Trigger travel (0-255) above which R2 / L2 count as pressed.\r\n"
    "TriggerThreshold=40\r\n"
    "; Tank, fire truck, gun jeep: the right stick aims the turret. Max turn per frame,\r\n"
    "; in degrees x 4 (40 = 10 degrees, twice the game's own speed).\r\n"
    "TurretStep=40\r\n"
    "\r\n"
    "[OnFoot]\r\n"
    "; Left stick / D-pad give an absolute direction on screen.\r\n"
    "Enabled=1\r\n"
    "; Max turn per frame, in degrees x 4 (96 = 24 degrees, about 2 turns per second).\r\n"
    "TurnStep=96\r\n"
    "; Start walking once facing within this angle of the stick, in degrees x 4 (180 = 45 degrees).\r\n"
    "WalkTolerance=180\r\n"
    "\r\n"
    "[Aim]\r\n"
    "; Right stick aims (sprite and shots) independently of the walking direction; R2 fires.\r\n"
    "Enabled=1\r\n"
    "; Right stick deflection (0-255) below which it does not aim.\r\n"
    "DeadZone=80\r\n"
    "\r\n"
    "[Look]\r\n"
    "; Right stick looks around in a car (except turret vehicles), and on foot while L2 is held.\r\n"
    "Enabled=1\r\n"
    "; Look distance, in % of the game's own look (Special 2 / Left Alt + arrows).\r\n"
    "CarRange=100\r\n"
    "OnFootRange=100\r\n";

Settings g_settings = {true, true, true, 40, 40, 96, 180, true, 80, true, 100, 100, true};
char g_path[MAX_PATH + 32] = {};

int Clamp(int v, int lo, int hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

void WriteDefaultsIfMissing() {
    HANDLE h = CreateFileA(g_path, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return; // already exists (or not writable): keep it untouched
    DWORD written = 0;
    WriteFile(h, kDefaultIni, sizeof(kDefaultIni) - 1, &written, NULL);
    CloseHandle(h);
}

int ReadInt(const char* section, const char* key, int def) {
    return static_cast<int>(GetPrivateProfileIntA(section, key, def, g_path));
}

} // namespace

void Load() {
    char dir[MAX_PATH] = {};
    SelfDirectory(dir, sizeof(dir));
    snprintf(g_path, sizeof(g_path), "%sGTA2ModernControls.ini", dir);
    WriteDefaultsIfMissing();

    Settings& s = g_settings;
    s.enabled = ReadInt("General", "Enabled", 1) != 0;
    s.probeLog = ReadInt("General", "ProbeLog", 1) != 0;
    s.vehicle = ReadInt("Vehicle", "Enabled", 1) != 0;
    s.triggerThreshold = Clamp(ReadInt("Vehicle", "TriggerThreshold", 40), 1, 254);
    s.turretStep = Clamp(ReadInt("Vehicle", "TurretStep", 40), 4, 720);
    s.onFoot = ReadInt("OnFoot", "Enabled", 1) != 0;
    s.turnStep = Clamp(ReadInt("OnFoot", "TurnStep", 96), 4, 720);
    s.walkTolerance = Clamp(ReadInt("OnFoot", "WalkTolerance", 180), 0, 720);
    s.aim = ReadInt("Aim", "Enabled", 1) != 0;
    s.aimDeadZone = Clamp(ReadInt("Aim", "DeadZone", 80), 1, 254);
    s.look = ReadInt("Look", "Enabled", 1) != 0;
    s.lookCarRange = Clamp(ReadInt("Look", "CarRange", 100), 0, 200);
    s.lookFootRange = Clamp(ReadInt("Look", "OnFootRange", 100), 0, 200);
}

const Settings& Get() {
    return g_settings;
}

void SetEnabled(bool on) {
    g_settings.enabled = on;
    WritePrivateProfileStringA("General", "Enabled", on ? "1" : "0", g_path);
}

} // namespace settings
} // namespace gmc
