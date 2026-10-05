# Riptide GP — PS Vita · v1.0.1

> Paste the section below into the GitHub release description. The title above goes in the
> release-title field; suggested tag: `v1.0.1`.

---

**Your progress is now saved — quit and continue a championship where you left off —
and tilt steering works with the Vita's accelerometer. Locked 60 FPS.**

## What's new in 1.0.1

- **Progress is kept between launches.** v1.0.0 wrote the profile but never managed to read it
  back, so every launch started a fresh profile and overwrote the old one. The engine read the
  save file's size through a field that only exists in Android's `FILE` struct, got 0 and treated
  the profile as empty. The loader now measures the file size itself (hook on
  `VuGenericFile::size`).
- **Accelerometer.** The Vita's motion sensor now feeds the game's accelerometer, so the
  tilt control method steers by tilting the console (confirmed on real hardware).
  Configurable in `config.txt` (`accelerometer`, `invert_tilt`).
- `AMotionEvent_getAxisValue` implemented (it was the last unresolved symbol in the log).
- Your save lives in `ux0:data/riptidegp/files/profile`. A `saves/` folder is not needed; you can
  keep a manual backup copy of `profile` anywhere (e.g. `ux0:data/riptidegp/saves/`) and copy it
  back to `files/` with VitaShell while the game is closed.

**Updating from 1.0.0:** install the new `riptidegp.vpk` over the old one; the data files in
`ux0:data/riptidegp/` stay as they are. Progress made on 1.0.0 was being reset on every launch,
so it cannot be recovered; from 1.0.1 on, it is kept.

## Requirements

- PS Vita / PS TV with HENkaku/Enso (3.60–3.74 with a kernel plugin loader).
- `kubridge.skprx` in `*KERNEL` of your taiHEN config.
- `libshacccg.suprx` in `ur0:data/` (use ShaRKBR33D).
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
