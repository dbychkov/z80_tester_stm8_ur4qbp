#include "stm8s.h"
#include "si5351_i2c.h"

#define DIG_1 GPIO_PIN_2 //GPIOC анод/катод 1-го знака
#define DIG_1_PORT GPIOD
#define DIG_1_ON GPIO_WriteHigh(DIG_1_PORT, DIG_1)
#define DIG_1_OFF GPIO_WriteLow(DIG_1_PORT, DIG_1)
#define DIG_2 GPIO_PIN_1 //GPIOC анод/катод 2-го знака
#define DIG_2_PORT GPIOD
#define DIG_2_ON GPIO_WriteHigh(DIG_2_PORT, DIG_2)
#define DIG_2_OFF GPIO_WriteLow(DIG_2_PORT, DIG_2)

#define SEG_A GPIO_PIN_7  //GPIOC сегмент "А"
#define SEG_A_PORT GPIOC
#define SEG_A_ON GPIO_WriteLow(SEG_A_PORT, SEG_A)
#define SEG_A_OFF GPIO_WriteHigh(SEG_A_PORT, SEG_A)
#define SEG_B GPIO_PIN_3  //GPIOD сегмент "B"
#define SEG_B_PORT GPIOD
#define SEG_B_ON GPIO_WriteLow(SEG_B_PORT, SEG_B)
#define SEG_B_OFF GPIO_WriteHigh(SEG_B_PORT, SEG_B)
#define SEG_C GPIO_PIN_6  //GPIOC сегмент "C"
#define SEG_C_PORT GPIOC
#define SEG_C_ON GPIO_WriteLow(SEG_C_PORT, SEG_C)
#define SEG_C_OFF GPIO_WriteHigh(SEG_C_PORT, SEG_C)
#define SEG_D GPIO_PIN_4  //GPIOD сегмент "D"
#define SEG_D_PORT GPIOD
#define SEG_D_ON GPIO_WriteLow(SEG_D_PORT, SEG_D)
#define SEG_D_OFF GPIO_WriteHigh(SEG_D_PORT, SEG_D)
#define SEG_E GPIO_PIN_5  //GPIOD сегмент "E"
#define SEG_E_PORT GPIOD
#define SEG_E_ON GPIO_WriteLow(SEG_E_PORT, SEG_E)
#define SEG_E_OFF GPIO_WriteHigh(SEG_E_PORT, SEG_E)
#define SEG_F GPIO_PIN_6  //GPIOD сегмент "F"
#define SEG_F_PORT GPIOD
#define SEG_F_ON GPIO_WriteLow(SEG_F_PORT, SEG_F)
#define SEG_F_OFF GPIO_WriteHigh(SEG_F_PORT, SEG_F)
#define SEG_G GPIO_PIN_5  //GPIOD сегмент "G"
#define SEG_G_PORT GPIOC
#define SEG_G_ON GPIO_WriteLow(SEG_G_PORT, SEG_G)
#define SEG_G_OFF GPIO_WriteHigh(SEG_G_PORT, SEG_G)
#define SEG_DP GPIO_PIN_4  //GPIOD сегмент "DP"
#define SEG_DP_PORT GPIOC

#define RST_Z80 GPIO_PIN_3  //Reset Z80
#define RST_Z80_PORT GPIOA

#define BTN_PORT 											GPIOA
#define BTN_UP_PIN			 							GPIO_PIN_2
#define BTN_DN_PIN		 								GPIO_PIN_1

//OA - 2281B, OK - 2281A
//#define OA 1
#define OK 1

#if OA
const unsigned char number[] = //В этом массиве задаются цифры на семисегментном индикаторе c общим анодом
{ 0b01000000, //0
  0b01111001, //1
  0b00100100, //2
  0b00110000, //3
  0b00011001, //4
  0b00010010, //5
  0b00000010, //6
  0b01111000, //7
  0b00000000, //8
  0b00010000, //9
};
#define SEG_DP_ON GPIO_WriteLow(SEG_DP_PORT, SEG_DP)
#define SEG_DP_OFF GPIO_WriteHigh(SEG_DP_PORT, SEG_DP)
#endif

#if OK
const unsigned char number[] = //В этом массиве задаются цифры на семисегментном индикаторе c общим катодом
{ 0b10111111, //0
  0b10000110, //1
  0b11011011, //2
  0b11001111, //3
  0b11100110, //4
  0b11101101, //5
  0b11111101, //6
  0b10000111, //7
  0b11111111, //8
  0b11101111, //9
};
#define SEG_DP_OFF GPIO_WriteLow(SEG_DP_PORT, SEG_DP)
#define SEG_DP_ON GPIO_WriteHigh(SEG_DP_PORT, SEG_DP)
#endif

unsigned char freq, freq_max = 40, freq_min = 1;
unsigned char t, t_delay = 1;
bool dp_flag = 0;

void SendSeg(const unsigned char byte); //Запись сегментов в индикатор
void FLASH_Write(unsigned int WriteAddr, unsigned char val);
void CPU_Set(void);
void Check_Freq(void);

void main(void)
{
CPU_Set();
GPIO_WriteHigh(RST_Z80_PORT, RST_Z80); dp_flag = 1;
SI5351_I2C_Init(); 
freq = FLASH_ReadByte(0x4000); delay_ms(100);
SI5351_SetFrequencyA(freq*1000000);
delay_ms(400);
GPIO_WriteLow(RST_Z80_PORT, RST_Z80); dp_flag = 0;
//*********************************************************	
while (1)
	{
		if ( !GPIO_ReadInputPin(BTN_PORT, BTN_UP_PIN) ) 
		{
		//GPIO_WriteHigh(RST_Z80_PORT, RST_Z80); dp_flag = 1;
		freq++; Check_Freq(); 
		FLASH_Write(0x4000, freq); 
		SI5351_SetFrequencyA(freq*1000000); 
		delay_ms(500);
		//GPIO_WriteLow(RST_Z80_PORT, RST_Z80); dp_flag = 0;
		};
		if ( !GPIO_ReadInputPin(BTN_PORT, BTN_DN_PIN) ) 
		{
		//GPIO_WriteHigh(RST_Z80_PORT, RST_Z80); dp_flag = 1;
		freq--; Check_Freq(); 
		FLASH_Write(0x4000, freq); 
		SI5351_SetFrequencyA(freq*1000000); 
		delay_ms(500);
		//GPIO_WriteLow(RST_Z80_PORT, RST_Z80); dp_flag = 0;
		};
	}
}
//**********************************************************
void Check_Freq(void)
{
	if ( freq > freq_max ) { freq = freq_max;};
	if ( freq < freq_min ) { freq = freq_min;};
}

void CPU_Set(void)
{
	CLK_HSIPrescalerConfig(CLK_PRESCALER_HSIDIV1 | CLK_PRESCALER_CPUDIV1); 
	FLASH_DeInit();
	GPIO_Init(DIG_1_PORT, DIG_1, GPIO_MODE_OUT_PP_HIGH_FAST);
	GPIO_Init(DIG_2_PORT, DIG_2, GPIO_MODE_OUT_PP_HIGH_FAST);
	GPIO_Init(SEG_A_PORT, SEG_A, GPIO_MODE_OUT_PP_HIGH_FAST);
	GPIO_Init(SEG_B_PORT, SEG_B, GPIO_MODE_OUT_PP_HIGH_FAST);
	GPIO_Init(SEG_C_PORT, SEG_C, GPIO_MODE_OUT_PP_HIGH_FAST);
	GPIO_Init(SEG_D_PORT, SEG_D, GPIO_MODE_OUT_PP_HIGH_FAST);
	GPIO_Init(SEG_E_PORT, SEG_E, GPIO_MODE_OUT_PP_HIGH_FAST);
	GPIO_Init(SEG_F_PORT, SEG_F, GPIO_MODE_OUT_PP_HIGH_FAST);
	GPIO_Init(SEG_G_PORT, SEG_G, GPIO_MODE_OUT_PP_HIGH_FAST);
	GPIO_Init(SEG_DP_PORT, SEG_DP, GPIO_MODE_OUT_PP_HIGH_FAST);
	GPIO_Init(RST_Z80_PORT, RST_Z80, GPIO_MODE_OUT_PP_HIGH_FAST);
	
	GPIO_Init(BTN_PORT, BTN_UP_PIN | BTN_DN_PIN, GPIO_MODE_IN_PU_NO_IT);
		
	TIM1_TimeBaseInit(16000, TIM1_COUNTERMODE_UP, 1, 0);
	TIM1_ITConfig(TIM1_IT_UPDATE, ENABLE);
	TIM1_Cmd(ENABLE);
	enableInterrupts();	
}

@far @interrupt void tim1UpdateInterrupt(void) {
	TIM1_ClearITPendingBit(TIM1_IT_UPDATE);
	
#if OA
if (t == t_delay*1) {SendSeg(0xFF); if (freq/10%10) {DIG_1_ON; DIG_2_OFF;} else {DIG_1_OFF; DIG_2_OFF;}; SendSeg(number[freq/10%10]); SEG_DP_OFF;};
if (t == t_delay*2) {SendSeg(0xFF); DIG_1_OFF; DIG_2_ON; SendSeg(number[freq%10]); if (dp_flag) {SEG_DP_ON;} else {SEG_DP_OFF;};};
#endif

#if OK
if (t == t_delay*1) {SendSeg(0x00); if (freq/10%10) {DIG_1_OFF; DIG_2_ON;} else {DIG_1_ON; DIG_2_ON;}; SendSeg(number[freq/10%10]); SEG_DP_OFF;};
if (t == t_delay*2) {SendSeg(0x00); DIG_1_ON; DIG_2_OFF; SendSeg(number[freq%10]); if (dp_flag) {SEG_DP_ON;} else {SEG_DP_OFF;};};
#endif

//if (dp_flag) {SEG_DP_ON;} else {SEG_DP_OFF;};
t++; if (t > t_delay*2) {t = 0;};//Программный счетчик динамической индикации
}

void SendSeg(const unsigned char byte) //Запись сегментов в индикатор
{
if  (byte & 0b00000001)       {SEG_A_OFF;} else {SEG_A_ON;};
if ((byte & 0b00000010) >> 1) {SEG_B_OFF;} else {SEG_B_ON;};
if ((byte & 0b00000100) >> 2) {SEG_C_OFF;} else {SEG_C_ON;};
if ((byte & 0b00001000) >> 3) {SEG_D_OFF;} else {SEG_D_ON;};
if ((byte & 0b00010000) >> 4) {SEG_E_OFF;} else {SEG_E_ON;};
if ((byte & 0b00100000) >> 5) {SEG_F_OFF;} else {SEG_F_ON;};
if ((byte & 0b01000000) >> 6) {SEG_G_OFF;} else {SEG_G_ON;};
}

void FLASH_Write(unsigned int WriteAddr, unsigned char val)
{	
FLASH_Unlock(FLASH_MEMTYPE_DATA);
FLASH_EraseByte(WriteAddr);
FLASH_ProgramByte(WriteAddr, val);
FLASH_Lock(FLASH_MEMTYPE_DATA);	
}