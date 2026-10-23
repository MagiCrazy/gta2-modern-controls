# Release checklist (Steam Deck, Game mode)

Run once on each release candidate (`vX.Y.Z-rcN`) before tagging the final
version. Controls are described in the [README](../README.md#controls). Play a
few normal games and note anything odd with the situation it happened in (on
foot / in a vehicle, mission, what you pressed).

## Deploy the candidate

```sh
tag=v1.0.0-rc1
cd ~/Games/gta2/game/scripts
curl -fsSL --remote-name-all \
  https://github.com/MagiCrazy/gta2-modern-controls/releases/download/$tag/GTA2ModernControls.asi \
  https://github.com/MagiCrazy/gta2-modern-controls/releases/download/$tag/SHA256SUMS
sha256sum -c SHA256SUMS && rm SHA256SUMS
```

To check the defaults, delete `GTA2ModernControls.ini` first (it is recreated
on launch). Set `ProbeLog=1` in it to get a log to attach to a report; its
first line shows the version.

## On foot

- [ ] Each of the 8 directions walks that way on screen; releasing the stick stops.
- [ ] Reversing direction turns quickly without walking the wrong way first.
- [ ] Small stick movements do not make the player jitter in place.
- [ ] Jump (A), attack (R2), weapon switch (D-pad ← / →, once per press) work,
      also while walking.
- [ ] Entering / leaving a vehicle (Y) works; nothing odd during the animation.
- [ ] R2 during the enter animation does not shoot; it accelerates once seated.
- [ ] Leaving a vehicle with R2 held does not shoot; releasing then pressing R2 does.
- [ ] Falling, dying, getting arrested: no stuck movement afterwards.

## Aiming and camera

- [ ] Standing still, the right stick turns the player to face each of the
      8 directions (up on the stick = up on screen); R2 shoots that way.
- [ ] Walking one way while aiming another: shots go where the right stick
      points, the player keeps walking where the left stick points.
- [ ] Releasing the right stick: the player faces the walking direction again.
- [ ] Throwing weapons (grenades, molotovs) go towards the aim.
- [ ] On foot, aiming keeps the camera still.
- [ ] On foot, L2 + right stick looks around, diagonals reach the corners;
      releasing L2 recentres smoothly and aiming resumes.
- [ ] In a vehicle, the right stick looks around (also while driving),
      diagonals reach the corners; releasing it recentres smoothly.

## Vehicles

- [ ] R2 accelerates, L2 brakes then reverses, half-pressed triggers count.
- [ ] Steering with the left stick while accelerating; D-pad ← / → switch the
      vehicle weapon instead of steering.
- [ ] Handbrake (A), attack (X) on an armed vehicle.
- [ ] Horn: holding B honks. Police car / ambulance: a short tap toggles the
      siren on release; holding B with the siren off honks, with the siren on
      it changes the siren tone.
- [ ] D-pad ↑ / ↓ in a civilian car change the radio station; nothing in a
      police car.
- [ ] Turret (tank, fire truck, gun jeep): right stick ← / → rotates it at the
      game's own speed while still steering; releasing keeps its angle; X fires
      where it points; B + left / right still works.

## General

- [ ] L3 repeats the last mission message; pressing again shows older ones.
- [ ] R3 shows the district name.
- [ ] Select + R3 shows "Modern controls" / "Classic controls" and switches
      immediately; the choice survives a restart.
- [ ] Classic mode behaves like the game without the plugin.
- [ ] Pause menu: the player does not move while paused; Frontend Fix's pause
      menu still works.
- [ ] Cutscenes / scripted sequences: the player does not move on their own.
- [ ] Quit to the main menu and start again: no stuck movement.
- [ ] No `GTA2ModernControls.log` is written with the default ini.
