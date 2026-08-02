#include "hx711.h"
#include "bsp_time.h"

/* HX711底层驱动：严格按照芯片手册产生时钟并读取24位补码数据。 */
static void HX711_ClockDelay(void)
{
    volatile uint32_t i;

    /* 72MHz下该延时大于手册规定的0.2us最小时钟高/低电平时间。 */
    for (i = 0U; i < 12U; ++i) {
        __NOP();
    }
}

void HX711_Init(void)
{
    GPIO_InitTypeDef gpio;

    RCC_APB2PeriphClockCmd(HX711_GPIO_CLOCK, ENABLE);

    gpio.GPIO_Pin = HX711_SCK_PIN;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(HX711_GPIO_PORT, &gpio);
    GPIO_ResetBits(HX711_GPIO_PORT, HX711_SCK_PIN);

    /* 数据手册建议DOUT使用浮空输入，不接内部或外部上下拉。 */
    gpio.GPIO_Pin = HX711_DOUT_PIN;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(HX711_GPIO_PORT, &gpio);
}

void HX711_PowerDown(void)
{
    GPIO_SetBits(HX711_GPIO_PORT, HX711_SCK_PIN);
    BSP_DelayMs(1U); /* 手册规定高电平超过60us即进入断电状态。 */
}

void HX711_PowerUp(void)
{
    /* 空闲状态必须保持SCK为低，否则可能让芯片持续断电。 */
    GPIO_ResetBits(HX711_GPIO_PORT, HX711_SCK_PIN);
}

uint8_t HX711_GetDoutLevel(void)
{
    return (GPIO_ReadInputDataBit(HX711_GPIO_PORT, HX711_DOUT_PIN) != Bit_RESET)
               ? 1U : 0U;
}

HX711_Status HX711_ReadRaw(int32_t *value, uint32_t timeout_ms)
{
    uint32_t data = 0U;
    uint32_t waited_ms = 0U;
    uint32_t primask;
    uint8_t i;

    if (value == 0) {
        return HX711_INVALID_DATA;
    }

    GPIO_ResetBits(HX711_GPIO_PORT, HX711_SCK_PIN);
    /*
     * 每1ms检查一次DOUT，并用局部计数实现有限等待。
     * 这里不依赖SysTick，避免系统时基异常时永久卡在while循环。
     */
    while (GPIO_ReadInputDataBit(HX711_GPIO_PORT, HX711_DOUT_PIN) != Bit_RESET) {
        if (waited_ms >= timeout_ms) {
            return HX711_TIMEOUT;
        }
        BSP_DelayMs(1U);
        ++waited_ms;
    }

    /*
     * 读取期间暂时关闭中断，防止SCK高电平被中断延长到60us以上，
     * 否则HX711会把正常读数误判成断电命令。
     */
    primask = __get_PRIMASK();
    __disable_irq();

    /* HX711从最高位开始输出24位二进制补码。 */
    for (i = 0U; i < 24U; ++i) {
        GPIO_SetBits(HX711_GPIO_PORT, HX711_SCK_PIN);
        HX711_ClockDelay();
        data <<= 1;
        if (GPIO_ReadInputDataBit(HX711_GPIO_PORT, HX711_DOUT_PIN) != Bit_RESET) {
            ++data;
        }
        GPIO_ResetBits(HX711_GPIO_PORT, HX711_SCK_PIN);
        HX711_ClockDelay();
    }

    /* 第25个脉冲选择“下一次”转换为通道A、增益128。 */
    GPIO_SetBits(HX711_GPIO_PORT, HX711_SCK_PIN);
    HX711_ClockDelay();
    GPIO_ResetBits(HX711_GPIO_PORT, HX711_SCK_PIN);

    if (primask == 0U) {
        __enable_irq();
    }

    /* 把24位二进制补码符号扩展为STM32的32位有符号整数。 */
    if ((data & 0x800000UL) != 0U) {
        data |= 0xFF000000UL;
    }
    *value = (int32_t)data;

    /* 接近ADC正负满量程通常表示传感器断线、短路或严重过载。 */
    if ((*value >= 0x7FFFF0L) || (*value <= -0x7FFFF0L)) {
        return HX711_INVALID_DATA;
    }
    return HX711_OK;
}