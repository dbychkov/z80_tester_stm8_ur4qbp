#define SI5351_SDA_PIN				GPIO_PIN_5	// Si5351 5 pin, SDA
#define SI5351_SCL_PIN				GPIO_PIN_4	// Si5351 4 pin, SCL
#define SI5351_I2C_PORT  			GPIOB

#define SI5351_I2C_ADDR      	0xC0	//I2C address for writing to the Si5351A
#define SI5351a_CLK0_CONTROL	16	// Register definitions
#define SI5351a_CLK1_CONTROL	17
#define SI5351a_CLK2_CONTROL	18
#define SI5351a_SYNTH_PLL_A		26
#define SI5351a_SYNTH_PLL_B  	34
#define SI5351a_SYNTH_MS_0		42
#define SI5351a_SYNTH_MS_1		50
#define SI5351a_SYNTH_MS_2		58
#define SI5351a_PLL_RESET			177
#define SI5351a_PLL_LOADCAP		183

#define SI5351a_R_DIV_1				0x00	   // R-division ratio definitions
#define SI5351a_R_DIV_2				0x10
#define SI5351a_R_DIV_4				0x20
#define SI5351a_R_DIV_8				0x30
#define SI5351a_R_DIV_16			0x40
#define SI5351a_R_DIV_32			0x50
#define SI5351a_R_DIV_64			0x60
#define SI5351a_R_DIV_128			0x70

#define SI5351a_CLK_SRC_PLL_A	0x00
#define SI5351a_CLK_SRC_PLL_B	0x20
// Use VCO value for max 800MHz max internal PLL frequency. The 900Mhz may often be unstable.
#define SI5351_PLL_FREQ	      800000000uL
extern unsigned long XTAL_FREQ = 25000000;   //Crystal frequency

void SI5351_I2C_Init(void)
{
	GPIO_DeInit(GPIOB);
	GPIO_Init(SI5351_I2C_PORT, SI5351_SCL_PIN, GPIO_MODE_OUT_OD_HIZ_FAST);
	GPIO_Init(SI5351_I2C_PORT, SI5351_SDA_PIN, GPIO_MODE_OUT_OD_HIZ_FAST);
	I2C_Init(100000, //Частота I2C
				  SI5351_I2C_ADDR, 
				  I2C_DUTYCYCLE_2, 
				  I2C_ACK_CURR, 
				  I2C_ADDMODE_7BIT, 
				  (CLK_GetClockFreq() / 1000000));//Частота тактирования шины
	I2C_Cmd(ENABLE);
}

static void SI5351_SendRegister(unsigned char reg, unsigned char data)
{
   I2C_GenerateSTART(ENABLE);
   while(!I2C_CheckEvent(I2C_EVENT_MASTER_MODE_SELECT));
   
   I2C_Send7bitAddress(SI5351_I2C_ADDR, I2C_DIRECTION_TX);  
   while(!I2C_CheckEvent(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED));
	 
   I2C_SendData(reg);
   while(!I2C_CheckEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED));
	 
	 I2C_SendData(data);
   while(!I2C_CheckEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED));
	 
	 I2C_GenerateSTOP(ENABLE); 
}

// Set up specified PLL with mult, num and denom
// mult is 15..90
// num is 0..1,048,575 (0xFFFFF)
// denom is 0..1,048,575 (0xFFFFF)
//
static void SI5351_SetupPLL(unsigned char pll, unsigned char mult, unsigned  long num, unsigned long denom)
{
	const float d = (float) num / (float) denom;
	const unsigned long Pz = (unsigned long) (128 * (d));

	const unsigned long P1 = (unsigned long) (128 * (unsigned long) mult + Pz - 512);
	const unsigned long P2 = (unsigned long) (128 * num - denom * Pz);
	const unsigned long P3 = denom;
	
	// Write Operation - Burst (Auto Address Increment)
	I2C_GenerateSTART(ENABLE);
  while(!I2C_CheckEvent(I2C_EVENT_MASTER_MODE_SELECT));
   
  I2C_Send7bitAddress(SI5351_I2C_ADDR, I2C_DIRECTION_TX);  
  while(!I2C_CheckEvent(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED));
	
  I2C_SendData(pll);
  while(!I2C_CheckEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED));	
	 
  I2C_SendData((P3 & 0x0000FF00) >> 8);
  while(!I2C_CheckEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED));	 
	 
  I2C_SendData((P3 & 0x000000FF));
  while(!I2C_CheckEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED));	
	 
  I2C_SendData((P1 & 0x00030000) >> 16);
  while(!I2C_CheckEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED));	 
	 
  I2C_SendData((P1 & 0x0000FF00) >> 8);
  while(!I2C_CheckEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED));	

  I2C_SendData((P1 & 0x000000FF));
  while(!I2C_CheckEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED));	
	 
  I2C_SendData(((P3 & 0x000F0000) >> 12) | ((P2 & 0x000F0000) >> 16));
  while(!I2C_CheckEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED));	 
	 
  I2C_SendData((P2 & 0x0000FF00) >> 8);
  while(!I2C_CheckEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED));	

  I2C_SendData((P2 & 0x000000FF));
  while(!I2C_CheckEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED));	

	I2C_GenerateSTOP(ENABLE); 
}

// Set up MultiSynth with integer divider and R divider
// R divider is the bit value which is OR'ed onto the appropriate register, it is a #define in si5351a.h
//
static void SI5351_SetupMultisynth(unsigned char synth, unsigned long divider, unsigned char rDiv)
{
	const unsigned long P1 = 128 * divider - 512;	// 18-bit number is an encoded representation of the integer part of the Multi-SynthX divider
	const unsigned long P2 = 0;						// P2 = 0, P3 = 1 forces an integer value for the divider
	const unsigned long P3 = 1;

	// Write Operation - Burst (Auto Address Increment)
	I2C_GenerateSTART(ENABLE);
  while(!I2C_CheckEvent(I2C_EVENT_MASTER_MODE_SELECT));
   
  I2C_Send7bitAddress(SI5351_I2C_ADDR, I2C_DIRECTION_TX);  
  while(!I2C_CheckEvent(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED));
	
	I2C_SendData(synth);
  while(!I2C_CheckEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED));
	
	I2C_SendData((P3 & 0x0000FF00) >> 8);// MSx_P3[15:8]
  while(!I2C_CheckEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED));
	
	I2C_SendData((P3 & 0x000000FF));// MSx_P3[7:0]
  while(!I2C_CheckEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED));
	
	I2C_SendData(((P1 & 0x00030000) >> 16) | rDiv | (divider == 4 ? 0x0c : 0x00));// Rx_DIV[2:0], MSx_DIVBY4[1:0], MSx_P1[17:16]
  while(!I2C_CheckEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED));

	I2C_SendData((P1 & 0x0000FF00) >> 8);// MSx_P1[15:8]
  while(!I2C_CheckEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED));
	
	I2C_SendData((P1 & 0x000000FF));// MSx_P1[7:0]
  while(!I2C_CheckEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED));
				
	I2C_SendData(((P3 & 0x000F0000) >> 12) | ((P2 & 0x000F0000) >> 16));// MSx_P3[19:16], MSx_P2[19:16]
  while(!I2C_CheckEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED));
	
	I2C_SendData((P2 & 0x0000FF00) >> 8);// MSx_P2[15:8]
  while(!I2C_CheckEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED));
	
	I2C_SendData((P2 & 0x000000FF));// MSx_P2[7:0]
  while(!I2C_CheckEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED));
	
	I2C_GenerateSTOP(ENABLE);
}

struct FREQ {
  unsigned char plldiv;	// должно быть чётное число
  unsigned char outdiv;	// Rx Output Divider code (SI5351a_R_DIV_1..SI5351a_R_DIV_128)
  unsigned int divider;	// общий делитель
  unsigned long fmin;
  unsigned long fmax;
};

static const unsigned char pllbase [2] =
{
	SI5351a_SYNTH_PLL_A,
	SI5351a_SYNTH_PLL_B,
};

static const unsigned char multisynchbase [2] =
{
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

	divider = SI5351_PLL_FREQ / frequency;// Calculate the division ratio. 900,000,000 is the maximum internal, PLL frequency: 900MHz
	if (divider % 2)
		divider -= 1;		// Ensure an even integer division ratio

	pllFreq = divider * frequency;	// Calculate the pllFrequency: the divider * desired output frequency

	denom = 0x000FFFFF;				// For simplicity we set the denominator to the maximum 0x000FFFFF
	mult = pllFreq / xtalFreq;		// Determine the multiplier to get to the required pllFrequency
	l = pllFreq % xtalFreq;			// It has three parts:
	f = l;							// mult is an integer that must be in the range 15..90
	f *= (unsigned long) denom;					// num and denom are the fractional parts, the numerator and denominator
	f /= xtalFreq;					// each is 20 bits (range 0..0x000FFFFF)
	num = f;						// the actual multiplier is  mult + num / denom

									// Set up PLL B with the calculated multiplication ratio
	SI5351_SetupPLL(pllbase [clkout], mult, num, denom);

	// Set up MultiSynth divider 1, with the calculated divider.
	// The final R division stage can divide by a power of two, from 1..128.
	// reprented by constants SI5351a_R_DIV1 to SI5351a_R_DIV128 (see si5351a.h header file)
	// If you want to output frequencies below 1MHz, you have to use the
	// final R division stage
	SI5351_SetupMultisynth(multisynchbase [clkout], divider, SI5351a_R_DIV_1);
	return mult;
}

//
// Set CLK0 output ON and to the specified frequency
// Frequency is in the range 1MHz to 150MHz
// Example: si5351aSetFrequency(10000000);
// will set output CLK0 to 10MHz
//
// This example sets up PLL A
// and MultiSynth 0
// and produces the output on CLK0
//
static void SI5351_SetFrequencyA(unsigned long frequency)
{
	static unsigned char skipreset;
	static unsigned char oldmult;
	const unsigned char mult = SI5351_SetFrequencyX(0, frequency);

	if (skipreset == 0 || mult != oldmult)
	{
		SI5351_SendRegister(SI5351a_PLL_RESET, 0x20);	// PLL A reset
		// Finally switch on the CLK1 output (0x4F)
		// and set the MultiSynth0 input to be PLL B
		SI5351_SendRegister(SI5351a_CLK0_CONTROL, 0x4F | SI5351a_CLK_SRC_PLL_A);

		skipreset = 1;
		oldmult = mult;
	}
}



static void SI5351_Init(void)
{
	SI5351_I2C_Init();   
	SI5351_SendRegister(SI5351a_PLL_LOADCAP, 0xC0 | 0x12);
	SI5351_SendRegister(SI5351a_PLL_RESET, 0x20);	// PLL A reset
	// Finally switch on the CLK0 output (0x4F)
	// and set the MultiSynth0 input to be PLL A
	SI5351_SendRegister(SI5351a_CLK0_CONTROL, 0x4F | SI5351a_CLK_SRC_PLL_A);

	SI5351_SendRegister(SI5351a_PLL_RESET, 0x80);	// PLL B reset
	// Finally switch on the CLK1 output (0x4F)
	// and set the MultiSynth0 input to be PLL B
	SI5351_SendRegister(SI5351a_CLK1_CONTROL, 0x4F | SI5351a_CLK_SRC_PLL_B);
}