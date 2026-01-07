#include "LPC11xx.h"

#define PWM_PERIOD  (SystemCoreClock / 1000)  // PWM周期为1ms
#define STEP_TIME   10                        // 32位定时器0中断间隔（ms）
#define STEP_VALUE  (PWM_PERIOD / (1000 / STEP_TIME))  // 每次占空比变化量

volatile uint32_t pwm_duty = 0;               // 当前PWM占空比（匹配值）
volatile int8_t direction = 1;                // 方向：1增加，-1减少

// 初始化32位定时器0（用于控制呼吸频率）
void TMR32B0_Init(void)
{
    LPC_SYSCON->SYSAHBCLKCTRL |= (1UL << 9);  // 使能定时器0时钟
    LPC_TMR32B0->PR = 0;                      // 预分频为0
    LPC_TMR32B0->MR0 = SystemCoreClock / (1000 / STEP_TIME); // 设置中断周期
    LPC_TMR32B0->MCR = 3;                     // 匹配时复位并产生中断
    LPC_TMR32B0->TCR = 1;                     // 启动定时器
    NVIC_EnableIRQ(TIMER_32_0_IRQn);          // 使能中断
}

// 初始化16位定时器1（用于PWM输出）
void TMR16B1_PWM_Init(void)
{
    LPC_SYSCON->SYSAHBCLKCTRL |= (1UL << 8) | (1UL << 16); // 使能定时器1和IOCON时钟
    LPC_IOCON->PIO1_9 &= ~0x07;               // 清除PIO1_9设置
    LPC_IOCON->PIO1_9 |= 0x01;                // 设置为MAT0功能（PWM输出）

    LPC_TMR16B1->TCR = 0x02;                  // 复位定时器
    LPC_TMR16B1->PR = 0;                      // 预分频为0
    LPC_TMR16B1->PWMC = 0x01;                 // 使能MAT0为PWM输出
    LPC_TMR16B1->MCR = 0x02 << 9;             // MR3匹配时复位TC
    LPC_TMR16B1->MR3 = PWM_PERIOD;            // 设置PWM周期
    LPC_TMR16B1->MR0 = 0;                     // 初始占空比为0
    LPC_TMR16B1->TCR = 0x01;                  // 启动定时器
}

// 32位定时器0中断服务函数（控制PWM占空比变化）
void TIMER32_0_IRQHandler(void)
{
    if (LPC_TMR32B0->IR & 0x01)               // 检查MR0中断
    {
        LPC_TMR32B0->IR = 0x01;               // 清除中断标志

        // 更新PWM占空比
        pwm_duty += direction * STEP_VALUE;

        // 边界判断，反转方向
        if (pwm_duty >= PWM_PERIOD)
        {
            pwm_duty = PWM_PERIOD;
            direction = -1;
        }
        else if (pwm_duty <= 0)
        {
            pwm_duty = 0;
            direction = 1;
        }

        // 更新PWM输出
        LPC_TMR16B1->MR0 = pwm_duty;
    }
}

int main(void)
{
    SystemCoreClockUpdate();                  // 更新系统时钟频率（48MHz）

    TMR16B1_PWM_Init();                       // 初始化PWM输出
    TMR32B0_Init();                           // 初始化呼吸控制定时器

    while (1)
    {
        __WFI();                              // 等待中断
    }
}