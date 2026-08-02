#include "scale.h"

#include "bsp_time.h"
#include "bsp_usart.h"

/* 称重算法模块：在HX711原始计数之上实现去皮、标定、滤波和稳定判断。 */
static int32_t Scale_Abs32(int32_t value)
{
    return (value < 0) ? -value : value;
}

static void Scale_SortInt32(int32_t *values, uint8_t count)
{
    uint8_t i;
    uint8_t j;

    /* 样本数很少，插入排序占用代码和RAM都比通用排序更合适。 */
    for (i = 1U; i < count; ++i) {
        int32_t key = values[i];
        j = i;
        while ((j > 0U) && (values[j - 1U] > key)) {
            values[j] = values[j - 1U];
            --j;
        }
        values[j] = key;
    }
}

static HX711_Status Scale_ReadRawWindow(int32_t *mean, int32_t *span)
{
    int32_t samples[SCALE_WINDOW_SIZE];
    int64_t sum = 0;
    HX711_Status status;
    uint8_t i;

    /* 10Hz模式下一组7点数据约需0.7秒，优先获得低噪声稳定结果。 */
    for (i = 0U; i < SCALE_WINDOW_SIZE; ++i) {
        status = HX711_ReadRaw(&samples[i], HX711_READ_TIMEOUT_MS);
        if (status != HX711_OK) {
            return status;
        }
    }

    Scale_SortInt32(samples, SCALE_WINDOW_SIZE);
    /* 去掉两端各一个毛刺后，再用剩余数据极差衡量稳定性。 */
    *span = samples[SCALE_WINDOW_SIZE - 2U] - samples[1U];

    /* 剔除最大和最小样本，对中间样本做截尾平均。 */
    for (i = 1U; i < (SCALE_WINDOW_SIZE - 1U); ++i) {
        sum += samples[i];
    }
    *mean = (int32_t)(sum / (int32_t)(SCALE_WINDOW_SIZE - 2U));
    return HX711_OK;
}

static void Scale_PrintSignedTenths(const char *prefix, int32_t tenths,
                                    const char *suffix)
{
    uint32_t magnitude;

    if (tenths < 0) {
        magnitude = (uint32_t)(-tenths);
        BSP_USART1_Printf("%s-%lu.%lu%s", prefix,
                          (unsigned long)(magnitude / 10U),
                          (unsigned long)(magnitude % 10U), suffix);
    } else {
        magnitude = (uint32_t)tenths;
        BSP_USART1_Printf("%s%lu.%lu%s", prefix,
                          (unsigned long)(magnitude / 10U),
                          (unsigned long)(magnitude % 10U), suffix);
    }
}

static int32_t Scale_RoundTenths(float value)
{
    /* 正负数分别加减0.5，确保都按数学规则四舍五入。 */
    float scaled = value * 10.0f;

    return (int32_t)((scaled >= 0.0f) ? (scaled + 0.5f) : (scaled - 0.5f));
}

HX711_Status Scale_Tare(Scale_Calibration *cal)
{
    int32_t mean;
    int32_t span;
    HX711_Status status;
    uint32_t start = BSP_Millis();

    if (cal == 0) {
        return HX711_INVALID_DATA;
    }

    /* 只在空秤数据波动进入阈值后更新零点，避免运动中误去皮。 */
    do {
        status = Scale_ReadRawWindow(&mean, &span);
        if (status != HX711_OK) {
            return status;
        }
        BSP_USART1_Printf("tare raw=%ld span=%ld\r\n", (long)mean, (long)span);
        if (span <= SCALE_RAW_STABLE_SPAN) {
            cal->offset = mean;
            return HX711_OK;
        }
    } while ((uint32_t)(BSP_Millis() - start) < SCALE_CAL_TIMEOUT_MS);

    return HX711_TIMEOUT;
}

HX711_Status Scale_Calibrate(Scale_Calibration *cal)
{
    int32_t loaded;
    int32_t span;
    int32_t delta;
    HX711_Status status;
    uint32_t start;

    if (cal == 0) {
        return HX711_INVALID_DATA;
    }
    /* 开始重新标定时先清除有效标志，防止失败后误用旧比例系数。 */
    cal->calibrated = 0U;

    BSP_USART1_WriteString("CAL: remove all load; waiting for stable tare...\r\n");
    status = Scale_Tare(cal);
    if (status != HX711_OK) {
        return status;
    }
    BSP_USART1_Printf("CAL: tare complete, offset=%ld\r\n", (long)cal->offset);
    Scale_PrintSignedTenths("CAL: place calibration weight ",
                            Scale_RoundTenths(SCALE_CAL_WEIGHT_G), " g\r\n");

    start = BSP_Millis();
    do {
        status = Scale_ReadRawWindow(&loaded, &span);
        if (status != HX711_OK) {
            return status;
        }
        delta = loaded - cal->offset;
        BSP_USART1_Printf("cal raw=%ld delta=%ld span=%ld\r\n",
                          (long)loaded, (long)delta, (long)span);

        if ((span <= SCALE_RAW_STABLE_SPAN) &&
            (Scale_Abs32(delta) >= SCALE_MIN_CAL_DELTA)) {
            /* 每克计数=(砝码原始值-空秤零点)/标准砝码克数。 */
            cal->counts_per_gram = (float)delta / SCALE_CAL_WEIGHT_G;
            cal->calibrated = 1U;
            BSP_USART1_Printf("CAL: complete, counts/g x1000=%ld\r\n",
                             (long)(cal->counts_per_gram * 1000.0f));
            BSP_USART1_WriteString("CAL: remove weight or start weighing.\r\n");
            return HX711_OK;
        }
    } while ((uint32_t)(BSP_Millis() - start) < SCALE_CAL_TIMEOUT_MS);

    return HX711_TIMEOUT;
}

HX711_Status Scale_CaptureCalibration(Scale_Calibration *cal, float weight_g)
{
    int32_t loaded;
    int32_t span;
    int32_t delta;
    HX711_Status status;

    if ((cal == 0) || (weight_g <= 0.0f)) {
        return HX711_INVALID_DATA;
    }
    status = Scale_ReadRawWindow(&loaded, &span);
    if (status != HX711_OK) {
        return status;
    }
    /* 此接口用于OLED交互式校准：去皮和放砝码由状态机分步完成。 */
    delta = loaded - cal->offset;
    BSP_USART1_Printf("cal raw=%ld delta=%ld span=%ld\r\n",
                      (long)loaded, (long)delta, (long)span);
    if ((span > SCALE_RAW_STABLE_SPAN) ||
        (Scale_Abs32(delta) < SCALE_MIN_CAL_DELTA)) {
        return HX711_NOT_STABLE;
    }
    cal->counts_per_gram = (float)delta / weight_g;
    cal->calibrated = 1U;
    return HX711_OK;
}

HX711_Status Scale_ReadStable(const Scale_Calibration *cal, float *weight_g)
{
    int32_t samples[SCALE_WINDOW_SIZE];
    int32_t span;
    int64_t sum = 0;
    HX711_Status status;
    uint8_t i;

    if ((cal == 0) || (weight_g == 0) || (cal->calibrated == 0U) ||
        (cal->counts_per_gram == 0.0f)) {
        return HX711_INVALID_DATA;
    }

    /* 采集完整窗口；任意一次超时或饱和都会终止本轮重量计算。 */
    for (i = 0U; i < SCALE_WINDOW_SIZE; ++i) {
        status = HX711_ReadRaw(&samples[i], HX711_READ_TIMEOUT_MS);
        if (status != HX711_OK) {
            return status;
        }
    }

    Scale_SortInt32(samples, SCALE_WINDOW_SIZE);
    for (i = 1U; i < (SCALE_WINDOW_SIZE - 1U); ++i) {
        sum += samples[i];
    }
    /* 重量=(滤波后的原始计数-去皮零点)/每克计数。 */
    *weight_g = ((float)(sum / (int32_t)(SCALE_WINDOW_SIZE - 2U)) -
                 (float)cal->offset) / cal->counts_per_gram;

    /* 零点附近设置小死区，避免空秤显示±0.1g反复跳动。 */
    if ((*weight_g > -SCALE_ZERO_BAND_G) && (*weight_g < SCALE_ZERO_BAND_G)) {
        *weight_g = 0.0f;
    }

    /* 极差换算成克后超过阈值，仍返回当前重量供OLED显示MOVING。 */
    span = samples[SCALE_WINDOW_SIZE - 2U] - samples[1U];
    if (((float)span / ((cal->counts_per_gram < 0.0f) ?
                       -cal->counts_per_gram : cal->counts_per_gram)) >
        SCALE_STABLE_SPAN_G) {
        return HX711_NOT_STABLE;
    }
    return HX711_OK;
}

void Scale_PrintWeight(float weight_g)
{
    Scale_PrintSignedTenths("weight=", Scale_RoundTenths(weight_g), " g\r\n");
}

void Scale_PrintUnstable(float weight_g)
{
    Scale_PrintSignedTenths("unstable=", Scale_RoundTenths(weight_g),
                            " g, waiting...\r\n");
}
