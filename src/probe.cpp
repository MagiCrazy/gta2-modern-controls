#include "plugin.h"
#include "CGame.h"
#include "CObject.h"
#include "CPed.h"
#include "CPlayerPed.h"
#include "CReplay.h"

#include "probe.h"

#include "gameutil.h"
#include "ginput.h"
#include "log.h"
#include "memutil.h"
#include "xinput_dyn.h"

#include <windows.h>
#include <cstdio>
#include <cstring>

namespace gmc {
namespace probe {

namespace {

// ---------------------------------------------------------------------------
// Addresses read by the probe (all GTA 2 9.6 virtual addresses).
// ---------------------------------------------------------------------------
const uintptr_t kInCarFlagAddr = 0x5E20BC;   // Dege: in-car byte
const uintptr_t kInputBitsAddr = 0x5ECACC;   // Dege: input bitfield (= gReplay + 4)
const uintptr_t kHookSiteAddr = 0x4A7683;    // CALL hooked by us and by Frontend Fix
const uintptr_t kExpectedBase96 = 0x400000;  // 9.6 image base
const uintptr_t kBase1144 = 0x3F0000;        // 11.44 image base (MISI.cpp)
const DWORD kSize1144 = 1740800;             // 11.44 SizeOfImage (MISI.cpp)

// Offsets of Dege's analog walk-speed chain: m_pObject + 0x80 -> +0x8 -> +0x38.
const uintptr_t kSpeedHop1 = 0x80;
const uintptr_t kSpeedHop2 = 0x08;
const uintptr_t kSpeedHop3 = 0x38;

const DWORD kLogIntervalMs = 100; // ~10 Hz

// Dege's code-patch sites (EnablePedSecondaryAngle). `patchLen` is how many
// bytes he overwrites there; `expected` is the original code his
// DisablePedSecondaryAngle restores (only known for two sites).
struct PatchSite {
    uintptr_t addr;
    size_t patchLen;
    const uint8_t* expected;
    size_t expectedLen;
    const char* what;
};

const uint8_t kOrig49C5DC[] = {0x66, 0x8B, 0x4E, 0x40, 0x8D, 0x7E, 0x40}; // mov cx,[esi+40h]; lea edi,[esi+40h]
const uint8_t kOrig433A44[] = {0x66, 0x8B, 0x49, 0x40};                   // mov cx,[ecx+40h]

const PatchSite kPatchSites[] = {
    {0x49C5DC, 7, kOrig49C5DC, sizeof(kOrig49C5DC), "sprite angle read (jmp to cave)"},
    {0x49C6D4, 12, NULL, 0, "sprite cave 1 (relocated original + jmp back)"},
    {0x49C6F1, 10, NULL, 0, "sprite cave 2 (cmp esi, ped; je/jmp)"},
    {0x49C731, 12, NULL, 0, "sprite cave 3 (secondary angle +42h)"},
    {0x433A44, 4, kOrig433A44, sizeof(kOrig433A44), "shoot direction read (jmp to cave)"},
    {0x433A5A, 6, NULL, 0, "shoot cave 1 (relocated original)"},
    {0x433A6A, 6, NULL, 0, "shoot cave 2 (secondary angle +42h)"},
    {0x433A84, 10, NULL, 0, "shoot cave 3 (cmp ecx, ped; je/jmp)"},
};

const char* const kControlKeyNames = "F B L R A E H p n 1 2 U"; // legend only

char g_anchor = 0; // any address inside our module, for GetModuleHandleEx

// ---------------------------------------------------------------------------
// Per-frame state
// ---------------------------------------------------------------------------
struct KeyState {
    uintptr_t ped;
    unsigned int pedState;
    int inCarByte;     // -1 when unreadable
    bool hasCar;       // GetCar() != NULL
    bool giConnected;
    int xiSlot;        // -1 when no XInput controller
    bool replay;
    int gameStatus;
};

bool g_lateInitDone = false;
DWORD g_lastGInputRetry = 0;
unsigned int g_frame = 0;
DWORD g_lastLogTick = 0;
bool g_havePrev = false;
KeyState g_prev;
bool g_prevL3 = false;
unsigned int g_markerCount = 0;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
unsigned int U32(uintptr_t v) {
    return static_cast<unsigned int>(v);
}

unsigned int U32(const void* p) {
    return static_cast<unsigned int>(reinterpret_cast<uintptr_t>(p));
}

bool ModulePath(HMODULE mod, char* out, DWORD size) {
    DWORD n = GetModuleFileNameA(mod, out, size);
    if (n == 0 || n >= size) {
        out[0] = '\0';
        return false;
    }
    return true;
}

void StripFileName(char* path) {
    char* slash = NULL;
    for (char* c = path; *c; ++c) {
        if (*c == '\\' || *c == '/')
            slash = c;
    }
    if (slash)
        slash[1] = '\0';
    else
        path[0] = '\0';
}

HMODULE SelfModule() {
    HMODULE self = NULL;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            &g_anchor, &self))
        return NULL;
    return self;
}

void OpenLog() {
    char dir[MAX_PATH] = {};
    HMODULE self = SelfModule();
    if (!self || !ModulePath(self, dir, MAX_PATH)) {
        // Fallback: game directory.
        ModulePath(NULL, dir, MAX_PATH);
    }
    StripFileName(dir);

    char path[MAX_PATH + 32];
    snprintf(path, sizeof(path), "%sGTA2ModernControls.log", dir);
    if (!log::Open(path))
        log::Open("GTA2ModernControls.log"); // last resort: current directory
}

// Name of the module that contains addr, or "?".
void ModuleNameAt(uintptr_t addr, char* out, DWORD size) {
    HMODULE mod = NULL;
    out[0] = '?';
    out[1] = '\0';
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCSTR>(addr), &mod) ||
        !mod)
        return;
    char full[MAX_PATH];
    if (!ModulePath(mod, full, MAX_PATH))
        return;
    const char* base = full;
    for (const char* c = full; *c; ++c) {
        if (*c == '\\' || *c == '/')
            base = c + 1;
    }
    lstrcpynA(out, base, static_cast<int>(size));
}

void LogExeInfo() {
    HMODULE exe = GetModuleHandleA(NULL);
    uintptr_t base = reinterpret_cast<uintptr_t>(exe);
    char path[MAX_PATH];
    ModulePath(NULL, path, MAX_PATH);
    log::Write("exe: %s", path);

    IMAGE_DOS_HEADER dos;
    if (!mem::Read(base, dos) || dos.e_magic != IMAGE_DOS_SIGNATURE) {
        log::Write("exe: base 0x%08X, DOS header unreadable", U32(base));
        return;
    }
    IMAGE_NT_HEADERS32 nt;
    if (!mem::Read(base + static_cast<uintptr_t>(dos.e_lfanew), nt) || nt.Signature != IMAGE_NT_SIGNATURE) {
        log::Write("exe: base 0x%08X, NT header unreadable", U32(base));
        return;
    }
    DWORD size = nt.OptionalHeader.SizeOfImage;
    log::Write("exe: base 0x%08X (preferred 0x%08X) SizeOfImage %lu (0x%08lX) end 0x%08X TimeDateStamp 0x%08lX "
               "EntryPoint RVA 0x%08lX",
               U32(base), U32(static_cast<uintptr_t>(nt.OptionalHeader.ImageBase)), static_cast<unsigned long>(size),
               static_cast<unsigned long>(size), U32(base + size),
               static_cast<unsigned long>(nt.FileHeader.TimeDateStamp),
               static_cast<unsigned long>(nt.OptionalHeader.AddressOfEntryPoint));
    log::Write("exe: base %s 9.6 expectation 0x%08X; %s 11.44 signature (base 0x%08X, size %lu)",
               base == kExpectedBase96 ? "MATCHES" : "DIFFERS FROM", U32(kExpectedBase96),
               (base == kBase1144 && size == kSize1144) ? "MATCHES" : "does not match", U32(kBase1144),
               static_cast<unsigned long>(kSize1144));
}

void LogModule(const char* name) {
    HMODULE mod = GetModuleHandleA(name);
    char path[MAX_PATH] = "";
    if (mod)
        ModulePath(mod, path, MAX_PATH);
    log::Write("module: %-20s %s 0x%08X %s", name, mod ? "LOADED" : "absent", U32(mod), path);
}

void DumpBytes(const char* label, uintptr_t addr, size_t len) {
    uint8_t buf[64];
    if (len > sizeof(buf))
        len = sizeof(buf);
    char hex[64 * 3 + 1];
    if (!mem::ReadBytes(addr, buf, len)) {
        log::Write("%s 0x%08X: UNREADABLE", label, U32(addr));
        return;
    }
    mem::FormatHex(buf, len, hex, sizeof(hex));
    log::Write("%s 0x%08X: %s", label, U32(addr), hex);
}

// Classifies bytes when no reference is known (code caves).
const char* ClassifySpan(const uint8_t* b, size_t n) {
    bool allCC = true, all90 = true, all00 = true;
    for (size_t i = 0; i < n; ++i) {
        allCC = allCC && b[i] == 0xCC;
        all90 = all90 && b[i] == 0x90;
        all00 = all00 && b[i] == 0x00;
    }
    if (allCC)
        return "span is all CC (int3 padding)";
    if (all90)
        return "span is all 90 (nop padding)";
    if (all00)
        return "span is all 00";
    return "span holds non-padding bytes";
}

void DumpPatchSites() {
    log::Write("--- Dege code-patch sites (11.44 addresses, checked on this exe) ---");
    int refMatch = 0, refTotal = 0;
    for (size_t i = 0; i < sizeof(kPatchSites) / sizeof(kPatchSites[0]); ++i) {
        const PatchSite& s = kPatchSites[i];
        uint8_t buf[16];
        char hex[16 * 3 + 1];
        if (!mem::ReadBytes(s.addr, buf, sizeof(buf))) {
            log::Write("patch 0x%08X: UNREADABLE (%s)", U32(s.addr), s.what);
            if (s.expected)
                ++refTotal;
            continue;
        }
        mem::FormatHex(buf, sizeof(buf), hex, sizeof(hex));
        log::Write("patch 0x%08X [%2u bytes patched]: %s  (%s)", U32(s.addr), static_cast<unsigned int>(s.patchLen),
                   hex, s.what);
        if (s.expected) {
            ++refTotal;
            bool match = std::memcmp(buf, s.expected, s.expectedLen) == 0;
            char exp[16 * 3 + 1];
            mem::FormatHex(s.expected, s.expectedLen, exp, sizeof(exp));
            log::Write("patch 0x%08X: %s (expected %s)", U32(s.addr), match ? "MATCH" : "MISMATCH", exp);
            if (match)
                ++refMatch;
        } else {
            log::Write("patch 0x%08X: NO-REF, %s", U32(s.addr), ClassifySpan(buf, s.patchLen));
        }
    }
    log::Write("patch summary: %d/%d reference sites MATCH -> address hypothesis %s", refMatch, refTotal,
               (refTotal > 0 && refMatch == refTotal) ? "SUPPORTED" : "NOT SUPPORTED");
}

void DumpHookSite(const char* when) {
    uint8_t buf[5];
    if (!mem::ReadBytes(kHookSiteAddr, buf, sizeof(buf))) {
        log::Write("hook site 0x%08X (%s): UNREADABLE", U32(kHookSiteAddr), when);
        return;
    }
    char hex[5 * 3 + 1];
    mem::FormatHex(buf, sizeof(buf), hex, sizeof(hex));
    if (buf[0] != 0xE8) {
        log::Write("hook site 0x%08X (%s): %s (not an E8 CALL)", U32(kHookSiteAddr), when, hex);
        return;
    }
    int32_t rel;
    std::memcpy(&rel, buf + 1, sizeof(rel));
    uintptr_t target = kHookSiteAddr + 5 + static_cast<uintptr_t>(static_cast<intptr_t>(rel));
    char mod[MAX_PATH];
    ModuleNameAt(target, mod, MAX_PATH);
    log::Write("hook site 0x%08X (%s): %s -> CALL 0x%08X in %s", U32(kHookSiteAddr), when, hex, U32(target), mod);
}

void LateInit() {
    g_lateInitDone = true;
    log::Write("--- late init (first hook call, game thread) ---");
    if (xinput::Load())
        log::Write("XInput: loaded %s", xinput::LoadedDllName());
    else
        log::Write("XInput: no DLL could be loaded, raw XInput column disabled");
    ginput::Resolve();
    ginput::LogResolution();
    log::Write("legend: t=tick ms f=frame [why] ped=GetPed() raw=m_pPed st=m_nPedState car=GetCar()!=0(m_pCurrentCar) "
               "ic=byte@5E20BC rot=m_nRotation(deg=raw/4) rot2=obj+42 ctl=bytes@5ECACC,5ECACD "
               "rp=gReplay->GetButton(0..11) pb=m_bButton*(0..10) [order %s] "
               "sp=special,changed [hn=car+A7 horn sr=car+A4 siren] spd=obj+80>+8>+38 (value, /256) gs=m_eGameStatus rep=IsPlayingBack "
               "gi=GInput NewState (c=IsConnected/field, L/R sticks, L1 L2 R1 R2, D=up,down,left,right, "
               "face=sq,tr,cr,ci, L3 R3 St Se) xi=XInput (slot, L/R sticks, LT RT, buttons hex)",
               kControlKeyNames);
}

void FormatGInput(const ginput::Snapshot& gi, char* out, size_t size) {
    if (!gi.valid) {
        snprintf(out, size, "-");
        return;
    }
    const ginput::ControllerState& s = gi.state;
    snprintf(out, size,
             "c%d%d L%d,%d R%d,%d L1=%d L2=%d R1=%d R2=%d D=%d,%d,%d,%d face=%d,%d,%d,%d L3=%d R3=%d St=%d Se=%d",
             gi.connectedCall ? 1 : 0, gi.connectedField ? 1 : 0, s.leftStickX, s.leftStickY, s.rightStickX,
             s.rightStickY, s.leftShoulder1, s.leftShoulder2, s.rightShoulder1, s.rightShoulder2, s.dpadUp,
             s.dpadDown, s.dpadLeft, s.dpadRight, s.square, s.triangle, s.cross, s.circle, s.leftShock,
             s.rightShock, s.start, s.select);
}

void FormatXInput(bool ok, int slot, const xinput::State& xs, char* out, size_t size) {
    if (!ok) {
        snprintf(out, size, "-");
        return;
    }
    const xinput::Gamepad& g = xs.gamepad;
    snprintf(out, size, "s%d L%d,%d R%d,%d LT=%u RT=%u B=%04X", slot, g.thumbLX, g.thumbLY, g.thumbRX, g.thumbRY,
             static_cast<unsigned int>(g.leftTrigger), static_cast<unsigned int>(g.rightTrigger),
             static_cast<unsigned int>(g.buttons));
}

// Dege's speed chain, with a readability check at every hop.
void FormatSpeedChain(uintptr_t obj, char* out, size_t size) {
    if (!obj) {
        snprintf(out, size, "noobj");
        return;
    }
    uintptr_t p1 = 0, p2 = 0;
    int32_t v = 0;
    if (!mem::Read(obj + kSpeedHop1, p1)) {
        snprintf(out, size, "X@obj+80");
        return;
    }
    if (!mem::Read(p1 + kSpeedHop2, p2)) {
        snprintf(out, size, "%08X>X", U32(p1));
        return;
    }
    if (!mem::Read(p2 + kSpeedHop3, v)) {
        snprintf(out, size, "%08X>%08X>X", U32(p1), U32(p2));
        return;
    }
    snprintf(out, size, "%08X>%08X>%d(%.3f)", U32(p1), U32(p2), v, v / 256.0);
}

void AppendReason(char* buf, size_t size, const char* what) {
    size_t len = std::strlen(buf);
    if (len + 1 >= size)
        return;
    snprintf(buf + len, size - len, "%s%s", len ? "," : "", what);
}

} // namespace

// ---------------------------------------------------------------------------
// Public entry points
// ---------------------------------------------------------------------------
void OnLoad() {
    OpenLog();
    SYSTEMTIME st;
    GetLocalTime(&st);
    log::Write("%s", "");
    log::Write("==== GTA2ModernControls Phase 1 probe | built %s %s | session %04u-%02u-%02u %02u:%02u:%02u ====",
               __DATE__, __TIME__, static_cast<unsigned int>(st.wYear), static_cast<unsigned int>(st.wMonth),
               static_cast<unsigned int>(st.wDay), static_cast<unsigned int>(st.wHour),
               static_cast<unsigned int>(st.wMinute), static_cast<unsigned int>(st.wSecond));
    log::Write("log: %s", log::Path());

    char self[MAX_PATH] = "?";
    HMODULE mod = SelfModule();
    if (mod)
        ModulePath(mod, self, MAX_PATH);
    log::Write("asi: %s (0x%08X)", self, U32(mod));

    LogExeInfo();
    LogModule("GInputII.asi");
    LogModule("FrontendFixII.asi");
    LogModule("dinput.dll");

    DumpPatchSites();
    DumpBytes("gReplay (0x3C bytes)", reinterpret_cast<uintptr_t>(gReplay), 0x3C);
    DumpHookSite("before our hook");
}

void OnHookInstalled() {
    DumpHookSite("after our hook");
}

void OnProcessPlayerKeys(CPlayerPed* player) {
    if (!log::IsOpen())
        return;
    if (!g_lateInitDone)
        LateInit();

    DWORD now = GetTickCount();

    // GInput may in theory show up later than us: retry every 5 s.
    if (!ginput::HasGetPad() && now - g_lastGInputRetry > 5000) {
        g_lastGInputRetry = now;
        if (ginput::Resolve()) {
            log::Write("GInput: resolved late");
            ginput::LogResolution();
        }
    }

    CGame* game = SafeGame();
    if (!game || !player || player != game->m_pCurrentPlayer)
        return;
    if (!mem::IsReadable(player, sizeof(CPlayerPed)))
        return;

    ++g_frame;

    // --- cheap per-frame sample (used for transitions and the marker) ---
    KeyState ks;
    std::memset(&ks, 0, sizeof(ks));

    CPed* ped = player->GetPed();
    bool pedOk = mem::IsReadable(ped, sizeof(CPed));
    ks.ped = reinterpret_cast<uintptr_t>(ped);
    ks.pedState = pedOk ? ped->m_nPedState : 0xFFFFFFFFu;
    CCar* car = pedOk ? ped->GetCar() : NULL;
    ks.hasCar = car != NULL;

    uint8_t inCar = 0;
    ks.inCarByte = mem::Read(kInCarFlagAddr, inCar) ? inCar : -1;

    bool replayOk = mem::IsReadable(gReplay, sizeof(CReplay));
    ks.replay = replayOk ? gReplay->IsPlayingBack() : false;
    ks.gameStatus = static_cast<int>(game->m_eGameStatus);

    ginput::Snapshot gi;
    ginput::Sample(gi);
    ks.giConnected = gi.valid && (gi.connectedCall || gi.connectedField);

    xinput::State xs;
    int xslot = -1;
    bool xok = xinput::Poll(xs, xslot);
    ks.xiSlot = xok ? xslot : -1;

    // --- marker on L3 rising edge ---
    bool l3 = (gi.valid && gi.state.leftShock != 0) || (xok && (xs.gamepad.buttons & xinput::BTN_LEFT_THUMB) != 0);
    if (l3 && !g_prevL3) {
        ++g_markerCount;
        log::Write("--- MARKER %u --- (t=%lu f=%u)", g_markerCount, static_cast<unsigned long>(now), g_frame);
        log::Flush();
    }
    g_prevL3 = l3;

    // --- transitions ---
    char why[96] = "";
    if (!g_havePrev) {
        AppendReason(why, sizeof(why), "first");
    } else {
        if (ks.ped != g_prev.ped)
            AppendReason(why, sizeof(why), "ped");
        if (ks.pedState != g_prev.pedState)
            AppendReason(why, sizeof(why), "state");
        if (ks.inCarByte != g_prev.inCarByte)
            AppendReason(why, sizeof(why), "incar");
        if (ks.hasCar != g_prev.hasCar)
            AppendReason(why, sizeof(why), "car");
        if (ks.giConnected != g_prev.giConnected)
            AppendReason(why, sizeof(why), "gipad");
        if (ks.xiSlot != g_prev.xiSlot)
            AppendReason(why, sizeof(why), "xipad");
        if (ks.replay != g_prev.replay)
            AppendReason(why, sizeof(why), "replay");
        if (ks.gameStatus != g_prev.gameStatus)
            AppendReason(why, sizeof(why), "gamestatus");
    }
    // Every frame while Special is held in a car: the horn logic counts frames.
    if (car && player->m_bButtonSpecial)
        AppendReason(why, sizeof(why), "horn");
    bool transition = why[0] != '\0';
    g_prev = ks;
    g_havePrev = true;

    if (!transition && now - g_lastLogTick < kLogIntervalMs)
        return;
    g_lastLogTick = now;
    if (!transition)
        AppendReason(why, sizeof(why), "tick");

    // --- full sample, only when a line is written ---
    uintptr_t rawPed = reinterpret_cast<uintptr_t>(player->m_pPed);
    uintptr_t rawCar = pedOk ? reinterpret_cast<uintptr_t>(ped->m_pCurrentCar) : 0;
    uintptr_t obj = 0;
    int rot = 0, rot2 = 0;
    bool objOk = false;
    if (pedOk) {
        obj = reinterpret_cast<uintptr_t>(ped->m_pObject);
        int16_t r = 0, r2 = 0;
        objOk = mem::Read(obj + 0x40, r) && mem::Read(obj + 0x42, r2);
        rot = r;
        rot2 = r2;
    }

    uint8_t ctl[2] = {0, 0};
    bool ctlOk = mem::ReadBytes(kInputBitsAddr, ctl, sizeof(ctl));

    char rp[NUM_CONTROLKEYS + 1];
    for (int i = 0; i < NUM_CONTROLKEYS; ++i)
        rp[i] = replayOk ? (gReplay->GetButton(i) ? '1' : '0') : '?';
    rp[NUM_CONTROLKEYS] = '\0';

    const bool pbVals[] = {player->m_bButtonForward,    player->m_bButtonBackward,  player->m_bButtonLeft,
                           player->m_bButtonRight,      player->m_bButtonAttack,    player->m_bButtonEnterExit,
                           player->m_bButtonHandbrakeJump, player->m_bButtonPrevWeapon, player->m_bButtonNextWeapon,
                           player->m_bButtonSpecial,    player->m_bButtonSpecial2};
    const size_t pbCount = sizeof(pbVals) / sizeof(pbVals[0]);
    char pb[pbCount + 1];
    for (size_t i = 0; i < pbCount; ++i)
        pb[i] = pbVals[i] ? '1' : '0';
    pb[pbCount] = '\0';

    char spd[64];
    if (objOk)
        FormatSpeedChain(obj, spd, sizeof(spd));
    else
        snprintf(spd, sizeof(spd), "noobj");

    char giStr[192];
    FormatGInput(gi, giStr, sizeof(giStr));
    char xiStr[96];
    FormatXInput(xok, xslot, xs, xiStr, sizeof(xiStr));

    char rotStr[32];
    if (objOk)
        snprintf(rotStr, sizeof(rotStr), "%d(%.2f) rot2=%d", rot, rot / 4.0, rot2);
    else
        snprintf(rotStr, sizeof(rotStr), "? rot2=?");

    char ctlStr[16];
    if (ctlOk)
        snprintf(ctlStr, sizeof(ctlStr), "%02X,%02X", ctl[0], ctl[1]);
    else
        snprintf(ctlStr, sizeof(ctlStr), "?");

    // Special button + its change flag, and the car's horn (+0xA7) / siren
    // (+0xA4) bytes, to follow the tap = siren / hold = horn logic (0x4260C0).
    char hornStr[32];
    uint8_t horn = 0;
    uint8_t siren = 0;
    if (car && mem::Read(reinterpret_cast<uintptr_t>(car) + 0xA7, horn) &&
        mem::Read(reinterpret_cast<uintptr_t>(car) + 0xA4, siren))
        snprintf(hornStr, sizeof(hornStr), "%d%d hn=%02X sr=%02X", player->m_bButtonSpecial ? 1 : 0,
                 player->m_bOldButtonSpecial ? 1 : 0, horn, siren);
    else
        snprintf(hornStr, sizeof(hornStr), "%d%d", player->m_bButtonSpecial ? 1 : 0,
                 player->m_bOldButtonSpecial ? 1 : 0);

    char icStr[8];
    if (ks.inCarByte >= 0)
        snprintf(icStr, sizeof(icStr), "%02X", ks.inCarByte);
    else
        snprintf(icStr, sizeof(icStr), "?");

    log::Write("t=%lu f=%u [%s] ped=%08X raw=%08X st=%d car=%d(%08X) ic=%s rot=%s ctl=%s rp=%s pb=%s sp=%s spd=%s "
               "gs=%d rep=%d gi=%s xi=%s",
               static_cast<unsigned long>(now), g_frame, why, U32(ks.ped), U32(rawPed), static_cast<int>(ks.pedState),
               ks.hasCar ? 1 : 0, U32(rawCar), icStr, rotStr, ctlStr, rp, pb, hornStr, spd, ks.gameStatus,
               ks.replay ? 1 : 0,
               giStr, xiStr);
}

void OnShutdown() {
    log::Write("==== shutdown (frames=%u markers=%u) ====", g_frame, g_markerCount);
    log::Close();
}

} // namespace probe
} // namespace gmc
