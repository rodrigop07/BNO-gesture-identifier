#ifndef __IMU_CONFIG_H__
#define __IMU_CONFIG_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "hal/i2c_types.h"
#include <stdbool.h>


void imu_config_init(void *unused);

bool imu_get_angles(float *x, float *y, float *z);

bool imu_get_accel(float *x, float *y, float *z);

bool imu_get_data(float *ang_x, float *ang_y, float *ang_z, float *acc_x,
                  float *acc_y, float *acc_z);

#ifdef __cplusplus
}
#endif

#endif // __IMU_CONFIG_H__