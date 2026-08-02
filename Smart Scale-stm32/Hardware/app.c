#include "app.h"

#include "alarm.h"
#include "bsp_time.h"
#include "bsp_usart.h"
#include "esp8266_link.h"
#include "hx711.h"
#include "keys.h"
#include "oled.h"
#include "scale.h"
#include "settings.h"

/* OLED和按键共同驱动的顶层业务状态。 */
typedef enum { APP_WEIGH, APP_MENU, APP_CAL_SELECT, APP_CAL_EMPTY,
               APP_CAL_LOAD, APP_MESSAGE, APP_SENSOR_ERROR } App_State;
/* 功能键短按时按顺序循环的菜单项目。 */
typedef enum { MENU_TARE, MENU_UNIT, MENU_LIMIT, MENU_ZERO, MENU_COUNT } Menu_Item;

/* 用户可选的三种标准砝码，单位始终为克。 */
static const float g_cal_masses[] = {10.0f, 50.0f, 100.0f};
static App_Settings g_settings;
static Scale_Calibration g_cal;
static App_State g_state;
static Menu_Item g_menu;
static uint8_t g_cal_index;
static float g_weight_g;
static float g_display_value;
static uint8_t g_stable;
static uint8_t g_alarm;
static uint32_t g_last_display;
static uint32_t g_last_publish;
static uint32_t g_last_timeout;
static uint32_t g_last_sensor_retry;
static uint32_t g_message_until;
static const char *g_message;
static App_State g_message_return;

static uint8_t ProbeHX711(uint8_t print_result)
{
    int32_t raw;
    HX711_Status status;

    /* 用手册规定的断电/唤醒过程恢复可能处于异常状态的HX711。 */
    HX711_PowerDown();
    HX711_PowerUp();
    BSP_DelayMs(500U);
    status = HX711_ReadRaw(&raw, HX711_READ_TIMEOUT_MS);

    /* 第25个脉冲后DOUT应回到高电平等待下一次转换；持续为低表示引脚短路。 */
    if ((status == HX711_OK) && (HX711_GetDoutLevel() != 0U)) {
        if (print_result != 0U) {
            BSP_USART1_Printf("HX711 online, raw=%ld\r\n", (long)raw);
        }
        return 1U;
    }

    if (print_result != 0U) {
        BSP_USART1_Printf("HX711 error, status=%u, DOUT=%u\r\n",
                          (unsigned)status,
                          (unsigned)HX711_GetDoutLevel());
        BSP_USART1_WriteString("Check: VCC=3.3V, common GND, DOUT=PB12, SCK=PB13\r\n");
    }
    return 0U;
}

static const char *UnitName(void)
{
    /* unit索引同时用于OLED显示和发往ESP8266的JSON。 */
    static const char *names[] = {"g", "kg", "oz"};
    return names[g_settings.unit % 3U];
}

static float ConvertUnit(float grams)
{
    /* 内部计算和报警永远使用g，只在最终显示时转换单位。 */
    if (g_settings.unit == 1U) return grams / 1000.0f;
    if (g_settings.unit == 2U) return grams / 28.349523f;
    return grams;
}

static void SyncCalibration(void)
{
    /* Flash结构和称重算法结构分离，此处在启动时同步一次。 */
    g_cal.offset = g_settings.offset;
    g_cal.counts_per_gram = g_settings.counts_per_gram;
    g_cal.calibrated = g_settings.calibrated;
}

static void SaveCalibration(void)
{
    /* 去皮或标定完成后同时更新RAM副本和Flash持久化数据。 */
    g_settings.offset = g_cal.offset;
    g_settings.counts_per_gram = g_cal.counts_per_gram;
    g_settings.calibrated = g_cal.calibrated;
    Settings_Save(&g_settings);
}

static void ShowMessage(const char *message, uint32_t duration_ms,
                        App_State return_state)
{
    /* 消息页到期后回到指定状态，校准失败时可返回原步骤重试。 */
    g_message = message;
    g_message_until = BSP_Millis() + duration_ms;
    g_message_return = return_state;
    g_state = APP_MESSAGE;
}

static void DrawScreen(void)
{
    static const char *menu_names[] = {"TARE", "UNIT", "LIMIT", "ZERO"};
    OLED_Clear();
    /*
     * 5x7字体每个字符占6像素宽。标题、提示和数值+单位均整体居中，
     * 避免字符位数改变后看起来偏向左侧或右侧。
     */
    if (g_state == APP_WEIGH) {
        OLED_DrawTextCentered(0U, "WEIGHT");
        OLED_DrawTenthsUnitCentered(2U, g_display_value, UnitName());
        OLED_DrawText(4U, 4U, g_stable ? "STABLE" : "MOVING");
        OLED_DrawText(86U, 4U, g_alarm ? "ALARM" : "OK");
        OLED_DrawText(0U, 6U, "LIMIT");
        OLED_DrawTenthsUnitCentered(6U, g_settings.alarm_limit_g, "G");
    } else if (g_state == APP_MENU) {
        OLED_DrawTextCentered(0U, "FUNCTION");
        OLED_DrawTextCentered(2U, menu_names[g_menu]);
        if (g_menu == MENU_LIMIT) {
            OLED_DrawTenthsUnitCentered(4U, g_settings.alarm_limit_g, "G");
        }
        OLED_DrawTextCentered(6U, "FUNC NEXT OK ENTER");
    } else if (g_state == APP_CAL_SELECT) {
        OLED_DrawTextCentered(0U, "CAL MASS");
        OLED_DrawTenthsUnitCentered(2U, g_cal_masses[g_cal_index], "G");
        OLED_DrawTextCentered(5U, "+/- SELECT");
        OLED_DrawTextCentered(7U, "OK NEXT");
    } else if (g_state == APP_CAL_EMPTY) {
        OLED_DrawTextCentered(0U, "CALIBRATION");
        OLED_DrawTextCentered(2U, "EMPTY SCALE");
        OLED_DrawTextCentered(6U, "PRESS OK");
    } else if (g_state == APP_CAL_LOAD) {
        OLED_DrawTextCentered(0U, "PLACE MASS");
        OLED_DrawTenthsUnitCentered(2U, g_cal_masses[g_cal_index], "G");
        OLED_DrawTextCentered(6U, "PRESS OK");
    } else if (g_state == APP_SENSOR_ERROR) {
        OLED_DrawTextCentered(1U, "HX711 TIMEOUT");
        OLED_DrawTextCentered(3U, "CHECK WIRING");
        OLED_DrawTextCentered(5U, "RETRYING");
    } else {
        OLED_DrawTextCentered(2U, g_message);
    }
    OLED_Update();
}

static void HandleKey(Key_Event event)
{
    HX711_Status status;
    if (event == KEY_NONE) return;
    /* 功能键长按在任何普通界面都优先进入校准砝码选择。 */
    if (event == KEY_FUNC_LONG) { g_cal_index = 1U; g_state = APP_CAL_SELECT; return; }
    if (g_state == APP_WEIGH) {
        if (event == KEY_FUNC_SHORT) { g_menu = MENU_TARE; g_state = APP_MENU; }
    } else if (g_state == APP_MENU) {
        /* 功能键换菜单，加减键只修改报警阈值，确认键执行菜单。 */
        if (event == KEY_FUNC_SHORT) g_menu = (Menu_Item)((g_menu + 1U) % MENU_COUNT);
        else if ((event == KEY_PLUS) && (g_menu == MENU_LIMIT) &&
                 (g_settings.alarm_limit_g < 1000.0f)) g_settings.alarm_limit_g += 10.0f;
        else if ((event == KEY_MINUS) && (g_menu == MENU_LIMIT) &&
                 (g_settings.alarm_limit_g > 10.0f)) g_settings.alarm_limit_g -= 10.0f;
        else if (event == KEY_OK) {
            if ((g_menu == MENU_TARE) || (g_menu == MENU_ZERO)) {
                status = Scale_Tare(&g_cal);
                if (status == HX711_OK) { SaveCalibration(); ShowMessage("TARE OK", 1200U, APP_WEIGH); }
                else ShowMessage("TIMEOUT", 1500U, APP_MENU);
            } else if (g_menu == MENU_UNIT) {
                g_settings.unit = (uint8_t)((g_settings.unit + 1U) % 3U);
                Settings_Save(&g_settings); ShowMessage("UNIT SAVED", 1000U, APP_WEIGH);
            } else { Settings_Save(&g_settings); ShowMessage("LIMIT SAVED", 1000U, APP_WEIGH); }
        }
    } else if (g_state == APP_CAL_SELECT) {
        /* 加减键在10/50/100g之间循环，不会产生越界索引。 */
        if (event == KEY_PLUS) g_cal_index = (uint8_t)((g_cal_index + 1U) % 3U);
        else if (event == KEY_MINUS) g_cal_index = (uint8_t)((g_cal_index + 2U) % 3U);
        else if (event == KEY_OK) g_state = APP_CAL_EMPTY;
    } else if ((g_state == APP_CAL_EMPTY) && (event == KEY_OK)) {
        /* 校准第一步：空秤稳定后记录零点。 */
        status = Scale_Tare(&g_cal);
        if (status == HX711_OK) g_state = APP_CAL_LOAD;
        else ShowMessage("TARE TIMEOUT", 1500U, APP_CAL_EMPTY);
    } else if ((g_state == APP_CAL_LOAD) && (event == KEY_OK)) {
        /* 校准第二步：放置标准砝码并计算每克对应的ADC计数。 */
        status = Scale_CaptureCalibration(&g_cal, g_cal_masses[g_cal_index]);
        if (status == HX711_OK) { SaveCalibration(); ShowMessage("CAL SUCCESS", 1800U, APP_WEIGH); }
        else if (status == HX711_NOT_STABLE) ShowMessage("KEEP STILL", 1200U, APP_CAL_LOAD);
        else ShowMessage("CAL TIMEOUT", 1500U, APP_CAL_LOAD);
    }
}

void App_Init(void)
{
    /* 先建立1ms时基，再依次初始化通信、传感器、人机界面和输出。 */
    BSP_Time_Init(); BSP_USART1_Init(9600U); ESP8266_LinkInit(9600U);
    HX711_Init(); Keys_Init(); Alarm_Init(); OLED_Init();
    Settings_Load(&g_settings); SyncCalibration();
    g_weight_g = 0.0f; g_display_value = 0.0f; g_stable = 0U; g_alarm = 0U;
    g_last_display = 0U; g_last_publish = 0U; g_last_timeout = 0U;
    g_last_sensor_retry = 0U;
    BSP_USART1_WriteString("\r\nHX711 scale, USART1 9600 8N1\r\n");
    BSP_USART1_WriteString("Boot OK, checking timebase and HX711...\r\n");
    BSP_USART1_Printf("SystemCoreClock=%lu Hz, SysTick=%s\r\n",
                      (unsigned long)SystemCoreClock,
                      BSP_TimebaseCheck() ? "OK" : "ERROR");

    if (ProbeHX711(1U) != 0U) {
        /* Flash没有有效标定时，首次启动进入校准砝码选择。 */
        g_state = g_settings.calibrated ? APP_WEIGH : APP_CAL_SELECT;
        if (!g_settings.calibrated) {
            BSP_USART1_WriteString("CAL: select 10/50/100 g, then press OK\r\n");
        }
    } else {
        /* 故障时不阻塞启动，显示错误并在前台周期性自动重试。 */
        g_state = APP_SENSOR_ERROR;
        g_alarm = 1U;
        Alarm_Set(1U);
    }
    DrawScreen();
}

void App_Run(void)
{
    uint32_t now = BSP_Millis();
    HX711_Status status;
    /* SysTick负责采样按键，前台只取出并执行已经消抖的事件。 */
    HandleKey(Keys_GetEvent());
    if ((g_state == APP_SENSOR_ERROR) &&
        ((uint32_t)(now - g_last_sensor_retry) >= 2000U)) {
        g_last_sensor_retry = now;
        BSP_USART1_WriteString("Retry HX711...\r\n");
        if (ProbeHX711(1U) != 0U) {
            Alarm_Set(0U);
            g_alarm = 0U;
            g_state = g_settings.calibrated ? APP_WEIGH : APP_CAL_SELECT;
        }
    }
    if ((g_state == APP_MESSAGE) && ((int32_t)(now - g_message_until) >= 0)) g_state = g_message_return;
    if (g_state == APP_WEIGH) {
        /* 每轮读取一组HX711样本；稳定与移动状态都保留当前重量。 */
        status = Scale_ReadStable(&g_cal, &g_weight_g);
        g_stable = (status == HX711_OK);
        if ((status == HX711_OK) || (status == HX711_NOT_STABLE)) {
            g_display_value = ConvertUnit(g_weight_g);
            /* 用户阈值和传感器1kg物理量程任一超出都触发报警。 */
            g_alarm = ((g_weight_g > g_settings.alarm_limit_g) || (g_weight_g > 1000.0f));
        } else {
            g_alarm = 1U;
            if ((uint32_t)(now - g_last_timeout) >= 1000U) {
                g_last_timeout = now;
                BSP_USART1_WriteString("timeout\r\n");
            }
        }
        Alarm_Set(g_alarm);
        /* 每秒发送一次完整状态，降低串口和MQTT服务器负担。 */
        if ((uint32_t)(now - g_last_publish) >= 1000U) {
            g_last_publish = now;
            ESP8266_SendWeight(g_weight_g, UnitName(), g_display_value, g_alarm, g_stable);
        }
    } else Alarm_Set(0U);
    /* OLED最高5Hz刷新，兼顾视觉响应和软件I2C占用时间。 */
    if ((uint32_t)(BSP_Millis() - g_last_display) >= 200U) {
        g_last_display = BSP_Millis(); DrawScreen();
    }
}
