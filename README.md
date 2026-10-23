# GTA2 Modern Controls

ASI plugin for **GTA 2 9.6.0.0** that replaces the "tank" controls with modern
gamepad controls: absolute movement, trigger driving, twin-stick aiming and a
free camera look. Made and tested on the Steam Deck; other XInput gamepads
should work but are untested.

It is a 9.6 port and extension of Dege's Twin Stick mod (made for 11.44),
built as a plugin-sdk ASI that coexists with GInput, Frontend Fix and the
Widescreen Fix.

## Controls

Button names as printed on the Steam Deck (Xbox layout). Select is the View
button (⧉), Start the Menu button (≡).

| Button | On foot | In a vehicle |
|---|---|---|
| Left stick | Walk towards that direction on screen | Steer |
| Right stick | Aim (shots and facing); released = aim where you walk | Look around; tank, fire truck, gun jeep: ← / → rotate the turret |
| R2 | Attack | Accelerate |
| L2 | Hold: the right stick looks around instead of aiming | Brake / reverse |
| A | Jump | Handbrake |
| X | — | Attack (vehicle weapon) |
| Y | Enter vehicle | Exit vehicle |
| B | Special | Horn; tap = siren on emergency vehicles |
| D-pad ← / → | Previous / next weapon | Previous / next weapon |
| D-pad ↑ / ↓ | — | Next / previous radio station |
| L3 | Repeat the last message (press again for older ones) | same |
| R3 | Show the district name | same |
| Select + R3 | Switch between modern and classic controls | same |
| Start | Pause | Pause |

L1 and R1 are left free. In classic mode the game and GInput behave as
without the plugin; only L3, R3, Select + R3 and the radio on D-pad ↑ / ↓
keep working.

Notes:

- R2 held while leaving a vehicle does not shoot until it is released.
- The turret rotates at the game's own speed, like the original
  Special + left / right, without losing the steering.
- Right-stick aiming reuses Dege's secondary heading: the player can walk one
  way and shoot another. The game has a single sprite per character, so
  walking backwards while aiming forwards "slides".

## Install

Requirements: GTA 2 **9.6.0.0** with an ASI loader (e.g. Ultimate ASI Loader
as `dinput.dll`). GInput is recommended (it is how the game reads the pad on
the Deck); without it the plugin reads XInput directly.

1. Download `GTA2ModernControls.asi` from the
   [latest release](https://github.com/MagiCrazy/gta2-modern-controls/releases/latest).
2. Copy it into `GTA2/scripts/`.
3. Start the game. `GTA2ModernControls.ini` is created next to the `.asi`.

## Settings

`GTA2ModernControls.ini` (delete it to get the defaults back, with comments):

| Key | Default | Effect |
|---|---|---|
| `[General] Enabled` | 1 | Modern controls on; also saved by Select + R3. |
| `[General] ProbeLog` | 0 | 1 = write `GTA2ModernControls.log` (diagnostics for bug reports; grows quickly). |
| `[Vehicle] Enabled` | 1 | Modern vehicle controls. |
| `[Vehicle] TriggerThreshold` | 40 | Trigger travel (0-255) that counts as pressed. |
| `[Vehicle] TurretAim` | 0 | 0 = right stick ← / → rotates the turret, 1 = the turret points at the stick. |
| `[Vehicle] TurretStep` | 20 | Turret turn per frame (degrees x 4); 20 is the game's own speed. |
| `[OnFoot] Enabled` | 1 | Absolute on-foot movement. |
| `[OnFoot] TurnStep` | 96 | Max turn per frame (degrees x 4). Higher = snappier. |
| `[OnFoot] WalkTolerance` | 180 | How well aligned the player must be before walking (degrees x 4). |
| `[Aim] Enabled` | 1 | Right-stick aiming. |
| `[Aim] DeadZone` | 80 | Right stick deflection (0-255) needed to aim or look. |
| `[Look] Enabled` | 1 | Right-stick camera look. |
| `[Look] CarRange` | 100 | Look distance in a vehicle, % of the game's own look. |
| `[Look] OnFootRange` | 100 | Look distance on foot (L2 held), same unit. |

## Download channels

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
base=https://github.com/MagiCrazy/gta2-modern-controls/releases/download/v1.0.0
curl -fsSL --remote-name-all "$base/GTA2ModernControls.asi" "$base/SHA256SUMS"
sha256sum -c SHA256SUMS
```

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
  release (a `-suffix` such as `v1.1.0-rc1` makes it a pre-release). The
  version (tag name, or `dev-<sha>`) is written at the top of the log.
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

Release validation: [docs/release-checklist.md](docs/release-checklist.md).

## Credits

- **Dege** — Twin Stick mod for GTA 2 11.44 (public domain, GTAMP): the
  secondary heading used for aiming, and the original idea.
- **Logofero / DenLover and the GTAMP community** — MISI, on which the Twin
  Stick mod is built.
- **DK22Pac and the plugin-sdk contributors** — plugin-sdk (Zlib).
- **gennariarmando** — GTA 2 Frontend Fix and its GInput API, used as a
  reference.
- **The GInput authors** — the gamepad input layer this plugin reads from.

## License

[Zlib](LICENSE).
