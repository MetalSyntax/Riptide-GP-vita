# Registro de Progreso — Riptide GP (PS Vita)

## Fase 1: Configuración y Preparación (Completada — 2026-09-30)
- Repo creado desde soloader-boilerplate, `.gitignore` anti-DMCA.
- APK `Riptide-GP-v1-6-3.apk` copiado y extraído.
- ABI detectada: armeabi-v7a, x86 (elegida: armeabi-v7a).
- GLES detectado: valor no estándar en manifest: 0x20000 (declarado en AndroidManifest.xml)

## Fase 2: Decompilación (Completada — 2026-09-30)
- jadx: corrido -> `decompiled/apk_jadx/` (com.vectorunit.* + com.vectorunit.blue.Blue).
- objdump: `decompiled/objdump/` (dynsyms de libBlue, `ANativeActivity_onCreate.s`, `android_main.s`).
- Ghidra (.so): NO corrido (docker/imagen no disponibles) -- pendiente, correr cuando haga falta para triage.

## Fase 3: Análisis del Motor Real (Completada — 2026-09-30)
- Motor propio de Vector Unit (clases `Vu*`), no comparte motor con ningún port hermano.
- libBlue.so es NativeActivity con android_native_app_glue estático: exporta
  `ANativeActivity_onCreate` + `android_main`, sin `JNI_OnLoad`. Único JNI llamado desde Java
  en el arranque: `Blue.setInternalDataPath(getFilesDir())`.
- Dependencias: libfmodex.so -> libfmodevent.so -> libBlue.so (orden de carga en `utils/init.c`).
- Assets: un único `assets/Base.apf` (37 MB), servido por `reimpl/asset_manager.cpp` desde
  `DATA_PATH assets/`.

## Fase 4: Bootstrap del loader (Completada — 2026-09-30, sin probar en consola)
- Loader escrito: `main.c`, `reimpl/native_activity.c` (ciclo de vida + hilo glue),
  `reimpl/looper.c`, `input_queue.c`, `sensor.c`, `audio.c` (FMODAudioDevice), `controls.c`, `java.c`.
- **Bug de link #1 (confirmado):** el `libvitaGL.a` instalado en `$VITASDK` trae su propio
  `egl.o` (se recompiló desde el scratchpad de Sacred-Odyssey), que choca con
  `source/reimpl/egl.c` -> `multiple definition of eglInitialize` y 13 más.
  Fix: `reimpl/egl.h` renombra por macro las 14 reimplementaciones a `eglX_soloader`;
  `dynlib.c` las sigue exportando al .so como `"eglX"`. `eglGetDisplay`/`eglSwapBuffers`/
  `eglGetError`/`eglBindAPI`/`eglGetProcAddress` vienen de vitaGL. Verificado con `nm`.
- **Bug de link #2 (confirmado):** `utf16_to_utf8`/`utf8_to_utf16` sin definir --
  faltaba `lib/falso_jni/converter.c` en `CMakeLists.txt`. Agregado.
- Primer build debug OK: `build/eboot.bin`, `build/riptidegp.vpk`.

## Fase 5: Auditoría estática pre-consola (Completada — 2026-09-30)
- **Imports:** los `*UND*` de libBlue/libfmodex/libfmodevent resuelven todos contra
  `default_dynlib` + exports cruzados (0 faltantes). Todos los `so_symbol()` del loader existen.
- **dlsym runtime:** `NvInputInit` (@0x110d40) hace `dlopen("libandroid.so")` +
  `dlsym("AMotionEvent_getAxisValue")`; NULL está manejado (loguea "Not supported"), se deja así.
- **eglGetProcAddress:** el motor pide `eglGetSystemTime[Frequency]NV` y, si
  `GL_OES_get_program_binary` está anunciada (vitaGL la anuncia), `glGetProgramBinaryOES`/
  `glProgramBinaryOES` (`VuAndroidOglesGfx::init` @0x19402a, gateado por un flag de config).
  Nuevo `eglGetProcAddress_soloader` en `dynlib.c`: busca primero en `default_dynlib` (aliases
  OES + wrappers GL) y recién después en vitaGL. Agregados los 4 nombres a la tabla.
- **JNI:** cruzados todos los `GetMethodID` de libBlue (strings + firmas). Faltaban 6 void:
  `gameInitialize`, `resetAchievement`, `resetAllAchievements`, `resetLeaderboardScores`,
  `hidePlayer`, `unhidePlayer` -> agregados como no-op en `java.c`.
  Firmas MOGA (`onMogaButtonEvent(int,int,boolean)`, `onMogaAxisEvent(int,6xfloat)`)
  coinciden con `VuGamePadHelper.java`.
- **Hilos:** `pthread_create_soloader` fuerza 1 MB de stack (igual que Android) -> alcanza para
  el hilo de `android_main`.
- **vitaGL instalado:** trae traductor GLSL + shader cache (`ux0:data/shader_cache/`), ABI softfp.
- **LiveArea:** reemplazados los placeholders de Android por arte real extraído de `Base.apf`
  (formato APF: header `FPUV` v3, directorio `nombre\0 + off,usize,csize,flags,hash,1`,
  payload zlib; `VuTextureAsset` = header 65 B con w/h @20/24 y `0x83F3` DXT5, filas invertidas).
  bg0/pic0 = `UI/MenuIcon_Track_CityA` + `UI/Title`, startup = `UI/MenuIcon_JetSki_A`,
  icon0 = `res/drawable-xhdpi/icon.png` escalado. `psvita-toolkit livearea --validate` OK.
- **Logs:** `ENABLE_FILE_LOG` (CMake, ON por defecto) como flag de producción;
  `LOG_INDEX_MIN/MAX` explícitos. `psvita-toolkit log-standard` OK.
- **Staging de datos:** `ux0_data/riptidegp/` (gitignored) con los 3 .so, `assets/Base.apf`,
  `files/`, `obb/`, `logs/`, `saves/`. Script `extras/scripts/upload_data.sh <ip>` lo sube por FTP.
- Builds debug y release limpios (`--clean`) OK.

## Fase 6: Primer arranque en consola real (Pendiente — único paso restante)
1. `psvita-toolkit deploy --vpk` e instalar `riptidegp.vpk` desde VitaShell.
2. `extras/scripts/upload_data.sh 192.168.3.15` (sube ux0_data/riptidegp/ a ux0:data/riptidegp/).
3. Requisito: `libshacccg.suprx` en `ur0:data/` (el loader aborta con diálogo si falta).
4. Lanzar, traer `ux0:data/riptidegp/logs/riptidegp_NNN.log` y el `.psp2dmp` si crashea
   -> triage con `so-crash-triage`.

## Fase 7 en adelante: Pendiente
Ver PORTING_PLAN.md sección 4. Actualizar con un bug confirmado a la vez en pruebas reales.

## Fase 8: Bug de consola #1 — glBindAttribLocation con vshader NULL (confirmado 2026-10-01)
- **Log:** `logs/riptidegp_001.log` (corte tras 3er `AAssetManager_open(Base.apf)`).
- **Dump:** `logs/riptidegp-psp2core-1790829328-0x0001a124b1-eboot.bin.psp2dmp`.
- **Crash:** Data abort en `glBindAttribLocation` de vitaGL (`custom_shaders.o` +0x30:
  `ldrb r3, [r0, #6]` con `r0 = 0`). LR `0x98993029` = libBlue
  `VuOglesShaderProgram::createProgram()` +0x5c: primer `glBindAttribLocation`
  de un loop de 10 que corre ANTES de los 2 `glAttachShader` (ver desensamblado
  Thumb en `0x192fcc`: binds en `0x193008-0x19302a`, attaches en `0x193030+`).
- **Causa raíz:** el motor bindea attribs justo después de `glCreateProgram(id=1`,
  confirmado: `R6 = prog-1 = 0`, `R4 = R5 = base de la tabla`) y antes de
  attachear shaders -- GLES2 legal (los binds latchan en el link), pero vitaGL
  desreferencia `p->vshader` (`->is_glsl`, luego `->prog`) sin chequeo de NULL.
  El bug existe igual en vitaGL upstream (master `custom_shaders.c`, sin guard).
  Modo `VGL_MODE_POSTPONED` confirmado activo (contador interno = 2 en el dump).
- **Fix (loader, sin tocar vitaGL):** `source/utils/glutil.c`
  `glBindAttribLocation_soloader()` encola (prog, index, name) (máx 32);
  `glLinkProgram_soloader()` los re-ejecuta justo antes del `glLinkProgram`
  real (shaders ya attacheados -> vitaGL los toma por su ruta POSTPONED normal)
  y loguea el info-log si el link falla; `glDeleteProgram_soloader()` purga la
  cola para que un id de programa reciclado nunca herede binds ajenos.
  `dynlib.c` remapea los 3 símbolos a los wrappers. Además
  `vglSetSemanticBindingMode(VGL_MODE_POSTPONED)` se movió de `gl_preload()`
  (pre-`vglInit`, frágil) a `gl_init()` (post-`vglInit`).
- **Estado:** build debug OK 2026-10-01, `eboot.bin` (3 símbolos `_soloader`
  verificados con `nm`) subido por FTP a la consola (`deploy --eboot` OK).
  El lanzamiento remoto por `nc 1338` no responde (rc=1) -> **pendiente que el
  usuario lance el juego en la consola y traiga `logs/riptidegp_002.log`** (+
  `.psp2dmp` si vuelve a crashear) para confirmar el fix / triage del siguiente bug.

## Fase 9: pantalla negra con música (log 002) -> vitaGL vendorizada (2026-10-01)
- **Síntoma (log `riptidegp_002.log`, 36 programas `glLinkProgram OK`, sin
  crash):** el juego corre (música FMOD, profile guardado) pero pantalla negra.
- **Causa:** la `libvitaGL.a` prebuilt del SDK no tiene `SOFTFP_ABI` y el
  loader compila con `-mfloat-abi=softfp` (obligatorio por el .so Android) --
  los floats loader<->vitaGL se corrompen. Mismo síntoma y misma causa que
  Carnivores-Dinosaur-Hunter logs 004-007 (ver su `CMakeLists.txt`).
- **Fix:** `vendor/vitaGL/` copiado verbatim de
  `Carnivores-Dinosaur-Hunter-vita` (idéntico al de Ice-Age, upstream
  cd3791e + mods vitasdk; ver `vendor/vitaGL/VENDORED.md`). `CMakeLists.txt`
  la compila con `SOFTFP_ABI=1 NO_SPLASHSCREEN=1 NO_DEBUG=1
  HAVE_SHADER_CACHE=1` (target `vitaGL_lib`), `include_directories(BEFORE)`
  para su `vitaGL.h`, y linkea el `.a` vendorizado en vez de `-lvitaGL`.
  Diferencia con Carnivores: se conserva `source/egl.c` (no hay duplicados
  porque `reimpl/egl.h` renombra lo nuestro a `eglX_soloader`).
  `.gitignore`: `vendor/vitaGL/**/*.o`, `vendor/vitaGL/libvitaGL.a`;
  `/Makefile` anclado a raíz para no ignorar `vendor/vitaGL/Makefile`.
- **Estado:** build debug OK, `eboot.bin` (595.7 KB) subido por FTP.
  **Pendiente test en consola: lanzar y traer `logs/riptidegp_003.log`** (+
  `.psp2dmp`/screenshot según lo que se vea).
