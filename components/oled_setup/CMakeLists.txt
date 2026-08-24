idf_component_register(SRCS "oled_setup.c" INCLUDE_DIRS . REQUIRES lvgl esp_timer driver esp_driver_gpio esp_driver_i2c esp_lcd esp_lvgl_port nvs_flash)
