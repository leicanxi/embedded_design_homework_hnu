#include <LPC11xx.h>

void UARTInit();

void I2CInit();

void TMR16B0_Init();

void SSPI1_Init();

void Delay_1s(void);

void DS1307Init();

void Led_off();

void Led_on();

/*主函数*/

int main(){

Led_on();

      SystemInit();//  主时钟设置成48Mhz

  UARTInit();

       I2CInit();

       SSPI1_Init();

       Delay_1s();

  DS1307Init();

       TMR16B0_Init();

       while(1){

       }

}

#include <LPC11xx.h>

void UART_Send();

void Get_temputerature();

void UART_Send_Bit();

//void SSP1_Send(uint8_t *buf,uint8_t Length);

void SPI1_Write_FLASH(uint8_t *data, uint8_t Length);

void SPI1_Read_FLASH(uint8_t *data,uint8_t Length);

void XT25_EraseSector();

void Delay_1s();

long int ADDR;

void XT25_EraseAll();

uint8_t buf[100]; //存放温度

void Led_off();

void Led_on();

/*LED亮*/

void Led_on(){

       LPC_SYSCON->SYSAHBCLKCTRL |= (1<<6); // 使能时钟

       LPC_GPIO1->DIR |= (1<<9);

       LPC_GPIO1->DATA &= (0<<9); //拉低

}

/*LED灭*/

void Led_off() {

  LPC_SYSCON->SYSAHBCLKCTRL |= (1<<6); // 使能时钟

       LPC_GPIO1->DIR |= (1<<9);

       LPC_GPIO1->DATA |= (1<<9); //拉高

}

void DS1307_Read(uint8_t *data);

/*

*函数功能：16B0 初始化

*计时1s

*/

void TMR16B0_Init(){//定时器初始化 定时1s

       LPC_SYSCON->SYSAHBCLKCTRL |=(1<<7);//使能16B0

       LPC_TMR16B0->MCR =3; //匹配MR0时复位TC且中断

       LPC_TMR16B0->PR=799;   //预分频值 3999

       LPC_TMR16B0->MR0=SystemCoreClock/800; //  设置周期为1秒

       LPC_TMR16B0->TCR=0X01;  //启动定时

       NVIC_EnableIRQ(TIMER_16_0_IRQn); //启动中断

}

/*

*16B0中断函数

*/

void TIMER16_0_IRQHandler(){

        int i;

        int n;

        uint8_t data[16]={0};

        uint8_t shijian[7]={17,20.00};

        uint8_t time[8]={0};

        DS1307_Read(shijian);

        time[0]=(shijian[0]&0x0F)+48;           //时间转换成ascii码进行保存

        time[1]=(shijian[0]>>4)+48;

        time[2]=(shijian[1]&0x0F)+48;

        time[3]=(shijian[1]>>4)+48;

        time[4]=(shijian[2]&0x0F)+48;

        time[5]=(shijian[2]>>4)+48;

        UART_Send_Bit(time[5]);

        UART_Send_Bit(time[4]);

        UART_Send("hou",3);

        UART_Send_Bit(':');

        UART_Send_Bit(time[3]);

        UART_Send_Bit(time[2]);

        UART_Send("min",3);

        UART_Send_Bit(':');

        UART_Send_Bit(time[1]);

        UART_Send_Bit(time[0]);

   UART_Send("sec",3);

//发送换行

        UART_Send_Bit(0x0d);

        UART_Send_Bit(0x0a);

        Get_temputerature();

//    Delay_1s();

        SPI1_Write_FLASH(buf,7);//  发送温度 以转换好的数组的形式发送 到xt52

        SPI1_Read_FLASH(data,7);

        UART_Send("Temputerature=",14);

        UART_Send(data,7);  //发送温度到pc

        UART_Send_Bit('C');

//发送换行

        UART_Send_Bit(0x0d);

        UART_Send_Bit(0x0a);

        ADDR=ADDR+8;

        LPC_TMR16B0->IR |=0X01; //清中断

}

#include <LPC11xx.h>

uint32_t Rcv_Buf[100]; //存放数据

int buf_i=0;//数据长度

void SPI1_Read_FLASH(uint8_t *data,uint8_t Length);

void SPI1_Write_FLASH(uint8_t *data, uint8_t Length);

uint8_t dest_addr[16];

uint8_t src_addr[16];

uint8_t XT25_ReadSR();

void XT25_WriteEnable();

void XT25_RUID();

void XT25_Erase();

void XT25_WriteSR(uint8_t sr);

void XT25_EraseSector();

void Delay_1s(void);

/*

*uart初始化

*clk 12MHZ

*115200 8 1 N

*FIFO 深度8

*/

void UARTInit(){

 

  //配置引脚

       LPC_SYSCON->SYSAHBCLKCTRL |=(1<<16);//使能IO

  LPC_SYSCON->SYSAHBCLKCTRL |=(1<<6) ;//使能GPIO

       LPC_IOCON->PIO1_6 |= 0x01; //设置成RXD 引脚

       LPC_IOCON->PIO1_7 |= 0x01; //设置成TXD 引脚

       LPC_UART->LCR=3; //数据8 停止1 无校验

       //设置波特率115384  近似115200

       LPC_SYSCON->SYSAHBCLKCTRL |=(1<<12);//使能UART

       LPC_SYSCON->UARTCLKDIV=4;  //设置分频值4  获得UART时钟为12Mhz

       LPC_UART->LCR=0X83; //DLAB=1

       LPC_UART->DLL=4;

       LPC_UART->DLM=0;

       LPC_UART->LCR=0x03; //DLAB=0

       LPC_UART->FDR=0X85; //MIV=8 DIV=5

      

      

       LPC_UART->FCR  =0X81; //使能FIFO 深度设置为8

       LPC_UART->IER |=1<<0; //使能接受中断

       NVIC_EnableIRQ(UART_IRQn); //启动中断

      

}

/*

*发送字符串

*/

void UART_Send(uint8_t str[],int lenght){

       int i;

       for(i=0;i<lenght;i++){

              LPC_UART->THR= str[i];

                     while((LPC_UART->LSR&0X40)==0);//等待数据发送完成

       }

}

/*

*发送 一个字节

*/

void UART_Send_Bit(uint8_t data){

       LPC_UART->THR= data;

                     while((LPC_UART->LSR&0X40)==0);//等待数据发送完成

}

#include <LPC11xx.h>

/*

*函数功能：I2C 初始化

*/

void I2CInit(){

       LPC_SYSCON->PRESETCTRL |= (1<<1); //复位取消

       LPC_SYSCON->SYSAHBCLKCTRL |=(1<<5);//使能I2C

       LPC_SYSCON->SYSAHBCLKCTRL |=(1<<16);//使能IO配置块

      

  //选择快速模式

       LPC_IOCON->PIO0_4 &=~(0X3F); //选择快速模式

       LPC_IOCON->PIO0_4 |=0X01;//选择SCL

       LPC_IOCON->PIO0_5 &=~(0X3F); //选择快速模式

       LPC_IOCON->PIO0_5 |=0X01;//选择SDA

      

       //设置SCL频率为400kHZ

       LPC_I2C->SCLH=40;

       LPC_I2C->SCLL=80;

      

       //使能I2C 同时将其他控制位清0

       LPC_I2C->CONCLR=0XFF;     //清所有标志

       LPC_I2C->CONSET |=(1<<6);    //使能I2C接口

}

/*

*函数功能：发送开始信号

*/

void I2c_Start(){

       LPC_I2C->CONSET =(1<<5);// 发送开始信号

       while(!(LPC_I2C->CONSET&(1<<3))){//等待开始信号发送完成 SI置位

       }

       LPC_I2C->CONCLR =(1<<5|1<<3); //清零START 和SI

}

/*

*函数功能：发送停止信号

*/

void I2C_Stop(){

       LPC_I2C->CONCLR =(1<<3);

       LPC_I2C->CONSET =(1<<4);// 发送停止信号

       while((LPC_I2C->CONSET&(1<<4))){//等待停止信号发送完成 SI置位

       }

}

/*

*函数功能：发送一个字节

*/

void I2C_Send_Byte(uint8_t data){

       LPC_I2C->DAT=data;

       LPC_I2C->CONCLR =(1<<3); //开始发送数据 清SI

              while(!(LPC_I2C->CONSET&(1<<3))){//等待数据发送完成 SI置位

       }

}

/*

*函数功能：接受一个字节

*/

uint8_t I2C_Recieve_Byte(){

       LPC_I2C->CONCLR =(1<<3);//开始接受数据  清SI

       while(!(LPC_I2C->CONSET&(1<<3))){//等待接受数据完成 SI置位

       }

       return (uint8_t)LPC_I2C->DAT;

}

#include <LPC11xx.h>

#define WREN    0X06            //写入使能

#define WRDI      0X04

#define RDSR      0X05

#define WRSR     0X01

#define READ      0X03

#define WRITE    0X02

extern uint8_t src_addr[16]; //写

extern uint8_t dest_addr[16];//读

void UART_Send(uint8_t str[],int lenght);

void UART_Send_Bit(uint8_t data);

void XT25_WriteEnable();

uint8_t XT25_ReadSR();

uint8_t SPI1_comunication(uint8_t TxData);

extern long int ADDR;

void Delay_1s(void){

       int i=SystemCoreClock/500;//0.01s

              while(--i);

}

/*

*函数功能：SSP1IO初始化

*/

void SSP1_IOConfig(){

       LPC_SYSCON->SYSAHBCLKCTRL |=((1<<6)|(1<<16)); //IO 和GPIO

       LPC_IOCON->PIO2_2 &=~(0X07); //

       LPC_IOCON->PIO2_2 |=0X02;// 把PIO2_3选择为MISO

      

       LPC_IOCON->PIO2_3 &=~(0X07); //

       LPC_IOCON->PIO2_3 |=0X02;//把PIO2_2选择为MOSI

       LPC_IOCON->PIO2_1 &=~(0X07); //

       LPC_IOCON->PIO2_1 |=0X02;//把PIO2_1选择为LPC_SSP   CLK

      

       LPC_GPIO2->DIR |= (1<<0);

       LPC_GPIO2->DATA |= (1<<0); //拉高

}

void SSP1_LOW(){

       LPC_GPIO2->DATA &= ~(1<<0); //拉低

}

void SSP1_HIGH(){

       LPC_GPIO2->DATA |= (1<<0); //拉高

}

/**

*函数功能：SSP1初始化

*/

void SSPI1_Init(){

       uint8_t Dummy=Dummy; //解决编译产生的Warning:never used

       uint8_t i;

      

       LPC_SYSCON->PRESETCTRL |=1<<2; //禁止LPC_ssp1复位

       LPC_SYSCON->SYSAHBCLKCTRL |=(1<<18); //ssp1 时钟使能

       LPC_SYSCON->SSP1CLKDIV=0X02 ;  //分频值 2 48/2=24

      

       SSP1_IOConfig(); //初始化SSP1 IO口

      

       LPC_SSP1->CR0=0X0707;   //CPSR=7 DATAbit= 8 CPOL=0 CPHA=0 SCR=7  选择spi

      

       LPC_SSP1->CPSR=0X02;   //SCLK=48/(2*2*8)= 1.5M

      

       LPC_SSP1->CR1 &=~(1<<0);//LBM=0 :正常模式

       LPC_SSP1->CR1 &=~(1<<2);//MS=0 主机模式

       LPC_SSP1->CR1 |=1<<1; //SSE=1 开启ssp1

      

       //清空RXFIFO

       for(i=0 ; i<8 ;i++){

              Dummy=LPC_SSP1->DR;

       }

       for(i=0;i<16;i++){

              dest_addr[i]=0;

              src_addr[i]=0;

       }

       ADDR=0xff0000;

}

/**

* 函数功能：SSP1通信

* 接受和发送一个字符

*/

uint8_t SPI1_comunication(uint8_t TxData){

       while(((LPC_SSP1->SR)&(1<<4))==(1<<4));//忙时等待

       LPC_SSP1->DR=TxData; //发送数据到TxFIFO

       while(((LPC_SSP1->SR)&(1<<2))!=(1<<2));//等待数据接受完

       return (LPC_SSP1->DR); //接受返回的数据

}

/**

*函数功能：SSP1发送

*/

void SSP1_Send(uint8_t *data,uint8_t Length){

       uint8_t i;

       for(i=0;i<Length;i++){

              SPI1_comunication(data[i]);

       }

}

/**

* 函数功能：SSP1接受

*/

void SSP1_Receive(uint8_t *data,int Length){

       uint8_t Dummy=Dummy; //随机数 用于产生时钟

       uint8_t i;

       for(i=0 ; i<Length ;i++){

              data[i]=SPI1_comunication(0xff);

       }

}

/**

* 函数功能：写入使能

*/

void XT25_WriteEnable(){

       SSP1_LOW();

       SPI1_comunication(WREN);

       SSP1_HIGH();

}

/**

* 函数功能：读状态寄存器

*/

uint8_t XT25_ReadSR(){

       uint8_t sr;

       SSP1_LOW();

       SPI1_comunication(RDSR);//发送读取命令

       sr=SPI1_comunication(0xff);//得到寄存器数据

       SSP1_HIGH();

       return  sr;

}

/*

*函数功能：忙碌等待 写入等待

*/

void XT25_Write_Wait(){

       int stat_code=0;

       while(1){

              stat_code=XT25_ReadSR();

              if(((stat_code&1<<1)==0x02)&&(stat_code&1)==0){

                     break;

              }

       }

}

/*

*函数功能：忙碌等待 读出等待

*/

void XT25_Read_Wait(){

       int stat_code=0;

       while(1){

              stat_code=XT25_ReadSR();

              if((stat_code&1)==0){

                     break;

              }

       }

}

/*

*函数功能：写FLASH

*/

void SPI1_Write_FLASH(uint8_t *data, uint8_t Length){

       uint8_t i;

       uint8_t sr;

       XT25_WriteEnable();//写入使能

       Delay_1s();//延时1秒

       src_addr[0]=WRITE;//页面编程开始

       //地址1~3

       src_addr[1]=(uint8_t)((ADDR)>>16);

       src_addr[2]=(uint8_t)((ADDR)>>8);

       src_addr[3]=(uint8_t) ADDR;

       //写入的数据

       for(i=0;i<Length;i++){

              src_addr[i+4]=data[i];

       }

       XT25_Write_Wait(); //忙时等待

       SSP1_LOW();//使能

       SSP1_Send((uint8_t *)src_addr,4+Length);

       SSP1_HIGH();//拉高

}

/**

* 函数功能：读flash

*/

void SPI1_Read_FLASH(uint8_t *data,uint8_t Length){

       int i;

       int stat_code=0;

       src_addr[0]=READ;

       //读取的地址

       src_addr[1]=(uint8_t)((ADDR)>>16);

       src_addr[2]=(uint8_t)((ADDR)>>8);

       src_addr[3]=(uint8_t) ADDR;

       XT25_Read_Wait();

       SSP1_LOW();

  SSP1_Send((uint8_t *)src_addr,4);

       SSP1_Receive((uint8_t *)dest_addr,Length);//接受温度

       SSP1_HIGH();

       for(i=0;i<Length;i++){ //温度为3个整数 1个小数点 2个小数

                     data[i]=dest_addr[i];

       }

}

#include <LPC11xx.h>

void I2c_Start();

void I2C_Stop();

void I2C_Send_Byte();

uint8_t I2C_Recieve_Byte();

void Ds1307_WriteByte(uint8_t WriteAddr,uint8_t WriteData);

void DS1307_Write(uint8_t *data);

/*

* 函数功能：DS1307初始化

* 默认初始化为全0

*/

void DS1307Init(){

       Ds1307_WriteByte(0x00,0x01);  //秒

       Ds1307_WriteByte(0x01,0x01);  //分

       Ds1307_WriteByte(0x02,0x01);  //时

       Ds1307_WriteByte(0x03,0x01);  //星期

       Ds1307_WriteByte(0x04,0x01);  //日

  Ds1307_WriteByte(0x05,0x01);  //月

       Ds1307_WriteByte(0x06,0x01);  //年

       uint8_t time[7]={0x00,0x00,0x00,0x05,0x10,0x02,0x23};

       DS1307_Write(time);

}

/**

* 函数功能：DS1307读

*                                        全读

*/

void DS1307_Read(uint8_t *data){

      

       Ds1307_WriteByte(0x3f,0x01);//定位ds1307内部指针到0x3f(RAM尾部)处

       I2c_Start();//start

       I2C_Send_Byte(0xd1);//读

       LPC_I2C->CONSET =(1<<2);//AA=1

      

       data[0]=I2C_Recieve_Byte();

  data[1]=I2C_Recieve_Byte();

       data[2]=I2C_Recieve_Byte();

       data[3]=I2C_Recieve_Byte();

       data[4]=I2C_Recieve_Byte();

       data[5]=I2C_Recieve_Byte();

      

      

       LPC_I2C->CONCLR =(1<<2);//AA=0

       data[6]=I2C_Recieve_Byte();

       I2C_Stop();//STOP

      

}

/**

* 函数功能：DS1307写

*                                        全写

*/

void DS1307_Write(uint8_t *data){

      

      

       I2c_Start();//start

       I2C_Send_Byte(0xd0);//读

       LPC_I2C->CONSET =(1<<2);//AA=1

       I2C_Send_Byte(0x00);//从0x00开始写入

       I2C_Send_Byte(data[0]);

  I2C_Send_Byte(data[1]);

  I2C_Send_Byte(data[2]);

  I2C_Send_Byte(data[3]);

  I2C_Send_Byte(data[4]);

  I2C_Send_Byte(data[5]);    

      

       LPC_I2C->CONCLR =(1<<2);//AA=0

       I2C_Send_Byte(data[6]);

       I2C_Stop();//STOP

      

}

/*

*函数功能：DS1307写一个字节

*/

void Ds1307_WriteByte(uint8_t WriteAddr,uint8_t WriteData)

{

                     //I2C_Start();    

         I2c_Start();//start

             

    I2C_Send_Byte(0xd0);    // Device Addr + Write (operation)

    I2C_Send_Byte(WriteAddr);

      

              LPC_I2C->CONCLR =(1<<2);//AA=0    接受完下一个字节后返回非应答信号

    I2C_Send_Byte(WriteData);

  

    I2C_Stop();  

   

}

/*

* 函数功能：DS1307读一个字节

*/

uint8_t Ds1307_ReadByte()

{

  uint8_t RevData;

 

  I2c_Start();//start              

  I2C_Send_Byte(0xD1);     // Device Addr + Write (operation)  

 

       LPC_I2C->CONCLR =(1<<2);//AA=0

  RevData = I2C_Recieve_Byte();   

 

  I2C_Stop();  

 

  return RevData;

}

#include <LPC11xx.h>

void I2c_Start();

void I2C_Stop();

void I2C_Send_Byte();

void UART_Send(uint8_t str[],int lenght);

uint8_t I2C_Recieve_Byte();

extern uint8_t buf[100]; //存放温度

uint16_t Temputerature_Test(){

       uint16_t Temputerature_8,Temputerature_16;

      

       float Temputerature;

       I2c_Start();//start

       I2C_Send_Byte(0x91);//读

       LPC_I2C->CONSET =(1<<2);//AA=1

       Temputerature_8=I2C_Recieve_Byte();//高八位

       LPC_I2C->CONCLR =(1<<2);//AA=0

       Temputerature_16=(Temputerature_8<<8)+I2C_Recieve_Byte();//合成温度

       I2C_Stop();//STOP

       //温度转换

       Temputerature_16=Temputerature_16>>5;

       if(Temputerature_16&0x0400){

              Temputerature=-(~(Temputerature_16&0x03ff)+1)*0.125;

       }else{

              Temputerature=0.125*(float)(Temputerature_16);

       }

       return (uint16_t)(Temputerature*1000);

}

void Get_temputerature(){

       uint16_t temp;

      

       temp=Temputerature_Test();//获得温度

      

       //将int 转为char 同时'0'对应00000000 对数值无影响

       buf[0]=temp/100000+'0';//

       if((temp/100000)==0)

                     buf[0]=' ';//不显示0

       buf[1]=temp/10000%10+'0';

       buf[2]=temp/1000%10+'0';

       buf[3]='.';

       buf[4]=temp/100%10+'0';

       buf[5]=temp/10%10+'0';

       buf[6]=temp%10+'0';

}
————————————————
版权声明：本文为CSDN博主「Karry D」的原创文章，遵循CC 4.0 BY-SA版权协议，转载请附上原文出处链接及本声明。
原文链接：https://blog.csdn.net/m0_52474147/article/details/131295256