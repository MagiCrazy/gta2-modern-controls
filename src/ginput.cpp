#include "ginput.h"

#include "log.h"
#include "memutil.h"

#include <windows.h>
#include <cstring>

namespace gmc {
namespace ginput {

namespace {

// Static member: CPad* __cdecl CPad::GetPad(char)  (mangled "?GetPad@CPad@@SAPAV1@D@Z")
typedef void*(__cdecl* GetPadFn)(char);
// Member: bool __thiscall CPad::IsConnected()      (mangled "?IsConnected@CPad@@QAE_NXZ")
typedef bool(__thiscall* IsConnectedFn)(void*);

const char* const kModuleName = "GInputII.asi";

const char* const kGetPadNames[] = {"CPad::GetPad", "?GetPad@CPad@@SAPAV1@D@Z"};
const char* const kIsConnectedNames[] = {"CPad::IsConnected", "?IsConnected@CPad@@QAE_NXZ"};

HMODULE g_module = NULL;
GetPadFn g_getPad = NULL;
IsConnectedFn g_isConnected = NULL;
const char* g_getPadName = NULL;
const char* g_isConnectedName = NULL;

template <typename Fn>
Fn ToFn(FARPROC p) {
    Fn f;
    static_assert(sizeof(f) == sizeof(p), "function pointer size");
    std::memcpy(&f, &p, sizeof(f));
    return f;
}

template <typename Fn, size_t N>
Fn ResolveFirst(HMODULE mod, const char* const (&names)[N], const char*& usedName) {
    for (size_t i = 0; i < N; ++i) {
        FARPROC p = GetProcAddress(mod, names[i]);
        if (p) {
            usedName = names[i];
            return ToFn<Fn>(p);
        }
    }
    usedName = NULL;
    return NULL;
}

} // namespace

bool Resolve() {
    if (g_getPad)
        return true;
    g_module = GetModuleHandleA(kModuleName);
    if (!g_module)
        return false;
    g_getPad = ResolveFirst<GetPadFn>(g_module, kGetPadNames, g_getPadName);
    g_isConnected = ResolveFirst<IsConnectedFn>(g_module, kIsConnectedNames, g_isConnectedName);
    return g_getPad != NULL;
}

bool IsModuleLoaded() {
    return GetModuleHandleA(kModuleName) != NULL;
}

bool HasGetPad() {
    return g_getPad != NULL;
}

void LogResolution() {
    HMODULE mod = GetModuleHandleA(kModuleName);
    log::Write("GInput: module %s %s (handle 0x%08X)", kModuleName, mod ? "LOADED" : "not loaded",
               static_cast<unsigned int>(reinterpret_cast<uintptr_t>(mod)));
    if (!mod)
        return;

    // Report every candidate name individually so the export naming is documented in the log.
    const char* const* lists[] = {kGetPadNames, kIsConnectedNames};
    const size_t counts[] = {sizeof(kGetPadNames) / sizeof(kGetPadNames[0]),
                             sizeof(kIsConnectedNames) / sizeof(kIsConnectedNames[0])};
    for (size_t l = 0; l < 2; ++l) {
        for (size_t i = 0; i < counts[l]; ++i) {
            FARPROC p = GetProcAddress(mod, lists[l][i]);
            log::Write("GInput: export %-32s -> %s 0x%08X", lists[l][i], p ? "resolved" : "MISSING",
                       static_cast<unsigned int>(reinterpret_cast<uintptr_t>(p)));
        }
    }
    log::Write("GInput: using GetPad=%s IsConnected=%s", g_getPadName ? g_getPadName : "(none)",
               g_isConnectedName ? g_isConnectedName : "(none)");
}

bool Sample(Snapshot& out) {
    std::memset(&out, 0, sizeof(out));
    if (!g_getPad)
        return false;

    void* pad = g_getPad(0);
    out.padAddr = reinterpret_cast<uintptr_t>(pad);
    PadPrefix prefix;
    if (!mem::Read(out.padAddr, prefix))
        return false;

    out.valid = true;
    out.state = prefix.newState;
    out.connectedField = prefix.padConnected != 0;
    out.connectedCall = g_isConnected ? g_isConnected(pad) : false;
    return true;
}

} // namespace ginput
} // namespace gmc
