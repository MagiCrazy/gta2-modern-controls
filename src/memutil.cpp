#include "memutil.h"

#include <windows.h>
#include <cstdio>

namespace gmc {
namespace mem {

namespace {
const DWORD kReadableMask = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READ |
                            PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
} // namespace

bool IsReadable(uintptr_t addr, size_t size) {
    // The first 64 KiB are never mapped on Windows: catches null + small offsets.
    if (addr < 0x10000 || size == 0)
        return false;
    uintptr_t end = addr + size;
    if (end < addr)
        return false;

    // A range can span several regions; walk them all.
    uintptr_t cur = addr;
    while (cur < end) {
        MEMORY_BASIC_INFORMATION mbi;
        if (VirtualQuery(reinterpret_cast<LPCVOID>(cur), &mbi, sizeof(mbi)) == 0)
            return false;
        if (mbi.State != MEM_COMMIT)
            return false;
        if ((mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0)
            return false;
        if ((mbi.Protect & kReadableMask) == 0)
            return false;
        uintptr_t regionEnd = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
        if (regionEnd <= cur)
            return false;
        cur = regionEnd;
    }
    return true;
}

void FormatHex(const uint8_t* bytes, size_t size, char* out, size_t outSize) {
    if (outSize == 0)
        return;
    out[0] = '\0';
    size_t pos = 0;
    for (size_t i = 0; i < size; ++i) {
        if (pos + 4 > outSize)
            break;
        int n = snprintf(out + pos, outSize - pos, i == 0 ? "%02X" : " %02X", bytes[i]);
        if (n <= 0)
            break;
        pos += static_cast<size_t>(n);
    }
}

} // namespace mem
} // namespace gmc
