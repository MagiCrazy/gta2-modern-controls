// Read-only memory helpers: every probe read goes through these so that a bad
// pointer never turns into an access violation.
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace gmc {
namespace mem {

// True when [addr, addr + size) is committed and readable (no guard page).
bool IsReadable(uintptr_t addr, size_t size);

inline bool IsReadable(const void* p, size_t size) {
    return IsReadable(reinterpret_cast<uintptr_t>(p), size);
}

// Copies size bytes into out if readable; returns false (and leaves out
// untouched) otherwise.
inline bool ReadBytes(uintptr_t addr, void* out, size_t size) {
    if (!IsReadable(addr, size))
        return false;
    std::memcpy(out, reinterpret_cast<const void*>(addr), size);
    return true;
}

template <typename T>
inline bool Read(uintptr_t addr, T& out) {
    return ReadBytes(addr, &out, sizeof(T));
}

// Formats size bytes as "AA BB CC" into out (out must hold 3 * size chars).
void FormatHex(const uint8_t* bytes, size_t size, char* out, size_t outSize);

} // namespace mem
} // namespace gmc
