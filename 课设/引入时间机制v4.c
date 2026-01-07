#include "lpc11xx.h"
#include <stdio.h>

// 引脚定义
#define FLASH_CS_HIGH()   LPC_GPIO2->DATA |= (1<<0)
#define FLASH_CS_LOW()    LPC_GPIO2->DATA &= ~(1<<0)

// Flash指令
#define FLASH_WriteEnable  0x06 
#define FLASH_ReadStatusReg  0x05 
#define FLASH_ReadData  0x03 
#define FLASH_PageProgram  0x02 
#define FLASH_SectorErase  0x20 
#define FLASH_ChipErase  0xC7 

// 全局变量
uint32_t flash_address = 0;
uint8_t recording_enabled = 0;
uint16_t record_count = 0;
uint8_t flash_erase_requested = 0;
uint32_t erase_start_address = 0;
uint16_t erase_start_count = 0;

// 温度报警阈值（30°C对应的原始数据值）
#define TEMP_THRESHOLD_RAW 0x1E00  // 30°C对应的近似原始值

// 魔数标记，用于识别有效温度数据
#define FLASH_MAGIC_NUMBER 0x544D5020  // "TMP " 的 ASCII 十六进制表示
#define INFO_DATA_SIZE 10              // 4字节魔数 + 4字节地址 + 2字节计数
#define DATA_START_ADDRESS 0x00000A    // 温度数据起始地址（INFO_DATA_SIZE之后）

// DS1307时间相关全局变量
uint8_t current_seconds, current_minutes, current_hours, current_date, current_month, current_year;

/************************LED函数*************************/
void LED_Init(void)
{
    LPC_GPIO1->DIR |= (1<<9);  // 设置P1.9为输出
    LPC_GPIO1->DATA |= (1<<9); // 初始状态LED熄灭
}

void LED_On(void)
{
    LPC_GPIO1->DATA &= ~(1<<9); // 点亮LED
}

void LED_Off(void)
{
    LPC_GPIO1->DATA |= (1<<9);  // 熄灭LED
}

/************************SPI函数*************************/
uint8_t SPI_ExchangeByte(uint8_t tx_data)
{  
    while((LPC_SSP1->SR & (1<<4)) == (1<<4));  // 等待不忙
    LPC_SSP1->DR = tx_data;
    while((LPC_SSP1->SR & (1<<2)) != (1<<2));  // 等待接收完成
    return LPC_SSP1->DR;
}

void SPI_Init(void)
{
    uint8_t i;
    
    // 使能SSP1时钟
    LPC_SYSCON->PRESETCTRL |= (1<<2);
    LPC_SYSCON->SYSAHBCLKCTRL |= (1<<18);
    LPC_SYSCON->SSP1CLKDIV = 0x06;
    
    // 配置SPI引脚
    LPC_SYSCON->SYSAHBCLKCTRL |= (1<<16);
    LPC_IOCON->PIO2_1 = 0x02;  // SCK
    LPC_IOCON->PIO2_2 = 0x02;  // MISO  
    LPC_IOCON->PIO2_3 = 0x02;  // MOSI
    LPC_SYSCON->SYSAHBCLKCTRL &= ~(1<<16);
    
    // 配置SSP1
    LPC_SSP1->CR0 = 0x01C7;  // 8位数据，SPI模式0
    LPC_SSP1->CPSR = 0x04;   // 预分频
    LPC_SSP1->CR1 = (1<<1);  // 使能SSP
    
    // 清空FIFO
    for(i = 0; i < 8; i++)
    {
        volatile uint8_t clear = LPC_SSP1->DR;
    }   
}

/************************系统时钟*************************/
void SystemClock_Config(void)
{
    LPC_SYSCON->SYSPLLCLKSEL = 0x1;
    LPC_SYSCON->SYSPLLCLKUEN = 0x0;
    LPC_SYSCON->SYSPLLCLKUEN = 0x1;
    
    LPC_SYSCON->SYSPLLCTRL = 0x03;
    while(!(LPC_SYSCON->SYSPLLSTAT & 0x1));
    
    LPC_SYSCON->MAINCLKSEL = 0x3;
    LPC_SYSCON->MAINCLKUEN = 0x0;
    LPC_SYSCON->MAINCLKUEN = 0x1;
    
    SystemCoreClock = 48000000;
}

/************************UART函数*************************/
void UART_Init(void)
{
    uint32_t divisor;
    
    // 配置UART引脚
    LPC_SYSCON->SYSAHBCLKCTRL |= (1<<16);
    LPC_IOCON->PIO1_6 = 0x01;  // RXD
    LPC_IOCON->PIO1_7 = 0x01;  // TXD
    LPC_SYSCON->SYSAHBCLKCTRL &= ~(1<<16);
    
    // UART时钟配置
    LPC_SYSCON->UARTCLKDIV = 0x1;
    LPC_SYSCON->SYSAHBCLKCTRL |= (1<<12);
    
    // 波特率115200
    divisor = (SystemCoreClock + 8 * 115200) / (16 * 115200);
    
    LPC_UART->LCR = 0x83;
    LPC_UART->DLM = divisor / 256;
    LPC_UART->DLL = divisor % 256;
    LPC_UART->LCR = 0x03;
    LPC_UART->FCR = 0x07;
}

void UART_SendByte(uint8_t data)
{
    LPC_UART->THR = data;
    while(!(LPC_UART->LSR & (1<<5)));
}

void UART_SendString(const char *str)
{
    while(*str)
    {
        UART_SendByte(*str++);
    }
}

/************************Flash函数*************************/
void Flash_Init(void)
{
    LPC_GPIO2->DIR |= (1<<0);
    FLASH_CS_HIGH();
    SPI_Init();
}  

uint8_t Flash_ReadStatus(void)
{  
    uint8_t status;
    FLASH_CS_LOW();
    SPI_ExchangeByte(FLASH_ReadStatusReg);
    status = SPI_ExchangeByte(0xFF);
    FLASH_CS_HIGH();
    return status;
}

void Flash_WriteEnable(void)
{
    FLASH_CS_LOW();
    SPI_ExchangeByte(FLASH_WriteEnable);
    FLASH_CS_HIGH();
}

void Flash_WaitBusy(void)
{   
    while((Flash_ReadStatus() & 0x01) == 0x01);
}

void Flash_ReadData(uint8_t* buffer, uint32_t address, uint16_t length)
{ 
    uint16_t i;  
    FLASH_CS_LOW();
    SPI_ExchangeByte(FLASH_ReadData);
    SPI_ExchangeByte((uint8_t)(address >> 16));
    SPI_ExchangeByte((uint8_t)(address >> 8));   
    SPI_ExchangeByte((uint8_t)address);   
    for(i = 0; i < length; i++)
    { 
        buffer[i] = SPI_ExchangeByte(0xFF);
    }
    FLASH_CS_HIGH();
}  

void Flash_WritePage(uint8_t* data, uint32_t address, uint16_t length)
{
    uint16_t i;  
    Flash_WriteEnable();
    FLASH_CS_LOW();
    SPI_ExchangeByte(FLASH_PageProgram);
    SPI_ExchangeByte((uint8_t)(address >> 16));
    SPI_ExchangeByte((uint8_t)(address >> 8));   
    SPI_ExchangeByte((uint8_t)address);   
    for(i = 0; i < length; i++)
    {
        SPI_ExchangeByte(data[i]);
    }
    FLASH_CS_HIGH();
    Flash_WaitBusy();
} 

void Flash_EraseChip(void)
{   
    Flash_WriteEnable();
    Flash_WaitBusy();   
    FLASH_CS_LOW();
    SPI_ExchangeByte(FLASH_ChipErase);
    FLASH_CS_HIGH();
    Flash_WaitBusy();
}

// 检查Flash中的数据是否有效（通过魔数标记）
uint8_t Flash_IsValidData(void)
{
    uint8_t info_data[4];
    uint32_t magic;
    
    // 从Flash的起始位置读取魔数
    Flash_ReadData(info_data, 0x000000, 4);
    
    // 组合魔数
    magic = (info_data[0] << 24) | (info_data[1] << 16) | (info_data[2] << 8) | info_data[3];
    
    return (magic == FLASH_MAGIC_NUMBER);
}

// 读取Flash中的记录计数和地址（从固定位置读取）
void Flash_ReadRecordInfo(uint32_t *address, uint16_t *count)
{
    uint8_t info_data[6];  // 4字节地址 + 2字节计数
    
    // 从Flash的固定位置读取记录信息（跳过4字节魔数）
    Flash_ReadData(info_data, 0x000004, 6);
    
    // 组合地址和计数
    *address = (info_data[0] << 24) | (info_data[1] << 16) | (info_data[2] << 8) | info_data[3];
    *count = (info_data[4] << 8) | info_data[5];
}

// 保存记录计数和地址到Flash（保存到固定位置）
void Flash_SaveRecordInfo(uint32_t address, uint16_t count)
{
    uint8_t info_data[INFO_DATA_SIZE];
    
    // 准备数据：魔数 + 地址 + 计数
    info_data[0] = (FLASH_MAGIC_NUMBER >> 24) & 0xFF;
    info_data[1] = (FLASH_MAGIC_NUMBER >> 16) & 0xFF;
    info_data[2] = (FLASH_MAGIC_NUMBER >> 8) & 0xFF;
    info_data[3] = FLASH_MAGIC_NUMBER & 0xFF;
    info_data[4] = (address >> 24) & 0xFF;
    info_data[5] = (address >> 16) & 0xFF;
    info_data[6] = (address >> 8) & 0xFF;
    info_data[7] = address & 0xFF;
    info_data[8] = (count >> 8) & 0xFF;
    info_data[9] = count & 0xFF;
    
    // 保存到Flash的起始位置
    Flash_WritePage(info_data, 0x000000, INFO_DATA_SIZE);
}

/************************I2C温度传感器函数*************************/
void I2C_Init(void)
{
    // 使能I2C
    LPC_SYSCON->PRESETCTRL |= (1<<1);
    LPC_SYSCON->SYSAHBCLKCTRL |= (1<<5);
    
    // 配置I2C引脚
    LPC_SYSCON->SYSAHBCLKCTRL |= (1<<16);
    LPC_IOCON->PIO0_4 = 0x01;  // SDA
    LPC_IOCON->PIO0_5 = 0x01;  // SCL
    LPC_SYSCON->SYSAHBCLKCTRL &= ~(1<<16);
    
    // 100kHz
    LPC_I2C->SCLH = 250;
    LPC_I2C->SCLL = 250;
    
    LPC_I2C->CONCLR = 0xFF;
    LPC_I2C->CONSET |= (1<<6);  // 使能I2C
}

void I2C_Start(void)
{
    LPC_I2C->CONSET |= (1<<5);
    while(!(LPC_I2C->CONSET & (1<<3)));
    LPC_I2C->CONCLR = (1<<5) | (1<<3);
}

void I2C_Stop(void)
{
    LPC_I2C->CONCLR = (1<<3);
    LPC_I2C->CONSET |= (1<<4);
    while(LPC_I2C->CONSET & (1<<4));
}

void I2C_SendByte(uint8_t data)
{
    uint16_t timeout = 20000;
    LPC_I2C->DAT = data;
    LPC_I2C->CONCLR = (1<<3);
    while((!(LPC_I2C->CONSET & (1<<3))) && (timeout--));  
}

uint8_t I2C_ReceiveByte(void)
{
    uint8_t data;
    uint16_t timeout = 20000;
    LPC_I2C->CONSET = (1<<2);
    LPC_I2C->CONCLR = (1<<3);
    while((!(LPC_I2C->CONSET & (1<<3))) && (timeout--));  
    data = (uint8_t)LPC_I2C->DAT;
    return data;
}

// 读取原始温度数据（16位）
int16_t ReadRawTemperature(void)
{  
    uint8_t high_byte, low_byte;
    int16_t raw_temp;
    
    I2C_Start();   
    I2C_SendByte(0x91);  // LM75地址 + 读  
    high_byte = I2C_ReceiveByte();   
    low_byte = I2C_ReceiveByte(); 
    I2C_Stop();  
    
    raw_temp = (high_byte << 8) | low_byte;
    return raw_temp;
}

// 原始数据转摄氏度
float ConvertToCelsius(int16_t raw_temp)
{
    int16_t temp_data = raw_temp >> 5;
    
    if(temp_data & 0x0400) {
        // 负数处理
        temp_data = -(~(temp_data & 0x03FF) + 1);
    }
    
    return temp_data * 0.125f;
}

/************************DS1307实时时钟函数*************************/
// I2C发送ACK
void I2C_Ack(void)
{
    LPC_I2C->CONSET = (1<<2); // AA=1
}

// I2C发送NACK
void I2C_NAck(void)
{
    LPC_I2C->CONCLR = (1<<2); // AAC=1
}

// BCD转十进制
uint8_t BCD_to_Decimal(uint8_t bcd)
{
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

// 十进制转BCD
uint8_t Decimal_to_BCD(uint8_t decimal)
{
    return ((decimal / 10) << 4) | (decimal % 10);
}

// DS1307初始化
void DS1307_Init(void)
{
    // 启动DS1307时钟 (清除CH位)
    I2C_Start();
    I2C_SendByte(0xD0); // DS1307写地址
    I2C_SendByte(0x00); // 秒寄存器地址
    I2C_SendByte(0x00); // 秒=0, CH=0 (启动时钟)
    I2C_Stop();
    
    // 设置初始时间: 2024年11月29日 12:00:00 星期五
    I2C_Start();
    I2C_SendByte(0xD0); // DS1307写地址
    I2C_SendByte(0x00); // 从秒寄存器开始
    
    // 秒 (00), 分 (00), 时 (12), 星期 (05), 日 (29), 月 (11), 年 (24)
    I2C_SendByte(Decimal_to_BCD(0));   // 秒
    I2C_SendByte(Decimal_to_BCD(0));   // 分
    I2C_SendByte(Decimal_to_BCD(12));  // 时 (12小时模式)
    I2C_SendByte(Decimal_to_BCD(5));   // 星期 (星期五)
    I2C_SendByte(Decimal_to_BCD(29));  // 日
    I2C_SendByte(Decimal_to_BCD(11));  // 月
    I2C_SendByte(Decimal_to_BCD(24));  // 年
    
    I2C_Stop();
}

// 读取DS1307时间到全局变量
void Read_DS1307_Time(void)
{
    uint8_t seconds, minutes, hours, day, date, month, year;
    
    // 设置读取起始地址
    I2C_Start();
    I2C_SendByte(0xD0); // DS1307写地址
    I2C_SendByte(0x00); // 从秒寄存器开始
    I2C_Stop();
    
    // 读取时间数据
    I2C_Start();
    I2C_SendByte(0xD1); // DS1307读地址
    
    I2C_Ack();
    seconds = I2C_ReceiveByte(); // 秒
    
    I2C_Ack();
    minutes = I2C_ReceiveByte(); // 分
    
    I2C_Ack();
    hours = I2C_ReceiveByte();   // 时
    
    I2C_Ack();
    day = I2C_ReceiveByte();     // 星期
    
    I2C_Ack();
    date = I2C_ReceiveByte();    // 日
    
    I2C_Ack();
    month = I2C_ReceiveByte();   // 月
    
    I2C_NAck();
    year = I2C_ReceiveByte();    // 年
    
    I2C_Stop();
    
    // 转换为十进制并存储到全局变量
    current_seconds = BCD_to_Decimal(seconds & 0x7F); // 去掉CH位
    current_minutes = BCD_to_Decimal(minutes);
    current_hours = BCD_to_Decimal(hours & 0x3F);     // 12小时模式
    current_date = BCD_to_Decimal(date);
    current_month = BCD_to_Decimal(month);
    current_year = BCD_to_Decimal(year);
}

// 显示当前时间
void Display_Current_Time(void)
{
    // 显示时间 HH:MM:SS
    UART_SendByte('0' + current_hours / 10);
    UART_SendByte('0' + current_hours % 10);
    UART_SendString(":");
    UART_SendByte('0' + current_minutes / 10);
    UART_SendByte('0' + current_minutes % 10);
    UART_SendString(":");
    UART_SendByte('0' + current_seconds / 10);
    UART_SendByte('0' + current_seconds % 10);
    
    // 显示日期 20YY-MM-DD
    UART_SendString(" 20");
    UART_SendByte('0' + current_year / 10);
    UART_SendByte('0' + current_year % 10);
    UART_SendString("-");
    UART_SendByte('0' + current_month / 10);
    UART_SendByte('0' + current_month % 10);
    UART_SendString("-");
    UART_SendByte('0' + current_date / 10);
    UART_SendByte('0' + current_date % 10);
}

/************************温度记录功能*************************/
// 保存原始温度数据到Flash
void SaveTemperatureData(int16_t raw_temp)
{
    uint8_t temp_data[2];
    
    // 将16位温度数据拆分为2个字节
    temp_data[0] = (raw_temp >> 8) & 0xFF;
    temp_data[1] = raw_temp & 0xFF;
    
    Flash_WritePage(temp_data, flash_address, 2);
    
    flash_address += 2;
    record_count++;
    
    // 保存当前的记录信息到Flash
    Flash_SaveRecordInfo(flash_address, record_count);
    
    // 防止地址溢出 - 设置擦除标志而不是立即擦除
    if(flash_address >= 0x1FFFE)
    {
        UART_SendString("Flash nearly full, requesting erase...\r\n");
        
        // 设置擦除标志和参数，不在中断中执行擦除
        flash_erase_requested = 1;
        erase_start_address = DATA_START_ADDRESS;
        erase_start_count = 0;
        
        // 暂时停止记录，等待擦除完成
        recording_enabled = 0;
    }
}

// 修改DisplayAllRecords函数中的温度显示部分
void DisplayAllRecords(void)
{
    uint8_t temp_data[2];
    int16_t raw_temp;
    float temp_c;
    uint32_t address = DATA_START_ADDRESS;
    uint16_t i;
    
    UART_SendString("\r\n=== Temperature Records ===\r\n");
    
    for(i = 0; i < record_count; i++)
    {
        Flash_ReadData(temp_data, address, 2);
        
        // 组合16位数据
        raw_temp = (temp_data[0] << 8) | temp_data[1];
        temp_c = ConvertToCelsius(raw_temp);
        
        // 显示记录
        UART_SendString("Record ");
        if(i < 1000) UART_SendByte('0');
        if(i < 100) UART_SendByte('0'); 
        if(i < 10) UART_SendByte('0');
        
        // 简化显示索引
        UART_SendByte('0' + i % 10);
        UART_SendString(": ");
        
        // 修改为显示三位小数
        if(temp_c >= 0) UART_SendString("+");
        else UART_SendString("-");
        
        int display_value = (int)(temp_c * 1000);
        if(display_value < 0) display_value = -display_value;
        
        UART_SendByte('0' + display_value / 100000 % 10);
        UART_SendByte('0' + display_value / 10000 % 10);
        UART_SendByte('0' + display_value / 1000 % 10);
        UART_SendByte('.');
        UART_SendByte('0' + display_value / 100 % 10);
        UART_SendByte('0' + display_value / 10 % 10);
        UART_SendByte('0' + display_value % 10);
        UART_SendString("C\r\n");
        
        address += 2;
    }
    
    UART_SendString("Total records: ");
    UART_SendByte('0' + record_count / 1000 % 10);
    UART_SendByte('0' + record_count / 100 % 10);
    UART_SendByte('0' + record_count / 10 % 10);
    UART_SendByte('0' + record_count % 10);
    UART_SendString("\r\n");
}

/************************定时器中断*************************/
void Timer_Init(void)
{   
    // 使能32位定时器0时钟
    LPC_SYSCON->SYSAHBCLKCTRL |= (1UL << 9);
    
    // 清除所有中断标志
    LPC_TMR32B0->IR = 0x1F;
    
    // 设置预分频器
    LPC_TMR32B0->PR = SystemCoreClock/1000 - 1; // 1ms计数
    
    // 设置匹配寄存器 (1000ms = 1s)
    LPC_TMR32B0->MR0 = 1000;
    
    // 配置匹配控制 (匹配时复位TC并产生中断)
    LPC_TMR32B0->MCR = 0x03;
    
    // 启动定时器
    LPC_TMR32B0->TCR = 0x01;
    
    // 使能定时器中断
    NVIC_EnableIRQ(TIMER_32_0_IRQn);
}

// 定时器中断服务函数
void TIMER32_0_IRQHandler(void)
{  
    static uint8_t time_update_counter = 0;
    
    if(LPC_TMR32B0->IR & 0x01)
    {
        LPC_TMR32B0->IR = 0x01;  // 清除中断标志
        
        // 检查串口命令
        if(LPC_UART->LSR & (1<<0))
        {
            uint8_t command = LPC_UART->RBR;
            
            if((command == 's' || command == 'S') && !recording_enabled && !flash_erase_requested)
            {
                recording_enabled = 1;
                UART_SendString("Start recording\r\n");
            }
            else if((command == 't' || command == 'T') && recording_enabled)
            {
                recording_enabled = 0;
                UART_SendString("Stop recording\r\n");
                DisplayAllRecords();
            }
            else if(command == 'r' || command == 'R')
            {
                DisplayAllRecords();
            }
            else if(command == 'e' || command == 'E')
            {
                // 手动擦除请求
                flash_erase_requested = 1;
                erase_start_address = DATA_START_ADDRESS;
                erase_start_count = 0;
                recording_enabled = 0;
                UART_SendString("Flash erase requested\r\n");
            }
        }
        
         Read_DS1307_Time();  // 读取时间到全局变量
 
        
        // 每次中断都显示当前时间（从全局变量读取）
        Display_Current_Time();
        UART_SendString("  ");
        
        // 读取并显示当前温度
        int16_t raw_temp = ReadRawTemperature();
        float current_temp = ConvertToCelsius(raw_temp);
        
        UART_SendString("Temp: ");
        if(current_temp >= 0) UART_SendString("+");
        else UART_SendString("-");
        
        // 修改为显示三位小数
        int display_value = (int)(current_temp * 1000);
        if(display_value < 0) display_value = -display_value;
        
        UART_SendByte('0' + display_value / 100000 % 10);
        UART_SendByte('0' + display_value / 10000 % 10);
        UART_SendByte('0' + display_value / 1000 % 10);
        UART_SendByte('.');
        UART_SendByte('0' + display_value / 100 % 10);
        UART_SendByte('0' + display_value / 10 % 10);
        UART_SendByte('0' + display_value % 10);
        UART_SendString("C");
        
        // 温度阈值检测和LED报警
        if(raw_temp >= TEMP_THRESHOLD_RAW)
        {
            LED_On();
            UART_SendString(" [ALARM!]");
        }
        else
        {
            LED_Off();
        }
        UART_SendString("\r\n");
        
        // 如果正在记录且没有擦除请求，保存数据
        if(recording_enabled && !flash_erase_requested)
        {
            SaveTemperatureData(raw_temp);
        }
    }
}

/************************主函数*************************/
int main(void)
{
    // 系统初始化
    SystemClock_Config();
    UART_Init();
    Flash_Init();
    I2C_Init();
    LED_Init();
    DS1307_Init();  // 初始化DS1307实时时钟
    
    // 初始读取一次时间
    Read_DS1307_Time();
    
    // 显示欢迎信息
    UART_SendString("\r\nTemperature Monitor with RTC Time Display\r\n");
    UART_SendString("Commands: s=start, t=stop, r=show records, e=erase flash\r\n");
    
    // 检查Flash中的数据是否有效
    if(Flash_IsValidData())
    {
        // 有效数据，读取之前的记录信息
        Flash_ReadRecordInfo(&flash_address, &record_count);
        UART_SendString("Valid temperature data found, resuming...\r\n");
        UART_SendString("Records count: ");
        UART_SendByte('0' + record_count / 1000 % 10);
        UART_SendByte('0' + record_count / 100 % 10);
        UART_SendByte('0' + record_count / 10 % 10);
        UART_SendByte('0' + record_count % 10);
        UART_SendString("\r\n");
    }
    else
    {
        // 无效数据，重新初始化
        UART_SendString("No valid temperature data found, initializing...\r\n");
        Flash_EraseChip();
        flash_address = DATA_START_ADDRESS;
        record_count = 0;
        Flash_SaveRecordInfo(flash_address, record_count);
        UART_SendString("Flash initialized for temperature recording\r\n");
    }
    
    UART_SendString("System ready\r\n");
    
    // 启动定时器
    Timer_Init();
    
    while(1)
    {
        // 检查并处理Flash擦除请求
        if(flash_erase_requested)
        {
            UART_SendString("Performing flash erase...\r\n");
            
            // 在主循环中执行耗时的擦除操作
            Flash_EraseChip();
            Flash_WaitBusy();
            
            // 重置地址和计数
            flash_address = erase_start_address;
            record_count = erase_start_count;
            
            // 重新写入魔数和初始信息
            Flash_SaveRecordInfo(flash_address, record_count);
            
            // 清除擦除标志
            flash_erase_requested = 0;
            
            UART_SendString("Flash erase completed. Ready for recording.\r\n");
            UART_SendString("Send 's' to start recording.\r\n");
        }
        
        __WFI();  // 等待中断
    }
    
    return 0;
}