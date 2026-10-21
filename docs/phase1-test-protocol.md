# Phase 1 — probe test protocol (Steam Deck, Game mode)

The probe (`GTA2ModernControls.asi`, Phase 1 build) is **read-only**: it never
writes game memory, it only logs. Goal: settle the open questions of
`PLAN.md > Roadmap > Phase 1` from a single scripted session.

## 1. Deploy

```sh
# on the dev machine, with the CI artifact in the current directory
scp GTA2ModernControls.asi steamdeck:gta2-deck-test/GTA2/scripts/
# optional: start from an empty log
ssh steamdeck 'rm -f gta2-deck-test/GTA2/scripts/GTA2ModernControls.log'
```

(`sshd` must be running on the Deck: `sudo systemctl start sshd`.)

## 2. Run the sequence

Launch the GTA 2 non-Steam shortcut **in Game mode** (desktop mode gives no
controller input). Start a new game (or load a save) so the player is on foot.

Press **L3** (left stick click) *before* each step: every press writes
`--- MARKER n ---` to the log. Markers are only logged while the game world
is running (the hook does not run in the main menu). Hold each pose for
~2 seconds so the 10 Hz sampling catches it.

| Marker | Action |
|---|---|
| 1 | Stand still, facing **up** on screen (rotate with the stick/D-pad until the sprite faces up, then release). |
| 2 | Rotate a **full circle left** slowly (hold left), stop. |
| 3 | Rotate a **full circle right** slowly (hold right), stop. |
| 4 | **Walk** forward ~3 s, then walk backward ~2 s. Push the stick partially, then fully. |
| 5 | Walk to a car and **enter** it (as driver). Wait until seated. |
| 6 | Press **R2 progressively**: ~25 %, 50 %, 100 %, release (≈1 s each). |
| 7 | Press **L2 progressively** the same way (brake, then reverse). |
| 8 | **Steer**: full left, full right, while holding R2. |
| 9 | **Handbrake** while moving. |
| 10 | **Exit** the car. Wait until on foot. |
| 11 | Open the **pause menu**, stay ~3 s, close it. |
| 12 | Open the pause menu again and **quit to the main menu**. Then quit the game. |

## 3. Fetch the log

```sh
scp steamdeck:gta2-deck-test/GTA2/scripts/GTA2ModernControls.log .
```

## 4. Reading the log

Header (written once at load, then at the first hook call):

- `exe:` image base, SizeOfImage, PE timestamp, and whether they match 9.6
  (`0x400000`) or the 11.44 signature.
- `module:` whether `GInputII.asi`, `FrontendFixII.asi` and the ASI loader are
  loaded.
- `patch 0x........:` 16 bytes at each of Dege's code-patch sites, `MATCH` /
  `MISMATCH` for the two sites whose original bytes are known
  (`0x49C5DC`, `0x433A44`), and a padding classification for the code caves.
- `hook site 0x004A7683 (before/after our hook)`: the CALL target, and in which
  module it lands (shows whether Frontend Fix hooked it first and that we
  chain on top).
- `GInput: export ...`: which GInput export names resolved.
- `legend:` the meaning of every column of the per-frame lines.

Per-frame lines (`t=... f=... [why] ...`): written at ~10 Hz (`[tick]`) and
immediately on a transition (`[state]`, `[incar]`, `[car]`, `[gipad]`,
`[xipad]`, `[replay]`, `[gamestatus]`, `[ped]`).

## 5. What each check answers

| Question | Where to look | How to conclude |
|---|---|---|
| **Address hypothesis** (Dege's 11.44 addresses valid on 9.6) | `patch summary`, `ic=` and `ctl=` columns | `2/2 reference sites MATCH`; caves are padding or unused code; `ic` flips 00→non-zero exactly at markers 5/10; `ctl` bits follow the pressed actions (bit 0 forward, 1 backward, 2 left, 3 right, 4 attack, 5 enter/exit, 6 handbrake/jump, 7 prev weapon; 2nd byte: 0 next weapon, 1 special 1, 2 special 2). |
| **In-car detection** | `car=` vs `ic=` vs `st=` around markers 5 and 10 | Which of `GetCar()`, byte `0x5E20BC`, `m_nPedState` (`10` = in car, `3`/`4` enter/exit) switches first and stays consistent. |
| **Rotation zero and direction** | `rot=` at markers 1–3 | Raw value when facing up (Dege's formula `atan2(y, x) + 90` predicts 720 = 180° for "up"); whether holding left increases or decreases the value; wrap-around at 1440 (0–1439, i.e. degrees × 4). |
| **Analog speed chain** | `spd=` at marker 4 | The chain resolves (no `X`), the value changes with walking and is ~0 standing still; whether it scales with partial stick deflection. |
| **Stick Y sign** | `gi=... L x,y` and `xi=... L x,y` at marker 4 / while pushing up | Sign of Y when pushing up, for GInput (unknown, to determine) and raw XInput (positive-up by spec). Also the GInput stick range. |
| **Trigger range** | `gi=... L2= R2=` and `xi=... LT= RT=` at markers 6–7 | Min/max raw values and whether GInput reports analog or 0/255 digital values. |
| **Authoritative button store** | `rp=` (gReplay buttons), `pb=` (`m_bButton*`), `ctl=` (raw bytes) at markers 4–9 | Which store reflects the actions the game actually performs (e.g. does `pb` forward follow R2 in a car while `rp` does too?). The one that changes first and matches the on-screen behaviour is the override target for Phases 2–3. |
| **GInput's default L2/R2 mapping** | `rp`/`pb` while only R2 or only L2 is pressed (markers 6–7), on foot and in car | Which `CONTROLKEY_*` GInput drives from each trigger with `ControlSetP1 = 0`. |
| **Pause / menus** | `gs=` and gaps in `f=` at markers 11–12 | Whether the hook keeps running while paused (`gs=2`) and what the state looks like after quit-to-menu (Dege's "stuck up" bug). |
| **Replay** | `rep=` | Should stay `0`; any `1` means replay playback was active. |
| **Hook chaining with Frontend Fix** | `hook site` lines; Frontend Fix features still working | Our hook wraps Frontend Fix's: `before` target is in `FrontendFixII.asi` (or `?` if injector placed the stub outside any module), `after` target is in `GTA2ModernControls.asi` (same caveat); mouse/pause-menu features of Frontend Fix still work. |

Send the log back (or commit it under a git-ignored path) for analysis; the
conclusions feed the Phase 2/3 design decisions in `PLAN.md`.
