# Modern controls — play-test checklist (Steam Deck, Game mode)

Covers vehicle controls, on-foot absolute movement, the ini and the in-game
toggle. Play a few normal games; no markers needed. Note anything odd with the
situation it happened in (on foot / in a car, mission, what you pressed).

## Deploy

```sh
cd ~/Games/gta2/game/scripts
curl -fsSL --remote-name-all \
  https://github.com/MagiCrazy/gta2-modern-controls/releases/download/dev/GTA2ModernControls.asi \
  https://github.com/MagiCrazy/gta2-modern-controls/releases/download/dev/SHA256SUMS
sha256sum -c SHA256SUMS && rm SHA256SUMS
```

`GTA2ModernControls.ini` is created next to the `.asi` on first launch.

## Mapping (Deck labels)

| Button | On foot | In a car |
|---|---|---|
| Left stick / D-pad | Walk towards that direction | Steer |
| R2 | Attack | Accelerate |
| L2 | Special 2 | Brake / reverse |
| A | Jump | Handbrake |
| X | — | Attack |
| L1 / R1 | Previous / next weapon | Previous / next weapon |
| Y | Enter / exit | Enter / exit |
| B | Special | Special (horn) |
| L3 | Special 2 | Special 2 |
| Select + R3 | Toggle modern / classic | Toggle modern / classic |

## Checklist

On foot
- [ ] Each of the 8 directions walks that way on screen; releasing the stick stops.
- [ ] Reversing direction turns quickly without walking the wrong way first.
- [ ] Small stick movements do not make the player jitter in place.
- [ ] Jump (A), attack (R2), weapon switch (L1 / R1) work, also while walking.
- [ ] Entering / leaving a car (Y) still works; nothing odd during the animation.
- [ ] Falling, dying, getting arrested: no stuck movement afterwards.

In a car
- [ ] R2 accelerates, L2 brakes then reverses, half-pressed triggers count.
- [ ] Steering with the stick and the D-pad while accelerating.
- [ ] Handbrake (A), horn (B), attack (X) on an armed vehicle.

General
- [ ] Select + R3 shows "Modern controls" / "Classic controls" and switches
      behaviour immediately; the choice survives a restart (`Enabled` in the ini).
- [ ] Pause menu: the player does not move while paused; Frontend Fix's pause
      menu still works.
- [ ] Cutscenes / scripted sequences: the player does not move on their own.
- [ ] Quit to the main menu and start again: no stuck movement.

## Tuning

`GTA2ModernControls.ini`:

| Key | Default | Effect |
|---|---|---|
| `[OnFoot] TurnStep` | 96 | Max turn per frame (degrees x 4). Higher = snappier. |
| `[OnFoot] WalkTolerance` | 180 | How well aligned the player must be before walking (degrees x 4). |
| `[Vehicle] TriggerThreshold` | 40 | Trigger travel (0-255) that counts as pressed. |
| `[General] ProbeLog` | 1 | Diagnostics log; send `GTA2ModernControls.log` with any report. |
