#include "imu_config.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>

#include "esp_err.h"
#include "esp_log.h"
#include "sdkconfig.h"

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "bno085.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef CONFIG_BNO085_I2C_ADDR
#define CONFIG_BNO085_I2C_ADDR 0x4A
#endif

#ifndef CONFIG_BNO085_I2C_PORT
#define CONFIG_BNO085_I2C_PORT 1
#endif

#ifndef CONFIG_BNO085_SDA_GPIO
#define CONFIG_BNO085_SDA_GPIO 6
#endif

#ifndef CONFIG_BNO085_SCL_GPIO
#define CONFIG_BNO085_SCL_GPIO 7
#endif

#ifndef CONFIG_BNO085_RESET_GPIO
#define CONFIG_BNO085_RESET_GPIO 4
#endif

#ifndef CONFIG_BNO085_INT_GPIO
#define CONFIG_BNO085_INT_GPIO 5
#endif

#ifndef CONFIG_BNO085_REPORT_INTERVAL_US
#define CONFIG_BNO085_REPORT_INTERVAL_US 10000
#endif

static const char TAG[] = "imu_config";

static TaskHandle_t imu_task_handle = NULL;
static SemaphoreHandle_t imu_mutex = NULL;
static bno085_handle_t s_bno085_handle = NULL;

static float g_angle_x = 0.0f;
static float g_angle_y = 0.0f;
static float g_angle_z = 0.0f;
static bool g_has_angle_reading = false;

static float g_acc_x = 0.0f;
static float g_acc_y = 0.0f;
static float g_acc_z = 0.0f;
static bool g_has_acc_reading = false;

static void bno085_event_callback(bno085_handle_t handle, const bno085_sensor_value_t *value, void *user_context) {
    if (value == NULL) {
        return;
    }

    if (value->sensor_id == BNO085_SENSOR_ROTATION_VECTOR) {
        float roll_rad = 0.0f, pitch_rad = 0.0f, yaw_rad = 0.0f;
        bno085_quaternion_to_euler(value->data.quaternion.i,
                                   value->data.quaternion.j,
                                   value->data.quaternion.k,
                                   value->data.quaternion.real,
                                   &roll_rad, &pitch_rad, &yaw_rad);

        float roll = roll_rad * (180.0f / (float)M_PI);
        float pitch = pitch_rad * (180.0f / (float)M_PI);
        float yaw = yaw_rad * (180.0f / (float)M_PI);

        if (imu_mutex != NULL && xSemaphoreTake(imu_mutex, 0) == pdTRUE) {
            g_angle_x = roll;
            g_angle_y = pitch;
            g_angle_z = yaw;
            g_has_angle_reading = true;
            xSemaphoreGive(imu_mutex);
        }
    } else if (value->sensor_id == BNO085_SENSOR_ACCELEROMETER) {
        if (imu_mutex != NULL && xSemaphoreTake(imu_mutex, 0) == pdTRUE) {
            g_acc_x = value->data.accelerometer.x;
            g_acc_y = value->data.accelerometer.y;
            g_acc_z = value->data.accelerometer.z;
            g_has_acc_reading = true;
            xSemaphoreGive(imu_mutex);
        }
    }
}

static void imu_task_code(void *pvParameter) {
    bno085_handle_t handle = (bno085_handle_t)pvParameter;

    while (1) {
        if (handle != NULL) {
            bno085_service(handle);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void imu_config_init(void *unused) {
    ESP_LOGI(TAG, "Initializing IMU with rinku404/bno085 driver...");

    if (imu_mutex == NULL) {
        imu_mutex = xSemaphoreCreateMutex();
    }

    // Configure dedicated I2C master bus for BNO085
    i2c_master_bus_config_t bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = CONFIG_BNO085_I2C_PORT,
        .scl_io_num = CONFIG_BNO085_SCL_GPIO,
        .sda_io_num = CONFIG_BNO085_SDA_GPIO,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus_handle = NULL;
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &bus_handle));

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = CONFIG_BNO085_I2C_ADDR,
        .scl_speed_hz = 400000,
    };
    i2c_master_dev_handle_t dev_handle = NULL;
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle));

    bno085_config_t config;
    bno085_config_default(&config);

    esp_err_t err = ESP_FAIL;
    const int max_attempts = 3;

    for (int attempt = 1; attempt <= max_attempts; attempt++) {
        err = bno085_init(&config, dev_handle, (gpio_num_t)CONFIG_BNO085_INT_GPIO,
                         (gpio_num_t)CONFIG_BNO085_RESET_GPIO, &s_bno085_handle);
        if (err == ESP_OK) {
            break;
        }
        ESP_LOGW(TAG, "BNO085 init attempt %d/%d failed: %s", attempt, max_attempts,
                 esp_err_to_name(err));
        if (attempt < max_attempts) {
            vTaskDelay(pdMS_TO_TICKS(500));
        }
    }

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize BNO085: %s", esp_err_to_name(err));
        return;
    }

    ESP_ERROR_CHECK(bno085_register_sensor_callback(s_bno085_handle, bno085_event_callback, NULL));

    // Enable rotation vector report
    err = bno085_enable_sensor(s_bno085_handle, BNO085_SENSOR_ROTATION_VECTOR, CONFIG_BNO085_REPORT_INTERVAL_US);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable rotation vector: %s", esp_err_to_name(err));
        return;
    }

    // Enable accelerometer report
    err = bno085_enable_sensor(s_bno085_handle, BNO085_SENSOR_ACCELEROMETER, CONFIG_BNO085_REPORT_INTERVAL_US);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable accelerometer: %s", esp_err_to_name(err));
        return;
    }

    ESP_LOGI(TAG, "BNO085 successfully initialized (rotation vector and accelerometer enabled)");

    xTaskCreate(imu_task_code, "imu_task_code", 4 * 1024, (void *)s_bno085_handle, 5,
                &imu_task_handle);
}

bool imu_get_angles(float *x, float *y, float *z) {
    if (imu_mutex != NULL && xSemaphoreTake(imu_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        if (!g_has_angle_reading) {
            xSemaphoreGive(imu_mutex);
            return false;
        }
        if (x) *x = g_angle_x;
        if (y) *y = g_angle_y;
        if (z) *z = g_angle_z;
        xSemaphoreGive(imu_mutex);
        return true;
    }
    return false;
}

bool imu_get_accel(float *x, float *y, float *z) {
    if (imu_mutex != NULL && xSemaphoreTake(imu_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        if (!g_has_acc_reading) {
            xSemaphoreGive(imu_mutex);
            return false;
        }
        if (x) *x = g_acc_x;
        if (y) *y = g_acc_y;
        if (z) *z = g_acc_z;
        xSemaphoreGive(imu_mutex);
        return true;
    }
    return false;
}

bool imu_get_data(float *ang_x, float *ang_y, float *ang_z,
                  float *acc_x, float *acc_y, float *acc_z) {
    if (imu_mutex != NULL && xSemaphoreTake(imu_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        if (!g_has_angle_reading || !g_has_acc_reading) {
            xSemaphoreGive(imu_mutex);
            return false;
        }
        if (ang_x) *ang_x = g_angle_x;
        if (ang_y) *ang_y = g_angle_y;
        if (ang_z) *ang_z = g_angle_z;
        if (acc_x) *acc_x = g_acc_x;
        if (acc_y) *acc_y = g_acc_y;
        if (acc_z) *acc_z = g_acc_z;
        
        g_has_angle_reading = false;
        g_has_acc_reading = false;

        xSemaphoreGive(imu_mutex);
        return true;
    }
    return false;
}