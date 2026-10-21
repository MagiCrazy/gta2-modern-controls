#include "plugin.h"
#include "CGame.h"

#include "gameutil.h"

#include "memutil.h"

#include <windows.h>

namespace gmc {

namespace {
// Any address inside this module, to find our own HMODULE.
const char g_anchor = 0;

bool ModulePath(HMODULE mod, char* out, DWORD size) {
    DWORD n = GetModuleFileNameA(mod, out, size);
    if (n == 0 || n >= size) {
        out[0] = '\0';
        return false;
    }
    return true;
}
} // namespace

CGame* SafeGame() {
    uintptr_t game = 0;
    if (!mem::Read(reinterpret_cast<uintptr_t>(gGame), game))
        return NULL;
    if (!mem::IsReadable(game, sizeof(CGame)))
        return NULL;
    return reinterpret_cast<CGame*>(game);
}

void SelfDirectory(char* out, size_t size) {
    char path[MAX_PATH] = {};
    HMODULE self = NULL;
    bool ok = GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                 &g_anchor, &self) &&
              ModulePath(self, path, MAX_PATH);
    if (!ok)
        ModulePath(NULL, path, MAX_PATH);

    char* slash = NULL;
    for (char* c = path; *c; ++c) {
        if (*c == '\\' || *c == '/')
            slash = c;
    }
    if (slash)
        slash[1] = '\0';
    else
        path[0] = '\0';
    lstrcpynA(out, path, static_cast<int>(size));
}

} // namespace gmc
