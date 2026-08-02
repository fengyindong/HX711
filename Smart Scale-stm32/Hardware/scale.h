#ifndef __SCALE_H
#define __SCALE_H

#include "stm32f10x.h"
#include "hx711.h"

#define SCALE_CAL_WEIGHT_G          500.0f /* 旧阻塞式校准接口的默认砝码 */
#define SCALE_CAL_TIMEOUT_MS        120000U
#define SCALE_RAW_STABLE_SPAN       1000L
#define SCALE_MIN_CAL_DELTA         5000L
#define SCALE_STABLE_SPAN_G         2.0f
#define SCALE_ZERO_BAND_G           0.3f
#define SCALE_WINDOW_SIZE           7U

typedef struct {
    int32_t offset;          /* 空秤原始计数，即去皮零点 */
    float counts_per_gram;  /* 每克对应的ADC计数，可正可负 */
    uint8_t calibrated;     /* 为1表示参数已经完成砝码标定 */
} Scale_Calibration;

/* 旧版完整阻塞式校准流程，保留给串口测试使用。 */
HX711_Status Scale_Calibrate(Scale_Calibration *cal);
/* 已去皮后，用指定砝码捕获比例系数。 */
HX711_Status Scale_CaptureCalibration(Scale_Calibration *cal, float weight_g);
/* 空秤稳定后更新offset。 */
HX711_Status Scale_Tare(Scale_Calibration *cal);
/* 读取一组数据，剔除极值、判断稳定并换算为克。 */
HX711_Status Scale_ReadStable(const Scale_Calibration *cal, float *weight_g);
/* 调试串口打印稳定重量或不稳定重量。 */
void Scale_PrintWeight(float weight_g);
void Scale_PrintUnstable(float weight_g);

#endif
