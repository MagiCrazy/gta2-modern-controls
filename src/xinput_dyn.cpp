#include "xinput_dyn.h"

#include <windows.h>
#include <cstring>

namespace gmc {
namespace xinput {

namespace {

typedef DWORD(WINAPI* XInputGetStateFn)(DWORD, State*);

const char* const kDllNames[] = {"xinput1_4.dll", "xinput1_3.dll", "xinput9_1_0.dll"};
const DWORD kRescanIntervalMs = 2000;
const DWORD kErrorSuccess = 0; // ERROR_SUCCESS

bool g_tried = false;
HMODULE g_dll = NULL;
const char* g_dllName = NULL;
XInputGetStateFn g_getState = NULL;
int g_slot = -1;
DWORD g_lastScan = 0;
bool g_scannedOnce = false;

} // namespace

bool Load() {
    if (g_tried)
        return g_getState != NULL;
    g_tried = true;

    for (size_t i = 0; i < sizeof(kDllNames) / sizeof(kDllNames[0]); ++i) {
        HMODULE h = LoadLibraryA(kDllNames[i]);
        if (!h)
            continue;
        FARPROC p = GetProcAddress(h, "XInputGetState");
        if (!p) {
            FreeLibrary(h);
            continue;
        }
        std::memcpy(&g_getState, &p, sizeof(g_getState));
        g_dll = h;
        g_dllName = kDllNames[i];
        return true;
    }
    return false;
}

const char* LoadedDllName() {
    return g_dllName ? g_dllName : "(none)";
}

bool Poll(State& state, int& slot) {
    std::memset(&state, 0, sizeof(state));
    slot = -1;
    if (!g_getState)
        return false;

    if (g_slot >= 0) {
        if (g_getState(static_cast<DWORD>(g_slot), &state) == kErrorSuccess) {
            slot = g_slot;
            return true;
        }
        g_slot = -1; // disconnected: fall through to a (throttled) rescan
        std::memset(&state, 0, sizeof(state));
    }

    DWORD now = GetTickCount();
    if (g_scannedOnce && now - g_lastScan < kRescanIntervalMs)
        return false;
    g_scannedOnce = true;
    g_lastScan = now;

    for (DWORD i = 0; i < 4; ++i) {
        if (g_getState(i, &state) == kErrorSuccess) {
            g_slot = static_cast<int>(i);
            slot = g_slot;
            return true;
        }
    }
    std::memset(&state, 0, sizeof(state));
    return false;
}

} // namespace xinput
} // namespace gmc
