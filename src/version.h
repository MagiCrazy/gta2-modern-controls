// Plugin version, passed by the build as an unquoted token sequence
// (premake: GMC_VERSION_RAW=v1.0.0, set from the GMC_VERSION environment
// variable in CI) and turned into a string here, which avoids quoting
// differences between the gmake and MSVC generators.
#pragma once

#ifndef GMC_VERSION_RAW
#define GMC_VERSION_RAW dev
#endif

#define GMC_STRINGIFY2(x) #x
#define GMC_STRINGIFY(x) GMC_STRINGIFY2(x)
#define GMC_VERSION GMC_STRINGIFY(GMC_VERSION_RAW)
