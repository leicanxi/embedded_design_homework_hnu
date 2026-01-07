#include "LPC11xx.h"

#define SystemCoreClock 48000000

// LED引脚定义
#define LED_PIN 9
#define LED_PORT 1

// 效果模式定义
#define MODE_BLINK 0
#define MODE_BREATHE 1

// 全局变量
volatile uint32_t timer1_counter = 0;        // 用于闪烁效果的1ms计数器
volatile uint32_t mode_timer = 0;            // 模式切换计时器
volatile uint8_t current_mode = MODE_BLINK;  // 当前模式

// 呼吸效果相关变量
volatile uint32_t pwm_duty = 0;              // 当前PWM占空比
volatile int8_t direction = 1;               // 呼吸方向
#define PWM_PERIOD (SystemCoreClock / 1000)  // PWM周期为1ms
#define STEP_TIME 10                         // 呼吸效果更新间隔(ms)
#define STEP_VALUE (PWM_PERIOD / (1000 / STEP_TIME)) // 每次占空比变化量

// 初始化GPIO
void GPIO_Init(void)
{
    // 使能IOCON时钟
    LPC_SYSCON->SYSAHBCLKCTRL |= (1 << 16);
    
    // 配置PIO1_9为GPIO输出（用于闪烁效果）
    LPC_IOCON->PIO1_9 &= ~0x07;  // 清除功能位
    LPC_IOCON->PIO1_9 |= 0x00;   // 设置为GPIO
    
    // 设置PIO1_9为输出方向
    LPC_GPIO1->DIR |= (1 << LED_PIN);
    
    // 初始状态为高电平（LED灭）
    LPC_GPIO1->DATA |= (1 << LED_PIN);
}

// 初始化16位定时器1 - 用于闪烁效果（1ms中断）
void TMR16B1_Init(void)
{
    // 使能16位定时器1时钟
    LPC_SYSCON->SYSAHBCLKCTRL |= (1 << 8);
    
    // 定时器复位
    LPC_TMR16B1->TCR = 0x02;
    
    // 设置预分频器，48MHz / (47999+1) = 1kHz (1ms)
    LPC_TMR16B1->PR = 47999;
    
    // 匹配寄存器0：每1ms匹配一次
    LPC_TMR16B1->MR0 = 1;
    
    // 匹配控制：MR0匹配时复位TC并产生中断
    LPC_TMR16B1->MCR = (1 << 0) | (1 << 1);  // 中断 + 复位
    
    // 清除所有中断标志
    LPC_TMR16B1->IR = 0x1F;
    
    // 使能定时器1中断
    NVIC_EnableIRQ(TIMER_16_1_IRQn);
    
    // 启动定时器
    LPC_TMR16B1->TCR = 0x01;
}

// 初始化32位定时器0 - 用于模式切换计时（1ms中断）
void TMR32B0_Init(void)
{
    LPC_SYSCON->SYSAHBCLKCTRL |= (1UL << 9);  // 使能定时器0时钟
    LPC_TMR32B0->PR = 0;                      // 预分频为0
    LPC_TMR32B0->MR0 = SystemCoreClock / 1000; // 1ms中断周期
    LPC_TMR32B0->MCR = 3;                     // 匹配时复位并产生中断
    LPC_TMR32B0->TCR = 1;                     // 启动定时器
    NVIC_EnableIRQ(TIMER_32_0_IRQn);          // 使能中断
}

// 初始化16位定时器1的PWM模式 - 用于呼吸效果
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

// 呼吸效果更新函数
void update_breathe_effect(void)
{
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

// 切换效果模式
void switch_mode(uint8_t new_mode)
{
    if (current_mode == new_mode) return;
    
    current_mode = new_mode;
    
    if (new_mode == MODE_BLINK)
    {
        // 切换到闪烁模式
        // 禁用PWM，恢复GPIO功能
        LPC_IOCON->PIO1_9 &= ~0x07;
        LPC_IOCON->PIO1_9 |= 0x00;
        
        // 重新配置为GPIO输出
        LPC_GPIO1->DIR |= (1 << LED_PIN);
        LPC_GPIO1->DATA |= (1 << LED_PIN); // LED初始状态为灭
        
        // 重新初始化16位定时器1用于闪烁（1ms中断）
        TMR16B1_Init();
        
        // 重置闪烁效果变量
        timer1_counter = 0;
    }
    else // MODE_BREATHE
    {
        // 切换到呼吸模式
        // 禁用16位定时器1中断（闪烁效果）
        NVIC_DisableIRQ(TIMER_16_1_IRQn);
        LPC_TMR16B1->TCR = 0; // 停止定时器
        
        // 初始化PWM输出
        TMR16B1_PWM_Init();
        
        // 重置呼吸效果变量
        pwm_duty = 0;
        direction = 1;
        LPC_TMR16B1->MR0 = 0; // 初始占空比为0
    }
}

// 16位定时器1中断服务函数 - 用于闪烁效果
void TIMER16_1_IRQHandler(void)
{
    if (LPC_TMR16B1->IR & (1 << 0))
    {
        // 清除MR0中断标志
        LPC_TMR16B1->IR = (1 << 0);
        
        // 闪烁效果计数器
        timer1_counter++;
        
        // 每1000次中断（1秒）翻转LED
        if (timer1_counter >= 1000)
        {
            timer1_counter = 0;
            // 翻转PIO1_9
            LPC_GPIO1->DATA ^= (1 << LED_PIN);
        }
    }
}

// 32位定时器0中断服务函数 - 用于模式切换计时和呼吸效果更新
void TIMER32_0_IRQHandler(void)
{
    static uint32_t breathe_counter = 0;
    
    if (LPC_TMR32B0->IR & 0x01)
    {
        LPC_TMR32B0->IR = 0x01; // 清除中断标志

        // 模式切换计时（每5秒切换一次模式）
        mode_timer++;
        if (mode_timer >= 5000) // 5秒
        {
            mode_timer = 0;
            if (current_mode == MODE_BLINK)
                switch_mode(MODE_BREATHE);
            else
                switch_mode(MODE_BLINK);
        }
        
        // 呼吸效果更新（每10ms更新一次）
        if (current_mode == MODE_BREATHE)
        {
            breathe_counter++;
            if (breathe_counter >= STEP_TIME) // 10ms
            {
                breathe_counter = 0;
                update_breathe_effect();
            }
        }
    }
}

int main(void)
{
    SystemCoreClockUpdate(); // 更新系统时钟频率
    
    // 初始化GPIO
    GPIO_Init();
    
    // 初始化32位定时器0用于模式切换计时（始终运行）
    TMR32B0_Init();
    
    // 初始化为闪烁模式
    switch_mode(MODE_BLINK);
    
    while (1)
    {
        __WFI(); // 等待中断，降低功耗
    }
}