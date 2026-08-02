#include "bsp_time.h"
#include "alarm.h"
#include "keys.h"
#include "core_cm3.h" 

// ============ 定义结束 ============
//#include "core_cm3.h"   
//#include "stm32f10x.h"  
typedef struct {
    volatile uint32_t CTRL;      /*!< Offset: 0x000  Control Register */
    volatile uint32_t CYCCNT;    /*!< Offset: 0x004  Cycle Counter Register */
    volatile uint32_t CPICNT;    /*!< Offset: 0x008  CPI Counter Register */
    volatile uint32_t EXCCNT;    /*!< Offset: 0x00C  Exception Overhead Counter */
    volatile uint32_t SLEEPCNT;  /*!< Offset: 0x010  Sleep Counter */
    volatile uint32_t LSUCNT;    /*!< Offset: 0x014  LSU Counter */
    volatile uint32_t FOLDCNT;   /*!< Offset: 0x018  Folded-instruction Counter */
    volatile uint32_t PCSR;      /*!< Offset: 0x01C  Program Counter Sample Register */
} DWT_Type;

/* DWT 基地址 (Cortex-M3 固定地址) */
#define DWT_BASE    (0xE0001000UL)
#define DWT         ((DWT_Type *)DWT_BASE)

/* DWT_CTRL 寄存器的位掩码定义 */
#define DWT_CTRL_CYCCNTENA_Msk    (1UL << 0)   /*!< CYCCNTENA: Cycle Counter Enable */
#define DWT_CTRL_NOCYCCNT_Msk     (1UL << 1)   /*!< NOCYCCNT: No cycle counter */

/* CoreDebug (核心调试) 寄存器结构体定义 */
//typedef struct {
//    volatile uint32_t DHCSR;     /*!< Offset: 0x000  Debug Halting Control and Status */
//    volatile uint32_t DCRSR;     /*!< Offset: 0x004  Debug Core Register Selector */
//    volatile uint32_t DCRDR;     /*!< Offset: 0x008  Debug Core Register Data */
//    volatile uint32_t DEMCR;     /*!< Offset: 0x00C  Debug Exception and Monitor Control */
//} CoreDebug_Type;

/* CoreDebug 基地址 (Cortex-M3 固定地址) */
#define CoreDebug_BASE    (0xE000EDF0UL)
#define CoreDebug         ((CoreDebug_Type *)CoreDebug_BASE)

/* CoreDebug_DEMCR 寄存器的位掩码定义 */
#define CoreDebug_DEMCR_TRCENA_Msk    (1UL << 24)   /*!< TRCENA: Enable trace and debug blocks */

/* 系统时间模块：提供毫秒计时，并调度必须按1ms运行的轻量任务。 */
static volatile uint32_t g_millis;
static uint32_t g_cycles_per_us;

/* SysTick每1ms产生一次中断，作为整个应用的统一软件时基。 */
void BSP_Time_Init(void)
{
    SystemCoreClockUpdate();
    g_cycles_per_us = SystemCoreClock / 1000000U;

    /* Cortex-M3 DWT CYCCNT提供无需中断的硬件周期计数。 */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    SysTick_Config(SystemCoreClock / 1000U);
}

uint32_t BSP_Millis(void)
{
    return g_millis;
}

void BSP_DelayUs(uint32_t us)
{
    uint32_t start;
    uint32_t cycles;

    if ((us == 0U) || (g_cycles_per_us == 0U)) {
        return;
    }
    start = DWT->CYCCNT;
    cycles = us * g_cycles_per_us;
    while ((uint32_t)(DWT->CYCCNT - start) < cycles) {
    }
}
void BSP_DelayMs(uint32_t ms)
{
    /* 分段到1ms，避免us乘法溢出，也允许中断在延时期间正常执行。 */
    while (ms-- > 0U) {
        BSP_DelayUs(1000U);
    }
}

uint8_t BSP_TimebaseCheck(void)
{
    uint32_t before = g_millis;
    BSP_DelayMs(5U);
    return (g_millis != before) ? 1U : 0U;
}

void SysTick_Handler(void)
{
    ++g_millis;
    /* 按键采样和报警节拍放在中断内，避免HX711慢速读取漏掉短按。 */
    Keys_Update(g_millis);
    Alarm_Tick1ms();
}
