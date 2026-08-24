#ifndef OLED_SETUP_H_H
#define OLED_SETUP_H_H

#include "esp_err.h"

#include "driver/i2c_master.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

extern lv_disp_t *local_disp;

esp_err_t configure_oled_screen(i2c_master_bus_handle_t *i2c_bus);

#ifdef __cplusplus
}
#endif

#endif // OLED_SETUP_H_H
