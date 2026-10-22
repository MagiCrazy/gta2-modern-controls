# GTA2 Modern Controls

ASI plugin for **GTA 2 9.6.0.0** that replaces the "tank" movement with modern
gamepad controls, primarily for the Steam Deck.

> **Status: work in progress.** Vehicle, on-foot controls and right-stick
> aiming are in testing.

## What it does

| Situation | Behaviour |
|---|---|
| On foot | The left stick / D-pad gives an absolute on-screen direction: the player turns towards it and walks, at a speed that follows stick deflection. |
| In a vehicle | R2 accelerates, L2 brakes / reverses, left stick X / D-pad steers. |
| Bonus | The right stick aims independently of the walking direction (twin-stick). |

Other actions keep a sensible gamepad mapping (see
[docs/controls-test-plan.md](docs/controls-test-plan.md)), and classic
controls can be restored in game with **Select + R3**. Settings live in
`GTA2ModernControls.ini`, created next to the `.asi` on first launch. It is a 9.6 port of Dege's Twin Stick mod (made for 11.44),
built as a plugin-sdk ASI meant to coexist with GInput and Frontend Fix.

## Download

Every release ships the same two assets: `GTA2ModernControls.asi` and
`SHA256SUMS`.

| Channel | URL |
|---|---|
| Latest stable | `https://github.com/MagiCrazy/gta2-modern-controls/releases/latest/download/GTA2ModernControls.asi` |
| Pinned version | `https://github.com/MagiCrazy/gta2-modern-controls/releases/download/vX.Y.Z/GTA2ModernControls.asi` |
| Development (tip of `main`, untested) | `https://github.com/MagiCrazy/gta2-modern-controls/releases/download/dev/GTA2ModernControls.asi` |

Replace the file name with `SHA256SUMS` to get the checksums. Scripts should
pin a version and verify it:

```sh
base=https://github.com/MagiCrazy/gta2-modern-controls/releases/download/v0.1.0
curl -fsSL --remote-name-all "$base/GTA2ModernControls.asi" "$base/SHA256SUMS"
sha256sum -c SHA256SUMS
```

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
  `GTA2ModernControls.asi`, and publishes it as a workflow artifact. Pushes
  to `main` refresh the `dev` pre-release; pushing a `vX.Y.Z` tag publishes a
  release (a `-suffix` such as `v0.2.0-rc1` makes it a pre-release).
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
