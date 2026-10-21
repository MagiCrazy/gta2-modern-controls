// Minimal append-only text log written with raw Win32 file I/O.
// Every line goes straight to WriteFile, so nothing is lost if the game crashes.
#pragma once

namespace gmc {
namespace log {

// Opens (or creates) the log file in append mode. Safe to call more than once.
bool Open(const char* path);
bool IsOpen();
const char* Path();

// printf-style; a trailing newline is appended automatically.
void Write(const char* fmt, ...)
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((format(printf, 1, 2)))
#endif
    ;

// Forces the OS to commit buffered data (FlushFileBuffers). Not needed for
// crash-safety, only called on markers and on shutdown.
void Flush();
void Close();

} // namespace log
} // namespace gmc
