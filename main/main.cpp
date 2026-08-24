#include "edge-impulse-sdk/classifier/ei_run_classifier.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "i2c_config.h"
#include "imu_config.h"
#include "oled_printf.h"
#include "oled_setup.h"
#include <stdio.h>
#include <string.h>

static float features[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE] = {0};
const int STRIDE_SIZE = 54;

int raw_feature_get_data(size_t offset, size_t length, float *out_ptr) {
  memcpy(out_ptr, features + offset, length * sizeof(float));
  return 0;
}

extern "C" void app_main(void) {
    // inicialização do OLED
    enable_vext_rail();
    i2c_master_bus_handle_t oled_i2c_bus = NULL;
    initialize_i2c(&oled_i2c_bus, (gpio_num_t)PIN_NUM_SDA, (gpio_num_t)PIN_NUM_SCL);
    configure_oled_screen(&oled_i2c_bus);
    oled_printf_init(local_disp);
    int volume = 50;
    printf_oled("Controle de Volume");
    printf_oled("Volume: %d%%", volume);
    imu_config_init(NULL);

    ei_impulse_result_t result = {0};
    static int feature_ix = 0;

    printf("Sistema iniciado, aguardando dados do sensor...\n");

    while (1) {
        float ang_x, ang_y, ang_z;
        float acc_x, acc_y, acc_z;

        if (imu_get_data(&ang_x, &ang_y, &ang_z, &acc_x, &acc_y, &acc_z)) {
            features[feature_ix + 0] = ang_x;
            features[feature_ix + 1] = ang_y;
            features[feature_ix + 2] = ang_z;
            features[feature_ix + 3] = acc_x;
            features[feature_ix + 4] = acc_y;
            features[feature_ix + 5] = acc_z;
            feature_ix += 6;

            if (feature_ix >= EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE) {
                signal_t features_signal;
                features_signal.total_length = EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE;
                features_signal.get_data = &raw_feature_get_data;

                EI_IMPULSE_ERROR res = run_classifier(&features_signal, &result, false);

                if (res == EI_IMPULSE_OK) {
                    int max_index = 0;
                    float max_val = result.classification[0].value;
                    for (uint16_t i = 1; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
                        if (result.classification[i].value > max_val) {
                            max_val = result.classification[i].value;
                            max_index = i;
                        }
                    }
                    printf("Predicted: %s (%.5f)\n", result.classification[max_index].label, max_val);

                    if (max_val > 0.6f) { // Ajusta volume se confiança for maior que 60%
                        if (strcmp(result.classification[max_index].label, "circulo_horario") == 0) {
                            volume += 5;
                            if (volume > 100) volume = 100;
                            printf_oled("Volume: %d%%", volume);
                        } else if (strcmp(result.classification[max_index].label, "circulo_antiHorario") == 0) {
                            volume -= 5;
                            if (volume < 0) volume = 0;
                            printf_oled("Volume: %d%%", volume);
                        }
                    }
                }

                int elementos_para_mover = EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE - STRIDE_SIZE;
                memmove(&features[0], &features[STRIDE_SIZE], elementos_para_mover * sizeof(float));
                feature_ix = elementos_para_mover;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
