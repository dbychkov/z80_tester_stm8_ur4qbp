/*
 * Project: Z80 Tester by UR4QBP (STM8 firmware)
 * Purpose: Drives a Z80 test setup by generating selectable clock frequencies
 *          with the SI5351A and controlling a 2-digit indicator and buttons.
 * Note: 	This code is written in C and uses the C17 language standard version.
 * 
 * Original Author: UR4QBP
 * Created: 05/08/2025
 *
 * Change Log:
 * - 05/08/2025 UR4QBP:		Initial version.
 * - 04/25/2026 dbychkov:	Added source code to GitHub.
 * - 04/25/2026 dbychkov:	Changed VCO frequency to 800MHz for improved stability at high frequencies.
 * - 04/25/2026 dbychkov:	Refactored I2C wait macros in Si5351_i2c.h.
 * - 04/25/2026 dbychkov:	Added power-on I2C check&reset to main.c to handle SI5351A startup issues.
 * - 04/25/2026 dbychkov:	Added PLL lock wait function to main.c to ensure stable clock output before releasing Z80 reset.
 * - 04/25/2026 dbychkov:	Added comments and documentation for clarity.
 */

#include "stm8s.h"
#include "si5351_i2c.h"

/* ====================================================================
 * HARDWARE PIN MAPPING & CONTROL MACROS
 * ====================================================================
 * This section defines GPIO pins and macros for controlling a 2-digit
 * 7-segment display (common cathode) and Z80 reset control.
 * Note: For common cathode displays, active-LOW pins turn segments ON.
 */

#define DIG_1 GPIO_PIN_2 // GPIOC anode/cathode of digit 1
#define DIG_1_PORT GPIOD
#define DIG_1_ON GPIO_WriteHigh(DIG_1_PORT, DIG_1)
#define DIG_1_OFF GPIO_WriteLow(DIG_1_PORT, DIG_1)
#define DIG_2 GPIO_PIN_1 // GPIOC anode/cathode of digit 2
#define DIG_2_PORT GPIOD
#define DIG_2_ON GPIO_WriteHigh(DIG_2_PORT, DIG_2)
#define DIG_2_OFF GPIO_WriteLow(DIG_2_PORT, DIG_2)

#define SEG_A GPIO_PIN_7 // GPIOC segment "A"
#define SEG_A_PORT GPIOC
#define SEG_A_ON GPIO_WriteLow(SEG_A_PORT, SEG_A)
#define SEG_A_OFF GPIO_WriteHigh(SEG_A_PORT, SEG_A)
#define SEG_B GPIO_PIN_3 // GPIOD segment "B"
#define SEG_B_PORT GPIOD
#define SEG_B_ON GPIO_WriteLow(SEG_B_PORT, SEG_B)
#define SEG_B_OFF GPIO_WriteHigh(SEG_B_PORT, SEG_B)
#define SEG_C GPIO_PIN_6 // GPIOC segment "C"
#define SEG_C_PORT GPIOC
#define SEG_C_ON GPIO_WriteLow(SEG_C_PORT, SEG_C)
#define SEG_C_OFF GPIO_WriteHigh(SEG_C_PORT, SEG_C)
#define SEG_D GPIO_PIN_4 // GPIOD segment "D"
#define SEG_D_PORT GPIOD
#define SEG_D_ON GPIO_WriteLow(SEG_D_PORT, SEG_D)
#define SEG_D_OFF GPIO_WriteHigh(SEG_D_PORT, SEG_D)
#define SEG_E GPIO_PIN_5 // GPIOD segment "E"
#define SEG_E_PORT GPIOD
#define SEG_E_ON GPIO_WriteLow(SEG_E_PORT, SEG_E)
#define SEG_E_OFF GPIO_WriteHigh(SEG_E_PORT, SEG_E)
#define SEG_F GPIO_PIN_6 // GPIOD segment "F"
#define SEG_F_PORT GPIOD
#define SEG_F_ON GPIO_WriteLow(SEG_F_PORT, SEG_F)
#define SEG_F_OFF GPIO_WriteHigh(SEG_F_PORT, SEG_F)
#define SEG_G GPIO_PIN_5 // GPIOD segment "G"
#define SEG_G_PORT GPIOC
#define SEG_G_ON GPIO_WriteLow(SEG_G_PORT, SEG_G)
#define SEG_G_OFF GPIO_WriteHigh(SEG_G_PORT, SEG_G)
#define SEG_DP GPIO_PIN_4 // GPIOD segment "DP"
#define SEG_DP_PORT GPIOC

#define RST_Z80 GPIO_PIN_3 // Reset Z80
#define RST_Z80_PORT GPIOA

#define BTN_PORT GPIOA
#define BTN_UP_PIN GPIO_PIN_2
#define BTN_DN_PIN GPIO_PIN_1

// OA - 2281B, OK - 2281A
//#define OA 1
#define OK 1

#if OA
const unsigned char number[] = // This array defines digits for a
                               // seven-segment common-anode display
    {
        0b01000000, // 0
        0b01111001, // 1
        0b00100100, // 2
        0b00110000, // 3
        0b00011001, // 4
        0b00010010, // 5
        0b00000010, // 6
        0b01111000, // 7
        0b00000000, // 8
        0b00010000, // 9
};
#define SEG_DP_ON GPIO_WriteLow(SEG_DP_PORT, SEG_DP)
#define SEG_DP_OFF GPIO_WriteHigh(SEG_DP_PORT, SEG_DP)
#endif

#if OK
const unsigned char number[] = // This array defines digits for a
                               // seven-segment common-cathode display
    {
        0b10111111, // 0
        0b10000110, // 1
        0b11011011, // 2
        0b11001111, // 3
        0b11100110, // 4
        0b11101101, // 5
        0b11111101, // 6
        0b10000111, // 7
        0b11111111, // 8
        0b11101111, // 9
};
#define SEG_DP_OFF GPIO_WriteLow(SEG_DP_PORT, SEG_DP)
#define SEG_DP_ON GPIO_WriteHigh(SEG_DP_PORT, SEG_DP)
#endif

/* ====================================================================
 * GLOBAL VARIABLES & STATE
 * ====================================================================
 * freq:      Current frequency (MHz). Range: freq_min to freq_max.
 * freq_max:  Upper frequency limit (40 MHz default).
 * freq_min:  Lower frequency limit (1 MHz default).
 * t:         Dynamic display counter (cycles 0 to 2*t_delay).
 *            Implements multiplexing for 2-digit display refresh.
 * t_delay:   Multiplexing timing divisor (affects display refresh rate).
 * dp_flag:   Decimal point enable flag (set during Z80 reset hold).
 * status:    Last read status from SI5351A PLL status register.
 */

// Function prototypes
void SendSeg(const unsigned char byte); //«апись сегментов в индикатор
void FLASH_Write(unsigned int WriteAddr, unsigned char val);
void CPU_Set(void);
void Check_Freq(void);
void PowerOn_I2C_Check(void);
void SI5351_WaitPLLLock(void);

// Variables
unsigned char freq, freq_max = 40, freq_min = 1;
unsigned char t, t_delay = 1;
bool dp_flag = 0;
unsigned char status;

// Power-on I2C BUSY lockup fix.
// At cold power-on the STM8 and SI5351A start simultaneously. Bus glitches
// during SI5351A power-on cause the STM8 I2C BUSY flag to get stuck Ч only
// a hardware reset can clear it, not software re-init.
//
// Boot 1 Ч cold power-on (WWDGF=0):
//   !(RST->SR & 0x01) is TRUE. I2C peripheral is uninitialized so
//   SI5351_ReadRegister times out and returns 0. 0 != 0x4F ? WWDG fires.
//   SI5351A stays powered through the reset.
//
// Boot 2 Ч after WWDG reset (WWDGF=1):
//   !(RST->SR & 0x01) is FALSE ? && short-circuits, no I2C call made.
//   Normal boot proceeds; SI5351A already initialized ? I2C works.
//   RST->SR cleared so next cold power-on triggers Boot 1 again.
void PowerOn_I2C_Check(void) {
    if (!(RST->SR & 0x01) &&
        (SI5351_ReadRegister(16) != (0x4F | SI5351a_CLK_SRC_PLL_A))) {
        WWDG->CR = 0x80; // WDGA=1, T6=0: immediate reset
        while (1) {
        }
    }
    RST->SR = 0x00; // clear WWDGF so next power-on triggers the reset again
}

// Wait for PLL to lock before releasing Z80 reset.
// CLK0 output is unstable until LOL_A (bit 5) and SYS_INIT (bit 7) clear.
// Retries PLL reset once on timeout; gives up after 3 retries to avoid hang.
void SI5351_WaitPLLLock(void) {
    unsigned char lock_wait = 0;
    unsigned char retries = 0;
    do {
        delay_ms(100);
        status = SI5351_ReadRegister(0);
        if (++lock_wait > 20) {
            SI5351_SendRegister(SI5351a_PLL_RESET, 0x20);
            lock_wait = 0;
            if (++retries > 3)
                break; // give up after ~8s total
        }
    } while (status & (0x20 | 0x80));
}

// Main function
void main(void) {
    // Recover from the power-on I2C BUSY lockup case before touching the SI5351A.
    PowerOn_I2C_Check();

    // Initialize MCU peripherals, keep the Z80 halted, and show the decimal
    // point while the saved clock setting is restored and the PLL stabilizes.
    CPU_Set();
    GPIO_WriteHigh(RST_Z80_PORT, RST_Z80);
    dp_flag = 1;
    SI5351_I2C_Init();
    freq = FLASH_ReadByte(0x4000);
    delay_ms(100);
    SI5351_SetFrequencyA(freq * 1000000);
    delay_ms(400);

    // Wait for PLL to lock before releasing Z80 reset.
    SI5351_WaitPLLLock();
    GPIO_WriteLow(RST_Z80_PORT, RST_Z80);
    dp_flag = 0;

    // Poll the two buttons and fully reapply the output clock after each
    // change so the Z80 only runs from a locked, persisted frequency.
    while (1) {
        if (!GPIO_ReadInputPin(BTN_PORT, BTN_UP_PIN)) {
            // Hold the Z80 in reset while stepping the frequency upward.
            GPIO_WriteHigh(RST_Z80_PORT, RST_Z80);
            dp_flag = 1;
            freq++;
            Check_Freq();
            FLASH_Write(0x4000, freq);
            SI5351_SetFrequencyA(freq * 1000000);
		    // Wait for PLL to lock before releasing Z80 reset.
    		SI5351_WaitPLLLock();
            delay_ms(500);
			// Release the Z80 now that the new frequency is stable.
            GPIO_WriteLow(RST_Z80_PORT, RST_Z80);
            dp_flag = 0;
        }
        if (!GPIO_ReadInputPin(BTN_PORT, BTN_DN_PIN)) {
            // Apply the same safe update sequence when stepping downward.
            GPIO_WriteHigh(RST_Z80_PORT, RST_Z80);
            dp_flag = 1;
            freq--;
            Check_Freq();
            FLASH_Write(0x4000, freq);
            SI5351_SetFrequencyA(freq * 1000000);
		    // Wait for PLL to lock before releasing Z80 reset.
    		SI5351_WaitPLLLock();
            delay_ms(500);
			// Release the Z80 now that the new frequency is stable.
            GPIO_WriteLow(RST_Z80_PORT, RST_Z80);
            dp_flag = 0;
        }
    }
}
//**********************************************************
void Check_Freq(void) {
    if (freq > freq_max) {
        freq = freq_max;
    }
    if (freq < freq_min) {
        freq = freq_min;
    }
}

// Initialize all MCU's GPIOs and peripherals, set the CPU clock to 16 MHz,
// and start Timer 1 for display multiplexing.
void CPU_Set(void) {
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


// Timer 1 update interrupt handler for dynamic display multiplexing.
@far @interrupt void tim1UpdateInterrupt(void) {
    TIM1_ClearITPendingBit(TIM1_IT_UPDATE);

#if OA
    if (t == t_delay * 1) {
        SendSeg(0xFF);
        if (freq / 10 % 10) {
            DIG_1_ON;
            DIG_2_OFF;
        } else {
            DIG_1_OFF;
            DIG_2_OFF;
        }
        SendSeg(number[freq / 10 % 10]);
        SEG_DP_OFF;
    }
    if (t == t_delay * 2) {
        SendSeg(0xFF);
        DIG_1_OFF;
        DIG_2_ON;
        SendSeg(number[freq % 10]);
        if (dp_flag) {
            SEG_DP_ON;
        } else {
            SEG_DP_OFF;
        }
    }
#endif

#if OK
    if (t == t_delay * 1) {
        SendSeg(0x00);
        if (freq / 10 % 10) {
            DIG_1_OFF;
            DIG_2_ON;
        } else {
            DIG_1_ON;
            DIG_2_ON;
        }
        SendSeg(number[freq / 10 % 10]);
        SEG_DP_OFF;
    }
    if (t == t_delay * 2) {
        SendSeg(0x00);
        DIG_1_ON;
        DIG_2_OFF;
        SendSeg(number[freq % 10]);
        if (dp_flag) {
            SEG_DP_ON;
        } else {
            SEG_DP_OFF;
        }
    }
#endif

    // if (dp_flag) {SEG_DP_ON;} else {SEG_DP_OFF;};
    t++;
    if (t > t_delay * 2) {
        t = 0;
    } // Counter for dynamic display multiplexing
}

// Set segments of the 7-segment LED display
void SendSeg(const unsigned char byte)
{
    if (byte & 0b00000001) {
        SEG_A_OFF;
    } else {
        SEG_A_ON;
    }
    if ((byte & 0b00000010) >> 1) {
        SEG_B_OFF;
    } else {
        SEG_B_ON;
    }
    if ((byte & 0b00000100) >> 2) {
        SEG_C_OFF;
    } else {
        SEG_C_ON;
    }
    if ((byte & 0b00001000) >> 3) {
        SEG_D_OFF;
    } else {
        SEG_D_ON;
    }
    if ((byte & 0b00010000) >> 4) {
        SEG_E_OFF;
    } else {
        SEG_E_ON;
    }
    if ((byte & 0b00100000) >> 5) {
        SEG_F_OFF;
    } else {
        SEG_F_ON;
    }
    if ((byte & 0b01000000) >> 6) {
        SEG_G_OFF;
    } else {
        SEG_G_ON;
    }
}

// Write Z80 frequency setting to MCU's EEPROM for persistence across resets and power cycles.
void FLASH_Write(unsigned int WriteAddr, unsigned char val) {
    FLASH_Unlock(FLASH_MEMTYPE_DATA);
    FLASH_EraseByte(WriteAddr);
    FLASH_ProgramByte(WriteAddr, val);
    FLASH_Lock(FLASH_MEMTYPE_DATA);
}