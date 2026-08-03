#ifndef __APP_CONFIG_H
#define __APP_CONFIG_H

/* 5kg称重系统的统一业务参数，所有模块不得再散落硬编码量程。 */
#define SCALE_CAPACITY_G             5000.0f
#define SCALE_DEFAULT_ALARM_G        4500.0f
#define SCALE_ALARM_MIN_G             100.0f
#define SCALE_ALARM_STEP_G            100.0f

/* USART2状态上报周期。 */
#define SCALE_REPORT_INTERVAL_MS     1000U

#endif
