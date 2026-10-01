/**
 * @file  controls.h
 * @brief Vita touch + buttons -> engine input.
 *
 * - Front touch panel -> AInputQueue motion events (menus are touch driven).
 * - Buttons / sticks  -> the engine's MOGA gamepad path
 *   (Java_com_vectorunit_VuGamePadHelper_onMogaButtonEvent / onMogaAxisEvent),
 *   the same entry points the Java MOGA ControllerListener fed on Android
 *   (see jadx com/vectorunit/VuGamePadHelper.java).
 */

#ifndef SOLOADER_CONTROLS_H
#define SOLOADER_CONTROLS_H

#include <so_util/so_util.h>

void controls_init(so_module *mod);
/** Poll hardware once and forward changes to the engine. */
void controls_poll(void);

#endif // SOLOADER_CONTROLS_H
