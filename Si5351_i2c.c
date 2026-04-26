#include "Si5351_i2c.h"

unsigned long XTAL_FREQ = 25000000;

#define I2C_TIMEOUT_VAL 10000u

static bool I2C_WaitEvent(I2C_Event_TypeDef event)
{
    unsigned int t = I2C_TIMEOUT_VAL;
    while (!I2C_CheckEvent(event)) {
        if (!--t) {
            I2C_GenerateSTOP(ENABLE);
            return FALSE;
        }
    }
    return TRUE;
}

#define I2C_WAIT(event)   if (!I2C_WaitEvent(event)) return;
#define I2C_WAIT_R(event) if (!I2C_WaitEvent(event)) return 0;

void SI5351_I2C_Init(void)
{
    GPIO_DeInit(GPIOB);
    GPIO_Init(SI5351_I2C_PORT, SI5351_SCL_PIN, GPIO_MODE_OUT_OD_HIZ_FAST);
    GPIO_Init(SI5351_I2C_PORT, SI5351_SDA_PIN, GPIO_MODE_OUT_OD_HIZ_FAST);
    I2C_Init(100000, // I2C frequency
             SI5351_I2C_ADDR,
             I2C_DUTYCYCLE_2,
             I2C_ACK_CURR,
             I2C_ADDMODE_7BIT,
             (CLK_GetClockFreq() / 1000000)); // Bus clock frequency
    I2C_Cmd(ENABLE);
}

void SI5351_SendRegister(unsigned char reg, unsigned char data)
{
    I2C_GenerateSTART(ENABLE);
    while (!I2C_CheckEvent(I2C_EVENT_MASTER_MODE_SELECT)) {
    }

    I2C_Send7bitAddress(SI5351_I2C_ADDR, I2C_DIRECTION_TX);
    while (!I2C_CheckEvent(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED)) {
    }

    I2C_SendData(reg);
    while (!I2C_CheckEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED)) {
    }

    I2C_SendData(data);
    while (!I2C_CheckEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED)) {
    }

    I2C_GenerateSTOP(ENABLE);
}

// Set up specified PLL with mult, num and denom
// mult is 15..90
// num is 0..1,048,575 (0xFFFFF)
// denom is 0..1,048,575 (0xFFFFF)
static void SI5351_SetupPLL(unsigned char pll, unsigned char mult, unsigned long num, unsigned long denom)
{
    const float d = (float)num / (float)denom;
    const unsigned long pz = (unsigned long)(128 * d);

    const unsigned long p1 = (unsigned long)(128 * (unsigned long)mult + pz - 512);
    const unsigned long p2 = (unsigned long)(128 * num - denom * pz);
    const unsigned long p3 = denom;

    // Write Operation - Burst (Auto Address Increment)
    I2C_GenerateSTART(ENABLE);
    while (!I2C_CheckEvent(I2C_EVENT_MASTER_MODE_SELECT)) {
    }

    I2C_Send7bitAddress(SI5351_I2C_ADDR, I2C_DIRECTION_TX);
    while (!I2C_CheckEvent(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED)) {
    }

    I2C_SendData(pll);
    while (!I2C_CheckEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED)) {
    }

    I2C_SendData((p3 & 0x0000FF00) >> 8);
    while (!I2C_CheckEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED)) {
    }

    I2C_SendData(p3 & 0x000000FF);
    while (!I2C_CheckEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED)) {
    }

    I2C_SendData((p1 & 0x00030000) >> 16);
    while (!I2C_CheckEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED)) {
    }

    I2C_SendData((p1 & 0x0000FF00) >> 8);
    while (!I2C_CheckEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED)) {
    }

    I2C_SendData(p1 & 0x000000FF);
    while (!I2C_CheckEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED)) {
    }

    I2C_SendData(((p3 & 0x000F0000) >> 12) | ((p2 & 0x000F0000) >> 16));
    while (!I2C_CheckEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED)) {
    }

    I2C_SendData((p2 & 0x0000FF00) >> 8);
    while (!I2C_CheckEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED)) {
    }

    I2C_SendData(p2 & 0x000000FF);
    while (!I2C_CheckEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED)) {
    }

    I2C_GenerateSTOP(ENABLE);
}

// Set up MultiSynth with integer divider and R divider
// R divider is the bit value which is OR'ed onto the appropriate register.
static void SI5351_SetupMultisynth(unsigned char synth, unsigned long divider, unsigned char rDiv)
{
    const unsigned long p1 = 128 * divider - 512;
    const unsigned long p2 = 0;
    const unsigned long p3 = 1;

    // Write Operation - Burst (Auto Address Increment)
    I2C_GenerateSTART(ENABLE);
    while (!I2C_CheckEvent(I2C_EVENT_MASTER_MODE_SELECT)) {
    }

    I2C_Send7bitAddress(SI5351_I2C_ADDR, I2C_DIRECTION_TX);
    while (!I2C_CheckEvent(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED)) {
    }

    I2C_SendData(synth);
    while (!I2C_CheckEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED)) {
    }

    I2C_SendData((p3 & 0x0000FF00) >> 8); // MSx_P3[15:8]
    while (!I2C_CheckEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED)) {
    }

    I2C_SendData(p3 & 0x000000FF); // MSx_P3[7:0]
    while (!I2C_CheckEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED)) {
    }

    I2C_SendData(((p1 & 0x00030000) >> 16) | rDiv | (divider == 4 ? 0x0c : 0x00)); // Rx_DIV[2:0], MSx_DIVBY4[1:0], MSx_P1[17:16]
    while (!I2C_CheckEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED)) {
    }

    I2C_SendData((p1 & 0x0000FF00) >> 8); // MSx_P1[15:8]
    while (!I2C_CheckEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED)) {
    }

    I2C_SendData(p1 & 0x000000FF); // MSx_P1[7:0]
    while (!I2C_CheckEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED)) {
    }

    I2C_SendData(((p3 & 0x000F0000) >> 12) | ((p2 & 0x000F0000) >> 16)); // MSx_P3[19:16], MSx_P2[19:16]
    while (!I2C_CheckEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED)) {
    }

    I2C_SendData((p2 & 0x0000FF00) >> 8); // MSx_P2[15:8]
    while (!I2C_CheckEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED)) {
    }

    I2C_SendData(p2 & 0x000000FF); // MSx_P2[7:0]
    while (!I2C_CheckEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED)) {
    }

    I2C_GenerateSTOP(ENABLE);
}

static const unsigned char pllbase[2] = {
    SI5351a_SYNTH_PLL_A,
    SI5351a_SYNTH_PLL_B,
};

static const unsigned char multisynchbase[2] = {
    SI5351a_SYNTH_MS_0,
    SI5351a_SYNTH_MS_1,
};

static unsigned char SI5351_SetFrequencyX(unsigned char clkout, unsigned long frequency)
{
    unsigned long pllFreq;
    const unsigned long xtalFreq = XTAL_FREQ;
    unsigned long l;
    double f;
    unsigned char mult;
    unsigned long num;
    unsigned long denom;
    unsigned long divider;

    divider = SI5351_PLL_FREQ / frequency; // Calculate division ratio.
    if (divider % 2) {
        divider -= 1;
    }

    pllFreq = divider * frequency;

    denom = 0x000FFFFF;
    mult = pllFreq / xtalFreq;
    l = pllFreq % xtalFreq;
    f = l;
    f *= (unsigned long)denom;
    f /= xtalFreq;
    num = f;

    SI5351_SetupPLL(pllbase[clkout], mult, num, denom);
    SI5351_SetupMultisynth(multisynchbase[clkout], divider, SI5351a_R_DIV_1);
    return mult;
}

void SI5351_SetFrequencyA(unsigned long frequency)
{
    static unsigned char skipreset;
    static unsigned char oldmult;
    const unsigned char mult = SI5351_SetFrequencyX(0, frequency);

    if (skipreset == 0 || mult != oldmult) {
        SI5351_SendRegister(SI5351a_PLL_RESET, 0x20); // PLL A reset
        SI5351_SendRegister(SI5351a_CLK0_CONTROL, 0x4F | SI5351a_CLK_SRC_PLL_A);

        skipreset = 1;
        oldmult = mult;
    }
}

unsigned char SI5351_ReadRegister(unsigned char reg_address)
{
    unsigned char data;

    // Start I2C communication
    I2C_GenerateSTART(ENABLE);
    I2C_WAIT_R(I2C_EVENT_MASTER_MODE_SELECT)

    // Send SI5351A address with WRITE bit
    I2C_Send7bitAddress(SI5351_I2C_ADDR, I2C_DIRECTION_TX);
    I2C_WAIT_R(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED)

    // Send register address
    I2C_SendData(reg_address);
    I2C_WAIT_R(I2C_EVENT_MASTER_BYTE_TRANSMITTED)

    // Repeated START
    I2C_GenerateSTART(ENABLE);
    I2C_WAIT_R(I2C_EVENT_MASTER_MODE_SELECT)

    // Send SI5351A address with READ bit
    I2C_Send7bitAddress(SI5351_I2C_ADDR, I2C_DIRECTION_RX);
    I2C_WAIT_R(I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED)

    // For single byte read: disable ACK and generate STOP before receiving the byte.
    I2C_AcknowledgeConfig(I2C_ACK_NONE);
    I2C_GenerateSTOP(ENABLE);

    // Wait for data
    I2C_WAIT_R(I2C_EVENT_MASTER_BYTE_RECEIVED)

    // Read data
    data = I2C_ReceiveData();

    // Re-enable ACK for future transactions
    I2C_AcknowledgeConfig(I2C_ACK_CURR);

    return data;
}
