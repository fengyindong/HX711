#ifndef __SETTINGS_H
#define __SETTINGS_H

#include "stm32f10x.h"

typedef struct {
    int32_t offset;          /* 去皮零点 */
    float counts_per_gram;  /* 标定比例系数 */
    float alarm_limit_g;    /* 用户设置的超重报警阈值，单位g */
    uint8_t unit;           /* 0=g、1=kg、2=oz */
    uint8_t calibrated;     /* 是否已有有效标定 */
} App_Settings;

/* 填入安全的出厂默认值。 */
void Settings_SetDefaults(App_Settings *settings);
/* 从Flash读取并校验参数，成功返回1。 */
uint8_t Settings_Load(App_Settings *settings);
/* 擦除最后一页Flash并保存参数，成功返回1。 */
uint8_t Settings_Save(const App_Settings *settings);

#endif
