#include "log.h"

#include <windows.h>
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace gmc {
namespace log {

namespace {
HANDLE g_file = INVALID_HANDLE_VALUE;
char g_path[MAX_PATH] = {};
CRITICAL_SECTION g_lock;
bool g_lockReady = false;
} // namespace

bool Open(const char* path) {
    if (!g_lockReady) {
        InitializeCriticalSection(&g_lock);
        g_lockReady = true;
    }
    if (g_file != INVALID_HANDLE_VALUE)
        return true;

    HANDLE h = CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                           OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return false;

    g_file = h;
    lstrcpynA(g_path, path, MAX_PATH);
    return true;
}

bool IsOpen() {
    return g_file != INVALID_HANDLE_VALUE;
}

const char* Path() {
    return g_path;
}

void Write(const char* fmt, ...) {
    if (g_file == INVALID_HANDLE_VALUE)
        return;

    char buf[2048];
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(buf, sizeof(buf) - 2, fmt, args);
    va_end(args);
    if (n < 0)
        return;
    if (n > static_cast<int>(sizeof(buf)) - 3)
        n = static_cast<int>(sizeof(buf)) - 3;
    buf[n++] = '\r';
    buf[n++] = '\n';

    EnterCriticalSection(&g_lock);
    DWORD written = 0;
    WriteFile(g_file, buf, static_cast<DWORD>(n), &written, NULL);
    LeaveCriticalSection(&g_lock);
}

void Flush() {
    if (g_file != INVALID_HANDLE_VALUE)
        FlushFileBuffers(g_file);
}

void Close() {
    if (g_file == INVALID_HANDLE_VALUE)
        return;
    FlushFileBuffers(g_file);
    CloseHandle(g_file);
    g_file = INVALID_HANDLE_VALUE;
}

} // namespace log
} // namespace gmc
