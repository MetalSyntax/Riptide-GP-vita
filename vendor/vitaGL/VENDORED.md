# vitaGL vendorizado en este repo

Upstream: https://github.com/Rinnegatamante/vitaGL
Commit base: cd3791e29ff7f1c0ab349f12c7231f4871ce6a75
Copiado de: `Carnivores-Dinosaur-Hunter-vita/vendor/vitaGL` (2026-10-01;
árbol idéntico al de `Carnivores-Ice-Age-vita`, ambos probados en consola
real). Ver su `VENDORED.md` para el diff completo contra upstream
(`source/shared.h`: defines `SCE_GXM_*` que faltan en este vitasdk;
`source/vgl.c`: guard `HAVE_RAZOR`; `Makefile`: fallback de git hash).

## Por qué vendorizado y no la lib del SDK

La `libvitaGL.a` prebuilt del SDK **no** está compilada con `SOFTFP_ABI=1`
mientras que este loader compila con `-mfloat-abi=softfp` (obligatorio: el
`.so` de Android es ABI softfp y el loader lo llama directo). El desajuste
de ABI corrompe los argumentos float que cruzan loader<->vitaGL y el
resultado es **pantalla negra permanente** con el juego corriendo (música,
sin crash) -- mismo síntoma que Carnivores logs 004-007.

Este árbol se compila desde `CMakeLists.txt` (`vitaGL_lib`) con:

```
SOFTFP_ABI=1 NO_SPLASHSCREEN=1 NO_DEBUG=1 HAVE_SHADER_CACHE=1
```

## Diferencia con Carnivores

Su `VENDORED.md` dice que `source/egl.c` está eliminado, pero el árbol
copiado **sí lo trae** (con su `egl.o`). En este port se conserva a
propósito: `source/reimpl/egl.h` renombra nuestras 14 reimplementaciones a
`eglX_soloader` (`dynlib.c` las exporta al `.so` como `"eglX"`), así que no
hay símbolos duplicados y el path EGL queda igual que con la lib del SDK.

## Notas

- `samples/` no existe en este árbol (solo lo usa `make samples`).
- Artefactos de build (`source/**/*.o`, `libvitaGL.a`, `*.stamp`) están
  gitignorados: los genera `build_vitagl.sh` + `make` desde CMakeLists.txt.
  `build_vitagl.sh` hace `make clean` solo cuando cambian los flags.
- NO activar `DRAW_SPEEDHACK`: en Carnivores provocaba GPUCRASH con meshes
  grandes (pasa memoria del juego directo a la GPU sin copiar).
