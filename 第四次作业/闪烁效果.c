#include "LPC11xx.h"

#define SystemCoreClock 48000000

// LED引脚定义
#define LED_PIN 9
#define LED_PORT 1

// 计数器变量，用于累计1ms中断次数
volatile uint32_t timer1_counter = 0;

// 初始化GPIO
void GPIO_Init(void)
{
    // 使能IOCON时钟
    LPC_SYSCON->SYSAHBCLKCTRL |= (1 << 16);
    
    // 配置PIO1_9为GPIO输出
    LPC_IOCON->PIO1_9 &= ~0x07;  // 清除功能位
    LPC_IOCON->PIO1_9 |= 0x00;   // 设置为GPIO
    
    // 设置PIO1_9为输出方向
    LPC_GPIO1->DIR |= (1 << LED_PIN);
    
    // 初始状态为高电平（LED灭）
    LPC_GPIO1->DATA |= (1 << LED_PIN);
}

// 初始化16位定时器1 - 中断方式
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

// 16位定时器1中断服务函数
void TIMER16_1_IRQHandler(void)
{
    // 检查MR0中断标志
    if (LPC_TMR16B1->IR & (1 << 0))
    {
        // 清除MR0中断标志
        LPC_TMR16B1->IR = (1 << 0);
        
        // 计数器加1
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

int main(void)
{
    // 初始化GPIO
    GPIO_Init();
    
    // 初始化16位定时器1
    TMR16B1_Init();
    
    while (1)
    {
        // 主循环为空，所有工作在中断中完成
        __WFI();  // 等待中断，降低功耗
    }
}