# Plan de Port — Riptide GP (PS Vita)

> Generado por psvita-port-toolkit el 2026-09-30. Punto de partida con lo detectado automáticamente --
confirmar todo con objdump/Ghidra/jadx a mano antes de asumirlo como cierto.

## 0. Contexto

- **Juego:** Riptide GP
- **Paquete Java:** com.vectorunit.blue
- **APK original:** `Riptide-GP-v1-6-3.apk`
- **TITLEID asignado:** `PSVRGP001`

**¿Motor conocido?** Revisar si algún port hermano (bajo la misma BASE_DIR) comparte motor antes de
reusar su código -- confirmar con símbolos JNI reales, no por analogía superficial.

## 1. Detección automática

- **ABI(s):** armeabi-v7a, x86
- **ABI elegida:** armeabi-v7a
- **Nota de arquitectura:** armeabi-v7a presente -> ARMv7 (hard-float/NEON disponible). El CPU de Vita (Cortex-A9) corre esto sin traducción. Hay más de una ABI (armeabi-v7a, x86) -- se eligió armeabi-v7a para el análisis.
- **Versión de GLES:** valor no estándar en manifest: 0x20000 (declarado en AndroidManifest.xml)

## 2. .so encontrados (ABI armeabi-v7a)

- `riptidegp_extract/lib/armeabi-v7a/libBlue.so` (2528 KB)
- `riptidegp_extract/lib/armeabi-v7a/libfmodevent.so` (362 KB)
- `riptidegp_extract/lib/armeabi-v7a/libfmodex.so` (851 KB)


## 3. Exports JNI (convención `Java_*`)

- `Java_com_vectorunit_VuAdminHelper_onGetAchievementsAdd`
- `Java_com_vectorunit_VuAdminHelper_onGetAchievementsDone`
- `Java_com_vectorunit_VuAdminHelper_onGetHiddenPlayersAdd`
- `Java_com_vectorunit_VuAdminHelper_onGetHiddenPlayersDone`
- `Java_com_vectorunit_VuAdminHelper_onGetLeaderboardScoresAddRow`
- `Java_com_vectorunit_VuAdminHelper_onGetLeaderboardScoresDone`
- `Java_com_vectorunit_VuBlueGojiHelper_nativeOnConnection`
- `Java_com_vectorunit_VuBlueGojiHelper_nativeOnIntensity`
- `Java_com_vectorunit_VuBlueGojiHelper_nativeOnStep`
- `Java_com_vectorunit_VuBlueGojiHelper_onButtonP7_JNIEnvP8_jobjecthi`
- `Java_com_vectorunit_VuCloudSaveHelper_onLoadResult`
- `Java_com_vectorunit_VuGamePadHelper_onMogaAxisEvent`
- `Java_com_vectorunit_VuGamePadHelper_onMogaButtonEvent`
- `Java_com_vectorunit_VuGamePadHelper_onMogaConnectedP7_JNIEnvP8_jobject`
- `Java_com_vectorunit_VuHttpHelper_onDataReceived`
- `Java_com_vectorunit_VuHttpHelper_onFailure`
- `Java_com_vectorunit_VuHttpHelper_onSuccess`
- `Java_com_vectorunit_VuOnlineHelper_isInitialized`
- `Java_com_vectorunit_VuOnlineHelper_onGetScoresFailure`
- `Java_com_vectorunit_VuOnlineHelper_onGetScoresSuccessAddRow`
- `Java_com_vectorunit_VuOnlineHelper_onGetScoresSuccessBegin`
- `Java_com_vectorunit_VuOnlineHelper_onGetScoresSuccessEnd`
- `Java_com_vectorunit_VuOnlineHelper_onRefreshAchievementResult`
- `Java_com_vectorunit_VuOnlineHelper_onRefreshAchievementsDone`
- `Java_com_vectorunit_VuOnlineHelper_onSignIn`
- `Java_com_vectorunit_VuOnlineHelper_onSignOut`
- `Java_com_vectorunit_VuOnlineHelper_onUnlockAchievementResult`
- `Java_com_vectorunit_blue_Blue_setInternalDataPath`


## 4. Checklist

- [x] Repo creado desde soloader-boilerplate, git init, .gitignore anti-DMCA.
- [x] APK decompilado (jadx) y .so decompilado(s) (Ghidra) -- ver sección 2/3.
- [x] Análisis del motor real (ciclo de vida nativo, reuso de otro port o boilerplate genérico).
- [x] Bootstrap del loader: so_file_load/so_relocate/so_resolve, primer build (debug OK 2026-09-30).
- [x] Tabla JNI (FalsoJNI): registrar exports + callbacks hacia "Java" (auditada contra strings del .so).
- [ ] Primer arranque en consola real.
- [x] Gráficos (wrappers GL según versión detectada) -- estático; validar en consola.
- [x] Input, Audio, Assets, LiveArea/VPK -- estático; validar en consola.
- [ ] Pruebas en hardware real.

## 5. Herramientas

Este port se gestiona con **psvita-port-toolkit** (standalone, fuera de este repo). Desde el
toolkit: `Continuar con un port existente` → elegí esta carpeta (ya tiene `.psvita-toolkit.json`).

## 6. Estándar de logs en consola

El log del juego vive en `<DATA_PATH>logs/<slug>_NNN.log` (`<slug>` = riptidegp), incremental de
001 a 999 con `next.idx` (nunca `log_<timestamp>.txt`, nunca base 000, nunca ruta hardcodeada:
usar `DATA_PATH`). La subida FTP crea `logs/` y `saves/` en la consola aunque estén vacíos.
Comandos del toolkit (sin IA, deterministas): `psvita-toolkit log-standard --fix-dirs` (audita
y crea los dirs), `psvita-toolkit log-trace --ensure-flag` (inyecta trazas `[TRACE] >> f()` por
función bajo `#ifdef PORT_TRACE`; compilar con `-DPORT_TRACE=ON` para debug, `OFF` para
producción, o `psvita-toolkit log-trace --remove` para quitarlas del árbol).
