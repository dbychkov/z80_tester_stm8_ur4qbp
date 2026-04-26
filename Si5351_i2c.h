#ifndef SI5351_I2C_H
#define SI5351_I2C_H

#include "stm8s.h"

#define SI5351_SDA_PIN              GPIO_PIN_5 // Si5351 pin 5, SDA
#define SI5351_SCL_PIN              GPIO_PIN_4 // Si5351 pin 4, SCL
#define SI5351_I2C_PORT             GPIOB

#define SI5351_I2C_ADDR             0xC0 // I2C address for writing to the Si5351A
#define SI5351a_CLK0_CONTROL        16   // Register definitions
#define SI5351a_CLK1_CONTROL        17
#define SI5351a_CLK2_CONTROL        18
#define SI5351a_SYNTH_PLL_A         26
#define SI5351a_SYNTH_PLL_B         34
#define SI5351a_SYNTH_MS_0          42
#define SI5351a_SYNTH_MS_1          50
#define SI5351a_SYNTH_MS_2          58
#define SI5351a_PLL_RESET           177
#define SI5351a_PLL_LOADCAP         183

#define SI5351a_R_DIV_1             0x00 // R-division ratio definitions
#define SI5351a_R_DIV_2             0x10
#define SI5351a_R_DIV_4             0x20
#define SI5351a_R_DIV_8             0x30
#define SI5351a_R_DIV_16            0x40
#define SI5351a_R_DIV_32            0x50
#define SI5351a_R_DIV_64            0x60
#define SI5351a_R_DIV_128           0x70

#define SI5351a_CLK_SRC_PLL_A       0x00
#define SI5351a_CLK_SRC_PLL_B       0x20

// Use VCO value for max 800MHz max internal PLL frequency. The 900MHz may often be unstable.
#define SI5351_PLL_FREQ             800000000uL

extern unsigned long XTAL_FREQ; // Crystal frequency

void SI5351_I2C_Init(void);
void SI5351_SendRegister(unsigned char reg, unsigned char data);
void SI5351_SetFrequencyA(unsigned long frequency);
unsigned char SI5351_ReadRegister(unsigned char reg_address);

#endif
