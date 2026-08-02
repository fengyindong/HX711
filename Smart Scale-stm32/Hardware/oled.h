#ifndef __OLED_H
#define __OLED_H

#include "stm32f10x.h"

/*
 * GMD09605老款4针模组参数（规格书）：
 *   控制器：SSD1306
 *   分辨率：128 x 64
 *   接口：I2C
 *   引脚：1=GND、2=VDD、3=SCK、4=SDA
 *
 * 该模组的有效列就是SSD1306的0~127列，因此列偏移必须为0。
 * 不要使用某些SH1106驱动中常见的“列地址+2”处理。
 */
#define OLED_WIDTH                 128U
#define OLED_HEIGHT                64U
#define OLED_PAGE_COUNT            (OLED_HEIGHT / 8U)
#define OLED_I2C_ADDRESS_7BIT      0x3CU

/*
 * 显示方向配置。当前取值保持原项目的正常观察方向。
 * 如果整个画面左右镜像，将OLED_MIRROR_X改为0；
 * 如果整个画面上下颠倒，将OLED_MIRROR_Y改为0。
 */
#define OLED_MIRROR_X              1U
#define OLED_MIRROR_Y              1U

/*
 * SSD1306显示起始行和COM偏移，规格书对应的正确值均为0。
 * 只有确认屏幕玻璃封装存在固定行偏移时才修改DISPLAY_OFFSET。
 */
#define OLED_DISPLAY_START_LINE    0U
#define OLED_DISPLAY_OFFSET        0U

/* 初始化PB6/PB7软件I2C及规格书指定的SSD1306 128x64 OLED。 */
void OLED_Init(void);
/* 清空RAM帧缓冲区，需要OLED_Update()后才显示。 */
void OLED_Clear(void);
/* 在指定x坐标和页(0~7)绘制紧凑ASCII文本。 */
void OLED_DrawText(uint8_t x, uint8_t page, const char *text);
/* 返回5x7字体字符串占用的像素宽度，每个字符占5列加1列间隔。 */
uint8_t OLED_TextWidth(const char *text);
/* 把一行ASCII文本在128像素宽度内水平居中。 */
void OLED_DrawTextCentered(uint8_t page, const char *text);
/* 将浮点数四舍五入到小数点后一位后绘制。 */
void OLED_DrawTenths(uint8_t x, uint8_t page, float value);
/* 将“数值 + 单位”作为一个整体水平居中显示。 */
void OLED_DrawTenthsUnitCentered(uint8_t page, float value,
                                 const char *unit);
/* 把1024字节帧缓冲区完整刷新到OLED。 */
void OLED_Update(void);

/*
 * 显示边框和四角坐标标记，用于检查列0~127、页0~7是否准确对齐。
 * 调试时可在OLED_Init()后临时调用，确认后再移除调用。
 */
void OLED_ShowAlignmentTest(void);

#endif
