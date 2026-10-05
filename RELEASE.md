# Riptide GP — PS Vita · v1.0.0 "First Ride"

> Paste the section below into the GitHub release description. The title above goes in the
> release-title field; suggested tag: `v1.0.0`.

---

**First public build: the Android version of Riptide GP (1.6.3) runs on PS Vita —
menus and racing, with FMOD sound.**

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
- Saved profile (`ux0:data/riptidegp/files/profile`) is written (but see Known issues: it is
  not read back in this build).
- Logs per run in `ux0:data/riptidegp/logs/`.

## Controls

| Vita | Action |
|---|---|
| Touch screen | Original touch controls |
| Left / Right sticks | MOGA analog axes |
| Cross / Circle / Square / Triangle | MOGA A / B / X / Y |
| L / R (+ L2 / R2 on PSTV/DS4) | MOGA shoulder buttons / trigger axes |
| D-pad, Select, Start, L3, R3 | MOGA D-pad, Select, Start, thumb buttons |

## Options

None yet (`config.txt` only carries boilerplate placeholder keys).

## Under the hood

Two bugs stood between the APK and a running game, each found on real hardware:

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

## Known issues

- **Progress is not kept between launches.** The profile is written, but the engine reads its
  size through a bionic-only `FILE` field, gets 0 and starts a fresh profile, overwriting the old
  one. Fixed after v1.0.0 (hook on `VuGenericFile::size` in `source/patch.c`).
- Frame rate has not been measured yet; long-session stability is unconfirmed.
- `AMotionEvent_getAxisValue` is not implemented (accelerometer sensor reports "not
  supported", which the engine already handles as a no-motion device).
