//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#include "ac97dev_UCB1400.h"
#include <string.h>
#include <stdlib.h>
#include "util.h"




#define NUM_REAL_GPIOS			9
#define VGPIO_NUM_OVLF			15		//0x8000
#define VGPIO_NUM_CLIP			14		//0x4000
#define VGPIO_NUM_TSMX			13		//0x2000
#define VGPIO_NUM_TSPX			12		//0x1000
#define VPGIO_NUM_ADCR			11		//0x0800

enum UCB1400REG {
	RESET = 0x00,
	MASTERVOL = 0x02,
	MICVOL = 0x0e,
	RECSEL = 0x1a,
	RECGAIN = 0x1c,
	GENPURPOSE = 0x20,
	PDOWN = 0x26,
	EXTDAUDIOID = 0x28,
	EXTDAUDIOCTL = 0x2a,
	DACRATE = 0x2c,
	ADCRATE = 0x32,
	IODATA = 0x5a,
	IODIR = 0x5c,
	POSINTENA = 0x5e,
	NEGINTENA = 0x60,
	INTCLRSTA = 0x62,
	TSCCTL = 0x64,
	ADCCTL = 0x66,
	ADCDATA = 0x68,
	FTRCSR1 = 0x6a,
	FTRCSR2 = 0x6c,
	TSTCTL = 0x6e,
	EXTINT = 0x70,
	VID1 = 0x7c,
	VID2 = 0x7e,
};

struct UCB1400 {
	
	struct SocGpio *gpio;
	struct SocAC97 *ac97;
	int8_t irqPin;

	uint16_t masterVol, micVol, recSel, recGain, loopbackCtlReg, pdown, extStaCtl, dacRate, adcRate, tscCtl, adcCtl, adcVal, ftrCsr1, ftrCsr2, testCtl, extInt;
	
	//gpios
	uint16_t gpioInVals, gpioOutLatches, gpioIsOut, prevGpioVal, posIntEna, negIntEna, intSta;
	
	//pen
	int16_t penX, penY;
	
	//adcs
	uint16_t adcVals[4];
};

static void ucb1400prvUpdateIrq(struct UCB1400 *ucb)
{
	bool irqReq = !!ucb->intSta;
	
	if (ucb->irqPin >= 0)
		socGpioSetState(ucb->gpio, ucb->irqPin, irqReq);
	
	if (irqReq && (ucb->ftrCsr1 & 0x0004))		//GIEN and irq = wake up
		ucb->pdown = 0x000b;
}

static uint16_t ucb1400prvReadGpios(struct UCB1400 *ucb)
{
	return (ucb->gpioOutLatches & ucb->gpioIsOut) | (ucb->gpioInVals &~ ucb->gpioIsOut);
}

static void ucb1400prvRecalcGpios(struct UCB1400 *ucb)
{
	uint16_t newGpioVals = ucb1400prvReadGpios(ucb);
	uint16_t gpioDiffs = newGpioVals ^ ucb->prevGpioVal;
	
	if (gpioDiffs) {
		
		uint16_t pinsRaised = gpioDiffs & newGpioVals;
		uint16_t pinsLowered = gpioDiffs & ucb->prevGpioVal;
		
		ucb->intSta |= (pinsRaised & ucb->posIntEna);
		ucb->intSta |= (pinsLowered & ucb->negIntEna);
		
		ucb->prevGpioVal = newGpioVals;
		ucb1400prvUpdateIrq(ucb);
	}
}

static void ucb1400prvSetGpioInVal(struct UCB1400 *ucb, uint8_t gpioIdx, bool high)		//allows setting VGPIOS
{
	uint16_t prevGpioVals = ucb->gpioInVals;
	
	if (high)
		ucb->gpioInVals = prevGpioVals | (1 << (uint32_t)gpioIdx);
	else
		ucb->gpioInVals = prevGpioVals &~ (1 << (uint32_t)gpioIdx);
	
	ucb1400prvRecalcGpios(ucb);
}

void ucb1400setGpioInputVal(struct UCB1400 *ucb, uint8_t gpioIdx, bool high)
{
	if (gpioIdx < NUM_REAL_GPIOS)
		ucb1400prvSetGpioInVal(ucb, gpioIdx, high);
}

static void ucb1400prvRecalcTouchIrq(struct UCB1400 *ucb)
{
	bool tsmxForIrq = true, tspxForIrq = true;
	
	if (((ucb->tscCtl >> 8) & 3) == 0) {	//irq mode
		
		if (ucb->penX >= 0 && ucb->penY >= 0) {
			tsmxForIrq = false;
			tspxForIrq = false;
		}
	}
	
	ucb1400prvSetGpioInVal(ucb, VGPIO_NUM_TSMX, tsmxForIrq);
	ucb1400prvSetGpioInVal(ucb, VGPIO_NUM_TSPX, tspxForIrq);
}

static void ucb1400prvReset(struct UCB1400 *ucb)
{
	ucb->masterVol = 0x8000;
	ucb->micVol = 0x0000;
	ucb->recSel = 0x0000;
	ucb->recGain = 0x8000;
	ucb->loopbackCtlReg = 0x0000;
	ucb->pdown = 0x000b;	//all up and ready
	ucb->extStaCtl = 0x0000;
	ucb->dacRate = 0xbb80;
	ucb->adcRate = 0xbb80;
	ucb->gpioOutLatches = 0x0000;
	ucb->gpioIsOut = 0x0000;
	ucb->posIntEna = 0x0000;
	ucb->negIntEna = 0x0000;
	ucb->intSta = 0x0000;
	ucb->tscCtl = 0x3000;		//pen up
	ucb->adcCtl = 0x0000;
	ucb->adcVal = 0x0000;
	ucb->ftrCsr1 = 0x0000;
	ucb->ftrCsr2 = 0x0000;
	ucb->extInt = 0x0000;
	ucb->prevGpioVal = ucb1400prvReadGpios(ucb);
	
	ucb1400prvUpdateIrq(ucb);
}

static bool ucb1400prvCodecRegR(void *userData, uint32_t regAddr, uint16_t *regValP)
{
	struct UCB1400 *ucb = (struct UCB1400*)userData;
	enum UCB1400REG which = (enum UCB1400REG)regAddr;
	uint16_t val;
	
	switch (which) {
		case RESET:
			val = 0x02A0;
			break;
		
		case MASTERVOL:
			val = ucb->masterVol;
			break;
		
		case MICVOL:
			val = ucb->micVol;
			break;
		
		case RECSEL:
			val = ucb->recSel;
			break;
		
		case RECGAIN:
			val = ucb->recGain;
			break;
		
		case GENPURPOSE:
			val = ucb->loopbackCtlReg;
			break;
		
		case PDOWN:
			val = ucb->pdown;
			break;
		
		case EXTDAUDIOID:
			val = 0x0001;
			break;
		
		case EXTDAUDIOCTL:
			val = ucb->extStaCtl;
			break;
		
		case DACRATE:
			val = ucb->dacRate;
			break;
		
		case ADCRATE:
			val = ucb->adcRate;
			break;
		
		case IODATA:
			val = ucb1400prvReadGpios(ucb);
			break;
		
		case IODIR:
			val = ucb->gpioIsOut;
			break;
		
		case POSINTENA:
			val = ucb->posIntEna;
			break;
		
		case NEGINTENA:
			val = ucb->negIntEna;
			break;
		
		case INTCLRSTA:
			val = ucb->intSta;
			break;
		
		case TSCCTL:
			val = ucb->tscCtl;
			break;

		case ADCCTL:
			val = ucb->adcCtl;
			break;
		
		case ADCDATA:
			val = ucb->adcVal;
			break;

		case FTRCSR1:
			val = ucb->ftrCsr1;
			break;

		case FTRCSR2:
			val = ucb->ftrCsr2;
			break;
		
		case TSTCTL:
			val = ucb->testCtl;
			break;
		
		case EXTINT:
			val = ucb->extInt;
			break;
		
		case VID1:
			val = 0x5053;
			break;
		
		case VID2:
			val = 0x4303;
			break;
		
		default:
			fprintf(stderr, "unknown reg read [0x%04x]\n", (unsigned)regAddr);
			return false;
	}
	
	//fprintf(stderr, "codec read [0x%04x] -> %04x\n", (unsigned)regAddr, (unsigned)val);
	*regValP = val;
	return true;
}

static bool ucb1400prvCodecRegW(void *userData, uint32_t regAddr, uint16_t val)
{
	struct UCB1400 *ucb = (struct UCB1400*)userData;
	enum UCB1400REG which = (enum UCB1400REG)regAddr;
	
	//fprintf(stderr, "codec wri [0x%04x] -> [0x%04x]\n", (unsigned)val, (unsigned)regAddr);
		
	switch (which) {
		
		case RESET:
			ucb1400prvReset(ucb);
			break;
		
		case MASTERVOL:
			ucb->masterVol = val & 0xbf3f;
			break;
		
		case MICVOL:
			ucb->micVol = val & 0x0040;
			break;
		
		case RECSEL:
			ucb->recSel = val & 0x0707;
			break;
		
		case RECGAIN:
			ucb->recGain = val & 0x8f0f;
			break;
		
		case GENPURPOSE:
			ucb->loopbackCtlReg = val & 0x0080;
			break;
		
		case PDOWN:	//our things become ready/shutdown instantly
			ucb->pdown = (val & 0x3b00) | (((~val) >> 8) & 0x000b);
			break;
		
		case EXTDAUDIOCTL:
			ucb->extStaCtl = val & 0x0001;
			break;
		
		case DACRATE:
			ucb->dacRate = val;
			break;
		
		case ADCRATE:
			ucb->adcRate = val;
			break;	
			
		case IODATA:
			ucb->gpioOutLatches = val & 0x01ff;
			break;
		
		case IODIR:
			ucb->gpioIsOut = val & 0x01ff;
			break;
		
		case POSINTENA:
			ucb->posIntEna = val & 0xfbff;
			break;
		
		case NEGINTENA:
			ucb->negIntEna = val & 0xfbff;
			break;
		
		case INTCLRSTA:
			ucb->intSta &=~ val;
			ucb1400prvUpdateIrq(ucb);
			break;
		
		case TSCCTL:
			ucb->tscCtl = (ucb->tscCtl & 0x3000) | (val & 0x0fff);
			ucb1400prvRecalcTouchIrq(ucb);	//if touch screen mode changed, virtual gpios to do with touch detech might have changed
			break;

		case ADCCTL:
			ucb->adcCtl = val & 0x80bf;
			
			if ((val & 0x8080) == 0x8080) {		//start something
				ucb->adcVal = 0x0000;			//in progress
				ucb1400prvSetGpioInVal(ucb, VPGIO_NUM_ADCR, 0);
			}
			break;
		
		case FTRCSR1:
			ucb->ftrCsr1 = val & 0x7ff5;
			break;

		case FTRCSR2:
			ucb->ftrCsr2 = val & 0xfc37;
			break;
		
		case TSTCTL:
			ucb->testCtl = val & 0x007f;
			break;
		
		case EXTINT:
			ucb->extInt &= (0xe000 &~ val) | 0x1fff;
			break;
		
		default:
			fprintf(stderr, "unknown reg wri [0x%04x] -> [0x%04x]\n", (unsigned)val, (unsigned)regAddr);
			return false;
	}
	return false;
}

static bool ucb1400prvUnusedRegR(void *userData, uint32_t regAddr, uint16_t *regValP)
{
	fprintf(stderr, "unexpected access to UCB1400 (RR)\n");
	return false;
}

static bool ucb1400prvUnusedRegW(void *userData, uint32_t regAddr, uint16_t val)
{
	fprintf(stderr, "unexpected access to UCB1400 (RW)\n");
	return false;
}

struct UCB1400* ucb1400Init(struct SocAC97* ac97, struct SocGpio *gpio, int8_t irqPin)
{
	struct UCB1400 *ucb = (struct UCB1400*)malloc(sizeof(*ucb));
	
	if (!ucb)
		ERR("cannot alloc UCB1400");
	
	memset(ucb, 0, sizeof (*ucb));
	
	ucb->irqPin = irqPin;
	ucb->ac97 = ac97;
	ucb->gpio = gpio;
	
	socAC97clientAdd(ac97, Ac97PrimaryAudio, ucb1400prvCodecRegR, ucb1400prvCodecRegW, ucb);
	socAC97clientAdd(ac97, Ac97SecondaryAudio, ucb1400prvUnusedRegR, ucb1400prvUnusedRegW, ucb);
	socAC97clientAdd(ac97, Ac97PrimaryModem, ucb1400prvUnusedRegR, ucb1400prvUnusedRegW, ucb);

	ucb1400prvReset(ucb);

	return ucb;
}

void ucb1400setAuxVoltage(struct UCB1400 *ucb, uint8_t adcIdx, uint32_t mV)
{
	if (adcIdx <= sizeof(ucb->adcVals) / sizeof(*ucb->adcVals)) {
		
		if (mV >= 7500)
			ucb->adcVals[adcIdx] = 0x3ff;
		else
			ucb->adcVals[adcIdx] = mV * 0x3ff / 7500;
	}
}

bool ucb1400getGpioOutputVal(struct UCB1400 *ucb, uint8_t gpioIdx)
{
	return gpioIdx < NUM_REAL_GPIOS && (ucb1400prvReadGpios(ucb) & (1 << (uint32_t)gpioIdx));
}

void ucb1400setPen(struct UCB1400 *ucb, int16_t x, int16_t y)
{
	ucb->penX = x;
	ucb->penY = y;
		
	if (x >= 0 && y >=0)
		ucb->tscCtl &=~ 0x3000;
	else
		ucb->tscCtl |= 0x3000;
	
	ucb1400prvRecalcTouchIrq(ucb);
}

static void ucb1400prvNewAudioPlaybackSample(struct UCB1400 *ucb, uint32_t samp)
{
	//nothing for now
}

static bool ucb1400prvHaveAudioOutSample(struct UCB1400 *ucb, uint32_t *sampP)
{
	*sampP = 0;
	return true;
}

static bool ucb1400prvHaveMicOutSample(struct UCB1400 *ucb, uint32_t *sampP)
{
	*sampP = 0;
	return true;
}

static bool ucb1400prvHaveModemOutSample(struct UCB1400 *ucb, uint32_t *sampP)
{
	return false;
}

static uint16_t ucb1400prvAdcConvert(struct UCB1400 *ucb, uint_fast8_t ai, uint_fast8_t tm, uint_fast8_t pinCfg)
{
	bool penDown = ucb->penX >=0 && ucb->penY >= 0;
	uint16_t ret = 0, pressure = penDown ? 200 : 1;
	
	if (ai >= 4) {
		
		uint_fast8_t idx = ai - 4;
		
		ret = ucb->adcVals[idx];
		fprintf(stderr, "UCB1400: sample AUX%u -> %u\n", idx, ret);
	}
	else if (tm >= 2) {	//TSC position
		
		if (pinCfg == 0x18) {		//pressure, no matter which pin is sampled
		
			ret = pressure;
		}
		else if (ai >= 2) {
			
			if (pinCfg == 0x12) {
				
				ret = penDown ? ucb->penX : 0;
				//fprintf(stderr, "UCB1400: sample +x -> %u\n", ret);
			}
			else if (pinCfg == 0x21) {
				
				ret = 0x3ff - (penDown ? ucb->penX : 0);
				//fprintf(stderr, "UCB1400: sample -x -> %u\n", ret);
			}
			else {
				
				fprintf(stderr, "UCB1400: x-like sample with weird pinconfig: %02x\n", pinCfg);
			}
		}
		else {
			
			if (pinCfg == 0x48) {
				
				ret = penDown ? ucb->penY : 0;
				//fprintf(stderr, "UCB1400: sample +y -> %u\n", ret);
			}
			else if (pinCfg == 0x84) {
				
				ret = 0x3ff - (penDown ? ucb->penY : 0);
				//fprintf(stderr, "UCB1400: sample -y -> %u\n", ret);
			}
			else {
				
				fprintf(stderr, "UCB1400: y-like sample with weird pinconfig: %02x\n", pinCfg);
			}
		}
	}
	else if (tm == 1) {	//pressure
		
		ret = pressure;
		
		//fprintf(stderr, "UCB1400: measure pressure 1 -> %u\n", ret);
	}
	else {
		
		fprintf(stderr, "UCB1400: unexpected ADC measurement in TSC irq mode\n");
	}
	
	return ret & 0x3ff;
}

static void ucb1400prvAdcPeriodic(struct UCB1400 *ucb)
{
	uint_fast8_t tm = (ucb->tscCtl >> 8) & 3;
	
	if (ucb->adcCtl & 0x8000) {	//ADC is on
		
		if (ucb->adcCtl & 0x0080) {		//start
			
			ucb->adcCtl &=~ 0x0080;		//self-clears
			ucb->adcVal = 0x8000 | ucb1400prvAdcConvert(ucb, (ucb->adcCtl >> 2) & 7, tm, (ucb->tscCtl >> 0) & 0xff);
			
			//conversion finished - there emight be an irq we need to send
			ucb1400prvSetGpioInVal(ucb, VPGIO_NUM_ADCR, 1);
		}
	}
}

void ucb1400periodic(struct UCB1400 *ucb)
{
	uint32_t val = 0;
	
	if (socAC97clientClientWantData(ucb->ac97, Ac97PrimaryAudio, &val))
		ucb1400prvNewAudioPlaybackSample(ucb, val);
	
	if (ucb1400prvHaveAudioOutSample(ucb, &val))
		socAC97clientClientHaveData(ucb->ac97, Ac97PrimaryAudio, val);
	
	if (ucb1400prvHaveMicOutSample(ucb, &val))
		socAC97clientClientHaveData(ucb->ac97, Ac97SecondaryAudio, val);
	
	if (ucb1400prvHaveModemOutSample(ucb, &val))
		socAC97clientClientHaveData(ucb->ac97, Ac97PrimaryModem, val);
	
	ucb1400prvAdcPeriodic(ucb);
	ucb1400prvRecalcGpios(ucb);
}







