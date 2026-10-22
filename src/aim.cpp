#include "plugin.h"
#include "CObject.h"

#include "aim.h"

#include "log.h"

#include <cstdint>
#include <cstring>

namespace gmc {
namespace aim {

namespace {

// Byte patches from Dege's l_EnablePedSecondaryAngle (MISI.cpp), checked
// against GTA 2 9.6 by the phase 1 probe. Each site jumps to a code cave in
// nop padding that compares the object with an immediate (the target) and
// reads +0x42 for it, +0x40 for every other object.
//
// Sprite: 0x49C5DC  mov cx,[esi+40h]; lea edi,[esi+40h]
//   -> jmp 49C6F1; cave: cmp esi,<target>; je 49C731; jmp 49C6D4
//      49C6D4: original two instructions; jmp 49C5E3
//      49C731: same with +42h;            jmp 49C5E3
// Shots: 0x433A44  mov cx,[ecx+40h]   (CObject::GetRotation, 0x433A40)
//   -> jmp 433A84; cave: cmp ecx,<target>; je 433A6A; jmp 433A5A
//      433A5A: mov cx,[ecx+40h]; jmp 433A48
//      433A6A: mov cx,[ecx+42h]; jmp 433A48

struct Patch {
    uintptr_t addr;
    uint8_t original[12];
    uint8_t patched[12];
    size_t size;
};

const uint8_t kNop = 0x90;

// Caves first, entry jumps last, so a jump never targets a half-written cave.
const Patch kPatches[] = {
    {0x49C6D4, {kNop, kNop, kNop, kNop, kNop, kNop, kNop, kNop, kNop, kNop, kNop, kNop},
     {0x66, 0x8B, 0x4E, 0x40, 0x8D, 0x7E, 0x40, 0xE9, 0x03, 0xFF, 0xFF, 0xFF}, 12},
    {0x49C6F1, {kNop, kNop, kNop, kNop, kNop, kNop, kNop, kNop, kNop, kNop},
     {0x81, 0xFE, 0x00, 0x00, 0x00, 0x00, 0x74, 0x38, 0xEB, 0xD9}, 10},
    {0x49C731, {kNop, kNop, kNop, kNop, kNop, kNop, kNop, kNop, kNop, kNop, kNop, kNop},
     {0x66, 0x8B, 0x4E, 0x42, 0x8D, 0x7E, 0x42, 0xE9, 0xA6, 0xFE, 0xFF, 0xFF}, 12},
    {0x433A5A, {kNop, kNop, kNop, kNop, kNop, kNop}, {0x66, 0x8B, 0x49, 0x40, 0xEB, 0xE8}, 6},
    {0x433A6A, {kNop, kNop, kNop, kNop, kNop, kNop}, {0x66, 0x8B, 0x49, 0x42, 0xEB, 0xD8}, 6},
    {0x433A84, {kNop, kNop, kNop, kNop, kNop, kNop, kNop, kNop, kNop, kNop},
     {0x81, 0xF9, 0x00, 0x00, 0x00, 0x00, 0x74, 0xDE, 0xEB, 0xCC}, 10},
    {0x49C5DC, {0x66, 0x8B, 0x4E, 0x40, 0x8D, 0x7E, 0x40}, {0xE9, 0x10, 0x01, 0x00, 0x00, 0x90, 0x90}, 7},
    {0x433A44, {0x66, 0x8B, 0x49, 0x40}, {0xEB, 0x3E, 0x90, 0x90}, 4},
};

// imm32 of the two "cmp reg, <target>" instructions.
const uintptr_t kTargetImmediates[] = {0x49C6F3, 0x433A86};

bool g_installed = false;
bool g_tried = false;
uintptr_t g_target = 0;

} // namespace

bool Install() {
    if (g_tried)
        return g_installed;
    g_tried = true;

    for (const Patch& p : kPatches) {
        uint8_t current[12];
        plugin::patch::GetRaw(p.addr, current, p.size);
        if (std::memcmp(current, p.original, p.size) != 0) {
            log::Write("aim: unexpected code at 0x%08X, twin-stick aiming disabled", static_cast<unsigned int>(p.addr));
            return false;
        }
    }
    for (const Patch& p : kPatches)
        plugin::patch::SetRaw(p.addr, const_cast<uint8_t*>(p.patched), p.size);

    g_installed = true;
    log::Write("aim: secondary heading patches installed");
    return true;
}

bool IsInstalled() {
    return g_installed;
}

void SetTarget(CObject* obj) {
    uintptr_t target = reinterpret_cast<uintptr_t>(obj);
    if (!g_installed || target == g_target)
        return;
    uint32_t imm = static_cast<uint32_t>(target);
    for (uintptr_t addr : kTargetImmediates)
        plugin::patch::SetRaw(addr, &imm, sizeof(imm));
    g_target = target;
}

} // namespace aim
} // namespace gmc
