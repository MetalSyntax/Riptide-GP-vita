# Riptide GP — PS Vita · v1.0.3

> Paste the section below into the GitHub release description. The title above goes in the
> release-title field; suggested tag: `v1.0.3`.

---

**Google Play leftovers replaced offline: no more Sign In button, achievements and records
screens work.**

## What's new in 1.0.3

- **Google Sign In button removed** from the bottom-left of the main menu. The game now
  signs in "offline" with your PS Vita username.
- **Achievements screen opens** from the menu (the game's own list, with your progress).
- **Leaderboard shows your personal records.** Without online servers, the table lists your
  best time on every event you have completed (Race = total time, Hot Lap = best lap).
- Trophies from 1.0.2 keep working as before.

**Updating from 1.0.2:** install the new `riptidegp.vpk` over the old one. Data files and your
save stay as they are.

## Requirements

- PS Vita / PS TV with HENkaku/Enso (3.60–3.74 with a kernel plugin loader).
- `kubridge.skprx` in `*KERNEL` of your taiHEN config.
- `libshacccg.suprx` in `ur0:data/` (use ShaRKBR33D).
- Optional, for trophies: `NoTrpDrm.suprx` in `*main`.
- Your own copy of the game APK (Android 1.6.3, `armeabi-v7a`).

## Install

1. Install `riptidegp.vpk` with VitaShell.
2. Create `ux0:data/riptidegp/` and copy into it:
   - `libBlue.so`, `libfmodex.so`, `libfmodevent.so` from the APK's `lib/armeabi-v7a/`
   - `assets/Base.apf` from the APK's `assets/`
3. Launch.

## What works

- Boot through the NativeActivity lifecycle (`OnInitApp`/`OnInitWindow`), full asset loading
  from `assets/Base.apf` (textures, shaders, sounds).
- All 57 shader programs compile and link on real hardware (confirmed in the console
  log, zero link failures, zero crashes across the run).
- Main menu and racing, confirmed playable on real hardware.
- FMOD music and sound effects (AudioTrack output).
- Progress saved and restored across launches (`ux0:data/riptidegp/files/profile`): continue
  championships, keep unlocked boats and upgrades.
- Locked 60 FPS in races.
- Tilt steering with the Vita's accelerometer.
- Native PS Vita trophies (29 + Platinum).
- Offline achievements screen and personal-records leaderboard (no Google Play).
- Logs per run in `ux0:data/riptidegp/logs/`.

## Controls

| Vita | Action |
|---|---|
| Touch screen | Original touch controls |
| Tilt the console | Accelerometer (tilt steering) |
| Left / Right sticks | MOGA analog axes |
| Cross / Circle / Square / Triangle | MOGA A / B / X / Y |
| L / R (+ L2 / R2 on PSTV/DS4) | MOGA shoulder buttons / trigger axes |
| D-pad, Select, Start, L3, R3 | MOGA D-pad, Select, Start, thumb buttons |

## Options

`ux0:data/riptidegp/config.txt` (one `key value` per line):

- `accelerometer 1` — Vita motion sensor drives tilt steering (`0` = no tilt).
- `invert_tilt 0` — `1` swaps left/right tilt.

## Under the hood

Three bugs stood between the APK and a fully playable game, each found on real hardware:

- **Crash in `glBindAttribLocation` on the first shader.** The engine binds its ~10 attribute
  locations right after `glCreateProgram()`, before attaching any shader -- legal GLES2, but
  vitaGL dereferences the program's vertex shader without a `NULL` check (upstream has the same
  bug). The loader now queues those bindings and replays them inside `glLinkProgram`, where the
  shaders are already attached; a link failure is reported with the program info log.
- **Permanent black screen with music playing.** The prebuilt `libvitaGL.a` in VitaSDK is not
  built with `SOFTFP_ABI=1`, but the loader is softfp (the Android `.so` requires it), so floats
  crossing loader<->vitaGL arrived garbled. vitaGL is now vendored (`vendor/vitaGL`, same tree as
  the Carnivores Vita ports) and built with the softfp ABI. Same root cause as Carnivores
  logs 004-007.
- **Progress reset on every launch (fixed in 1.0.1).** `VuGenericFile::size()` inlines
  `fileno()` as a read of bionic's `FILE::_file` (offset `0x0E`) and passes it to `fstat()`. The
  loader's `FILE*` comes from SceLibc with a different layout, so it always got fd 0, `fstat`
  failed and the profile looked empty. `source/patch.c` hooks the function and computes the size
  with `ftell`/`fseek`.

## Known issues

- **Physical buttons and sticks only work with the *Simple* control method.** With tilt
  steering selected in the game's options, tilting steers the boat but the buttons/sticks stop
  responding. Switch back to *Simple* to use them.

Report anything else you find with the log from `ux0:data/riptidegp/logs/`.
