# GTA2 Modern Controls

ASI plugin for **GTA 2 9.6.0.0** that replaces the "tank" movement with modern
gamepad controls, primarily for the Steam Deck.

> **Status: work in progress.** Nothing is playable yet.

## What it does

| Situation | Behaviour |
|---|---|
| On foot | The left stick / D-pad gives an absolute on-screen direction: the player turns towards it and walks, at a speed that follows stick deflection. |
| In a vehicle | R2 accelerates, L2 brakes / reverses, left stick X / D-pad steers. |
| Bonus | The right stick aims independently of the walking direction (twin-stick). |

Other actions keep a sensible gamepad mapping, and classic controls can be
restored in game. It is a 9.6 port of Dege's Twin Stick mod (made for 11.44),
built as a plugin-sdk ASI meant to coexist with GInput and Frontend Fix.

## Install

1. Install an ASI loader (e.g. Ultimate ASI Loader) in the GTA 2 directory.
2. Copy `GTA2ModernControls.asi` into `GTA2/scripts/`.

## Build

The plugin is built with [premake5](https://premake.github.io/) on top of
[plugin-sdk](https://github.com/DK22Pac/plugin-sdk) (`plugin_II`), vendored as
a git submodule in `vendor/plugin-sdk`.

GitHub Actions (`.github/workflows/build.yml`) builds it on every push and pull
request:

- **Linux (primary)**: clang + mingw-w64 cross-compilation to i686 Windows.
  It builds `plugin_II` with plugin-sdk's own premake script, then
  `GTA2ModernControls.asi`, and publishes it as a workflow artifact.
- **Windows (fallback, non-blocking)**: the same with MSVC / msbuild.

Local build with clang + mingw-w64:

```sh
git submodule update --init
export PLUGIN_SDK_DIR="$PWD/vendor/plugin-sdk"
(cd vendor/plugin-sdk && premake5 gmake --file=tools/premake/premake5.lua \
  && make -C plugin_II config=release CXX=clang++ CC=clang AR=ar)
premake5 gmake && make -C build config=release CXX=clang++ CC=clang
# -> build/bin/Release/GTA2ModernControls.asi
```

## Credits

- **Dege** — Twin Stick mod for GTA 2 11.44 (public domain, GTAMP), the basis
  of the movement logic.
- **Logofero / DenLover and the GTAMP community** — MISI, on which the Twin
  Stick mod is built.
- **DK22Pac and the plugin-sdk contributors** — plugin-sdk (Zlib).
- **gennariarmando** — GTA 2 Frontend Fix and its GInput API, used as a
  reference.
- **The GInput authors** — the gamepad input layer this plugin reads from.

## License

[Zlib](LICENSE).
