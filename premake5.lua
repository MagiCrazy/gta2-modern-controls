-- GTA2ModernControls build script.
--
-- The plugin_II static library must be built first with plugin-sdk's own
-- premake script (see .github/workflows/build.yml), which outputs it to
-- <sdk>/output/lib. The SDK location defaults to the vendor/plugin-sdk
-- submodule and can be overridden with the PLUGIN_SDK_DIR environment variable.
--
--   premake5 gmake   -> build/Makefile        (clang + mingw-w64, i686)
--   premake5 vs2026  -> build/*.slnx/.vcxproj (MSVC, Win32)

local sdkdir = os.getenv("PLUGIN_SDK_DIR")
if sdkdir == nil or sdkdir == "" then
    sdkdir = path.join(_MAIN_SCRIPT_DIR, "vendor/plugin-sdk")
end
sdkdir = path.getabsolute(sdkdir)

if not os.isdir(path.join(sdkdir, "plugin_II")) then
    error("plugin-sdk not found at '" .. sdkdir .. "' (run: git submodule update --init)")
end

local mingw = _ACTION == "gmake" or _ACTION == "gmake2"

workspace "GTA2ModernControls"
    location "build"
    configurations { "Release", "Debug" }

project "GTA2ModernControls"
    kind "SharedLib"
    language "C++"
    cppdialect "C++latest"
    architecture "x86"
    system "windows"
    characterset "MBCS"
    staticruntime "On"

    targetname "GTA2ModernControls"
    targetprefix ""
    targetextension ".asi"
    targetdir "build/bin/%{cfg.buildcfg}"
    objdir "build/obj/%{cfg.buildcfg}"

    files { "src/**.h", "src/**.cpp" }

    includedirs {
        "src",
        sdkdir .. "/shared",
        sdkdir .. "/shared/game",
        sdkdir .. "/plugin_II",
        sdkdir .. "/plugin_II/game_II",
    }

    libdirs { sdkdir .. "/output/lib" }

    defines { "GTA2", "PLUGIN_SGV_96EN", "_CRT_SECURE_NO_WARNINGS" }

    filter "configurations:Release"
        defines "NDEBUG"
        optimize "On"
        symbols "Off"
        links "Plugin_II"

    filter "configurations:Debug"
        defines "DEBUG"
        symbols "On"
        links "Plugin_II_d"

    filter {}

    if mingw then
        toolset "clang"
        buildoptions {
            "--target=i686-w64-mingw32",
            "-fpermissive",
            "-fcommon",
            "-fms-extensions",
            "-Wno-invalid-offsetof",
            "-Wno-microsoft-include",
            "-Wno-builtin-macro-redefined",
            "-D__cpp_concepts=202202L",
        }
        linkoptions {
            "--target=i686-w64-mingw32",
            "-static",
            "-static-libgcc",
            "-static-libstdc++",
        }
    else
        systemversion "latest"
        defines "_CRT_NON_CONFORMING_SWPRINTFS"
        buildoptions "/sdl-"
        linkoptions "/SAFESEH:NO"
        multiprocessorcompile "On"
    end
