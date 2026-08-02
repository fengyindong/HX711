#include "oled.h"

#include <stdio.h>
#include <string.h>
#include "bsp_time.h"

/*
 * OLED模块：针对GMD09605老款4针SSD1306模组重新实现。
 * 使用水平寻址模式，一次刷新明确限定列0~127、页0~7，防止地址
 * 没有复位造成画面整体偏移、换页错位或刷新后位置漂移。
 */
#define OLED_PORT       GPIOB
#define OLED_SCL_PIN    GPIO_Pin_6
#define OLED_SDA_PIN    GPIO_Pin_7
#define OLED_ADDRESS    ((uint8_t)(OLED_I2C_ADDRESS_7BIT << 1U))

static uint8_t g_buffer[OLED_WIDTH * OLED_PAGE_COUNT];

/* 软件I2C使用短延时控制总线速度，OLED不需要高速传输。 */
static void I2C_Delay(void)
{
    volatile uint8_t i;
    for (i = 0U; i < 12U; ++i) {
        __NOP();
    }
}

static void SDA(uint8_t high)
{
    if (high != 0U) GPIO_SetBits(OLED_PORT, OLED_SDA_PIN);
    else GPIO_ResetBits(OLED_PORT, OLED_SDA_PIN);
}

static void SCL(uint8_t high)
{
    if (high != 0U) GPIO_SetBits(OLED_PORT, OLED_SCL_PIN);
    else GPIO_ResetBits(OLED_PORT, OLED_SCL_PIN);
}

static void I2C_Start(void)
{
    SDA(1U); SCL(1U); I2C_Delay(); SDA(0U); I2C_Delay(); SCL(0U);
}

static void I2C_Stop(void)
{
    SDA(0U); SCL(1U); I2C_Delay(); SDA(1U); I2C_Delay();
}

static void I2C_Write(uint8_t value)
{
    uint8_t i;
    for (i = 0U; i < 8U; ++i) {
        SDA((value & 0x80U) != 0U); SCL(1U); I2C_Delay(); SCL(0U);
        value <<= 1;
    }
    /* 第9个时钟用于ACK；当前仅发送显示数据，不因无ACK阻塞系统。 */
    SDA(1U); SCL(1U); I2C_Delay(); SCL(0U);
}

static void OLED_Command(uint8_t command)
{
    I2C_Start(); I2C_Write(OLED_ADDRESS); I2C_Write(0x00U);
    I2C_Write(command); I2C_Stop();
}

/* 连续发送多条命令，Co=0、D/C#=0表示后续字节都是命令。 */
static void OLED_Commands(const uint8_t *commands, uint8_t count)
{
    uint8_t i;

    I2C_Start();
    I2C_Write(OLED_ADDRESS);
    I2C_Write(0x00U);
    for (i = 0U; i < count; ++i) {
        I2C_Write(commands[i]);
    }
    I2C_Stop();
}

static void Glyph(char c, uint8_t out[5])
{
    /*
     * 5x7紧凑字库仅包含界面所需ASCII字符，可显著节省F103 Flash。
     * 小写字母自动转换为大写，不支持的字符显示为空白。
     */
    static const uint8_t digits[12][5] = {
        {0x3E,0x51,0x49,0x45,0x3E},{0x00,0x42,0x7F,0x40,0x00},
        {0x42,0x61,0x51,0x49,0x46},{0x21,0x41,0x45,0x4B,0x31},
        {0x18,0x14,0x12,0x7F,0x10},{0x27,0x45,0x45,0x45,0x39},
        {0x3C,0x4A,0x49,0x49,0x30},{0x01,0x71,0x09,0x05,0x03},
        {0x36,0x49,0x49,0x49,0x36},{0x06,0x49,0x49,0x29,0x1E},
        {0x00,0x60,0x60,0x00,0x00},{0x08,0x08,0x08,0x08,0x08}
    };
    static const uint8_t letters[26][5] = {
        {0x7E,0x11,0x11,0x11,0x7E},{0x7F,0x49,0x49,0x49,0x36},
        {0x3E,0x41,0x41,0x41,0x22},{0x7F,0x41,0x41,0x22,0x1C},
        {0x7F,0x49,0x49,0x49,0x41},{0x7F,0x09,0x09,0x09,0x01},
        {0x3E,0x41,0x49,0x49,0x7A},{0x7F,0x08,0x08,0x08,0x7F},
        {0x00,0x41,0x7F,0x41,0x00},{0x20,0x40,0x41,0x3F,0x01},
        {0x7F,0x08,0x14,0x22,0x41},{0x7F,0x40,0x40,0x40,0x40},
        {0x7F,0x02,0x0C,0x02,0x7F},{0x7F,0x04,0x08,0x10,0x7F},
        {0x3E,0x41,0x41,0x41,0x3E},{0x7F,0x09,0x09,0x09,0x06},
        {0x3E,0x41,0x51,0x21,0x5E},{0x7F,0x09,0x19,0x29,0x46},
        {0x46,0x49,0x49,0x49,0x31},{0x01,0x01,0x7F,0x01,0x01},
        {0x3F,0x40,0x40,0x40,0x3F},{0x1F,0x20,0x40,0x20,0x1F},
        {0x3F,0x40,0x38,0x40,0x3F},{0x63,0x14,0x08,0x14,0x63},
        {0x07,0x08,0x70,0x08,0x07},{0x61,0x51,0x49,0x45,0x43}
    };

    memset(out, 0, 5U);
    if ((c >= '0') && (c <= '9')) memcpy(out, digits[c - '0'], 5U);
    else if (c == '.') memcpy(out, digits[10], 5U);
    else if (c == '-') memcpy(out, digits[11], 5U);
    else if (c == '+') {
        static const uint8_t plus[5] = {0x08,0x08,0x3E,0x08,0x08};
        memcpy(out, plus, 5U);
    } else if (c == '/') {
        static const uint8_t slash[5] = {0x20,0x10,0x08,0x04,0x02};
        memcpy(out, slash, 5U);
    } else if (c == ':') {
        static const uint8_t colon[5] = {0x00,0x36,0x36,0x00,0x00};
        memcpy(out, colon, 5U);
    }
    else {
        if ((c >= 'a') && (c <= 'z')) c = (char)(c - 'a' + 'A');
        if ((c >= 'A') && (c <= 'Z')) memcpy(out, letters[c - 'A'], 5U);
    }
}

void OLED_Init(void)
{
    GPIO_InitTypeDef gpio;
    /*
     * 初始化值按规格书的SSD1306命令表设置：
     * 64MUX、起始行0、显示偏移0、水平寻址、正常显示、内部电荷泵。
     */
    const uint8_t init[] = {
        0xAEU,                         /* 关闭显示，配置期间避免花屏 */
        0x2EU,                         /* 关闭可能残留的滚动模式 */
        0xD5U, 0x80U,                  /* 显示时钟分频/振荡频率 */
        0xA8U, 0x3FU,                  /* 1/64 Duty：MUX=63 */
        0xD3U, OLED_DISPLAY_OFFSET,    /* COM垂直显示偏移：规格书为0 */
        (uint8_t)(0x40U | (OLED_DISPLAY_START_LINE & 0x3FU)),
        0x8DU, 0x14U,                  /* 开启内部电荷泵 */
        0x20U, 0x00U,                  /* 水平寻址模式 */
#if OLED_MIRROR_X
        0xA1U,                         /* SEG127映射到列0 */
#else
        0xA0U,                         /* SEG0映射到列0 */
#endif
#if OLED_MIRROR_Y
        0xC8U,                         /* COM63扫描到COM0 */
#else
        0xC0U,                         /* COM0扫描到COM63 */
#endif
        0xDAU, 0x12U,                  /* 128x64交替COM引脚配置 */
        0x81U, 0x7FU,                  /* 对比度，可按亮度需求调整 */
        0xD9U, 0xF1U,                  /* 预充电周期 */
        0xDBU, 0x40U,                  /* VCOMH电平 */
        0xA4U,                         /* 显示内容来自GDDRAM */
        0xA6U                          /* 正常显示：1点亮、0熄灭 */
    };

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    gpio.GPIO_Pin = OLED_SCL_PIN | OLED_SDA_PIN;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode = GPIO_Mode_Out_OD;
    GPIO_Init(OLED_PORT, &gpio);
    SCL(1U);
    SDA(1U);

    /* 四针模组没有RST引脚，必须等待板载上电复位完成。 */
    BSP_DelayMs(100U);
    OLED_Commands(init, (uint8_t)sizeof(init));

    OLED_Clear();
    OLED_Update();
    OLED_Command(0xAFU);               /* 清屏完成后再开启显示 */
}

void OLED_Clear(void)
{
    memset(g_buffer, 0, sizeof(g_buffer));
}

void OLED_DrawText(uint8_t x, uint8_t page, const char *text)
{
    uint8_t glyph[5];
    uint8_t i;

    /* 每个字符占5列点阵和1列空白，超出屏幕宽度时停止绘制。 */
    while ((*text != '\0') && (page < OLED_PAGE_COUNT) &&
           (x <= (OLED_WIDTH - 6U))) {
        Glyph(*text++, glyph);
        for (i = 0U; i < 5U; ++i) {
            g_buffer[(uint16_t)page * OLED_WIDTH + x++] = glyph[i];
        }
        g_buffer[(uint16_t)page * OLED_WIDTH + x++] = 0U;
    }
}

uint8_t OLED_TextWidth(const char *text)
{
    uint16_t width = 0U;

    if (text == 0) {
        return 0U;
    }
    while (*text++ != '\0') {
        width += 6U;
        if (width >= OLED_WIDTH) {
            return OLED_WIDTH;
        }
    }
    /* 最后一个字符后不需要额外空白列。 */
    return (width == 0U) ? 0U : (uint8_t)(width - 1U);
}

void OLED_DrawTextCentered(uint8_t page, const char *text)
{
    uint8_t width = OLED_TextWidth(text);
    uint8_t x = (width < OLED_WIDTH) ? (uint8_t)((OLED_WIDTH - width) / 2U) : 0U;

    OLED_DrawText(x, page, text);
}

void OLED_DrawTenths(uint8_t x, uint8_t page, float value)
{
    char text[20];
    long tenths = (long)((value >= 0.0f) ? value * 10.0f + 0.5f : value * 10.0f - 0.5f);
    unsigned long magnitude = (tenths < 0) ? (unsigned long)(-tenths) : (unsigned long)tenths;
    snprintf(text, sizeof(text), "%s%lu.%lu", (tenths < 0) ? "-" : "",
             magnitude / 10U, magnitude % 10U);
    OLED_DrawText(x, page, text);
}

void OLED_DrawTenthsUnitCentered(uint8_t page, float value,
                                 const char *unit)
{
    char number[20];
    long tenths;
    unsigned long magnitude;
    uint8_t number_width;
    uint8_t unit_width;
    uint8_t total_width;
    uint8_t x;

    tenths = (long)((value >= 0.0f) ? value * 10.0f + 0.5f :
                                      value * 10.0f - 0.5f);
    magnitude = (tenths < 0) ? (unsigned long)(-tenths) :
                               (unsigned long)tenths;
    snprintf(number, sizeof(number), "%s%lu.%lu", (tenths < 0) ? "-" : "",
             magnitude / 10U, magnitude % 10U);

    number_width = OLED_TextWidth(number);
    unit_width = OLED_TextWidth(unit);
    total_width = (uint8_t)(number_width + 6U + unit_width);
    x = (total_width < OLED_WIDTH) ?
        (uint8_t)((OLED_WIDTH - total_width) / 2U) : 0U;

    OLED_DrawText(x, page, number);
    OLED_DrawText((uint8_t)(x + number_width + 6U), page, unit);
}

void OLED_Update(void)
{
    uint16_t i;
    const uint8_t window[] = {
        0x21U, 0x00U, 0x7FU,           /* 列地址窗口：0~127 */
        0x22U, 0x00U, 0x07U            /* 页地址窗口：0~7 */
    };

    /*
     * 每次刷新前重新指定完整显存窗口。这样即使之前执行过其他寻址命令，
     * 第一个缓冲字节也一定写入左上角(column 0, page 0)。
     */
    OLED_Commands(window, (uint8_t)sizeof(window));

    I2C_Start();
    I2C_Write(OLED_ADDRESS);
    I2C_Write(0x40U);                  /* Co=0、D/C#=1：后续均为显示数据 */
    for (i = 0U; i < sizeof(g_buffer); ++i) {
        I2C_Write(g_buffer[i]);
    }
    I2C_Stop();
}

void OLED_ShowAlignmentTest(void)
{
    uint8_t page;
    uint8_t x;

    OLED_Clear();

    /* 顶边、底边、左边和右边同时点亮，可直观看出是否存在固定偏移。 */
    for (x = 0U; x < OLED_WIDTH; ++x) {
        g_buffer[x] |= 0x01U;
        g_buffer[(OLED_PAGE_COUNT - 1U) * OLED_WIDTH + x] |= 0x80U;
    }
    for (page = 0U; page < OLED_PAGE_COUNT; ++page) {
        g_buffer[(uint16_t)page * OLED_WIDTH] = 0xFFU;
        g_buffer[(uint16_t)page * OLED_WIDTH + OLED_WIDTH - 1U] = 0xFFU;
    }

    OLED_DrawText(3U, 1U, "X0 Y0");
    OLED_DrawText(82U, 6U, "127 63");
    OLED_Update();
}