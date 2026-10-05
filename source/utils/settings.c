/*
 * Copyright (C) 2021      Andy Nguyen
 * Copyright (C) 2022-2023 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#include <stdio.h>
#include <string.h>
#include "settings.h"

#define CONFIG_FILE_PATH DATA_PATH"config.txt"

bool setting_accelerometer;
bool setting_invertTilt;

void settings_reset() {
    setting_accelerometer = true;
    setting_invertTilt    = false;
}

void settings_load() {
    settings_reset();

    char buffer[30];
    int value;

    FILE *config = fopen(CONFIG_FILE_PATH, "r");

    if (config) {
        while (EOF != fscanf(config, "%[^ ] %d\n", buffer, &value)) {
            if      (strcmp("accelerometer", buffer) == 0) setting_accelerometer = (bool)value;
            else if (strcmp("invert_tilt", buffer) == 0)   setting_invertTilt    = (bool)value;
        }
        fclose(config);
    }
}

void settings_save() {
    FILE *config = fopen(CONFIG_FILE_PATH, "w+");

    if (config) {
        fprintf(config, "%s %d\n", "accelerometer", (int)(setting_accelerometer));
        fprintf(config, "%s %d\n", "invert_tilt", (int)(setting_invertTilt));
        fclose(config);
    }
}
