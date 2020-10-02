//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#include "sspdev_TSC210x.h"
#include <string.h>
#include <stdlib.h>
#include "util.h"


//these match page 0
#define TSC_ADC_X		0x00
#define TSC_ADC_Y		0x01
#define TSC_ADC_Z1		0x02
#define TSC_ADC_Z2		0x03
#define TSC_ADC_BAT1	0x05
#define TSC_ADC_BAT2	0x06	//TSC2102 only
#define TSC_ADC_AUX1	0x07
#define TSC_ADC_AUX2	0x08
#define TSC_ADC_TEMP1	0x09
#define TSC_ADC_TEMP2	0x0a





struct Tsc210x {
	
	enum TscChipType chipType;
	
	struct SocGpio *gpio;
	int8_t pintdavGpio;
	
	uint16_t unreadConversions;
	uint16_t tscPenDown : 1;
	uint16_t tscBusy : 1;
	uint16_t tscFullTouchSetDone : 1;
	
	uint16_t selected	: 1;
	uint16_t gotAddr	: 1;
	uint16_t isRead		: 1;
	uint16_t pgNo		: 4;
	uint16_t addr		: 6;
	uint16_t scanPt		: 3;
	
	
	//page 0
	uint16_t tscX;
	uint16_t tscY;
	uint16_t tscZ1;
	uint16_t tscZ2;
	uint16_t tscBat1;
	uint16_t tscBat2;
	uint16_t tscAux1;
	uint16_t tscAux2;
	uint16_t tscTemp1;
	uint16_t tscTemp2;
	
	//page1:
	uint16_t tscAdc;
	uint16_t tscStatus;
	uint16_t tscBufMode;
	uint16_t tscRef;
	uint16_t tscRstCtrl;
	uint16_t tscConfig;
	uint16_t tscTempMax;
	uint16_t tscTempMin;
	uint16_t tscAux1Max;
	uint16_t tscAux1Min;
	uint16_t tscAux2Max;
	uint16_t tscAux2Min;
	uint16_t tscMeasCfg;
	uint16_t tscProgDelay;
	
	//page 2:
	uint16_t audioCtl1;
	uint16_t headsetPgaCtl;
	uint16_t dacPgaCtl;
	uint16_t mixerPgaCtl;
	uint16_t audioCtl2;
	uint16_t powerDownCtl;
	uint16_t audioCtl3;
	uint16_t filterCoeffs[20];
	uint16_t pll0x1B;
	uint16_t pll0x1C;
	uint16_t audioCtl4;
	uint16_t handPgaCtl;
	uint16_t buzzPgaCtl;
	uint16_t audioCtl5;
	uint16_t audioCtl6;
	uint16_t audioCtl7;
	uint16_t gpioCtl;
	uint16_t agcCpInCtl;
	uint16_t driverPwdnSta;
	uint16_t micAgcCtl;
	uint16_t cellAgcCtl;
	
	//these are all absolute 12-bit values. we scale them to requested size later
	uint16_t inputStimulusPenX, inputStimulusPenY, inputStimulusPenZ1, inputStimulusPenZ2;
	bool inputStimulusPenDown;
	
	//in mV
	uint16_t battery1Adc, battery2Adc, auxAdc1, auxAdc2, tempAdc1, tempAdc2;
};


static void tsc210xPrvSelect(struct Tsc210x *tsc, bool sel)
{
	if (sel) {
		tsc->selected = 1;
		tsc->gotAddr = 0;
	}
	else {
		
		//deselected. handle transaction being over if needed
	}
}

static void tsc210xPrvGpioHandler(void* userData, uint32_t gpio, bool oldState, bool newState)
{
	struct Tsc210x *tsc = (struct Tsc210x*)userData;
	
	(void)gpio;
	
	if (oldState && !newState) {
		
		tsc210xPrvSelect(tsc, true);
	}
	else if (!oldState && newState) {
		
		tsc210xPrvSelect(tsc, false);
	}
}

static void tsc210xPrvReset(struct Tsc210x *tsc)
{
	tsc->tscAdc = 0x4000;
	tsc->tscStatus = 0x8000;
	tsc->tscBufMode = 0x0200;
	tsc->tscRef = 0x0002;
	tsc->tscTempMax = 0x0fff;
	tsc->tscAux1Max = 0x0fff;
	tsc->tscAux1Min = 0x0fff;
	tsc->tscProgDelay = 0x0201;
	tsc->unreadConversions = 0;
	tsc->tscBusy = 0;
	
	tsc->headsetPgaCtl = 0xff00;
	tsc->dacPgaCtl = 0xffff;
	tsc->mixerPgaCtl = 0xc500;
	tsc->audioCtl2 = 0x4410;
	tsc->powerDownCtl = 0xfffc;
	tsc->pll0x1B = 0x1004;
	tsc->handPgaCtl = 0xff00;
	tsc->buzzPgaCtl = 0xc57c;
	tsc->audioCtl5 = 0x0006;
	tsc->audioCtl6 = 0x00e0;
	tsc->audioCtl7 = 0x0200;
	tsc->driverPwdnSta = 0xfc00;
	tsc->micAgcCtl = 0xfe00;
	tsc->cellAgcCtl = 0xfe00;
	
	tsc->filterCoeffs[0] = 27619;
	tsc->filterCoeffs[1] = -27034;
	tsc->filterCoeffs[2] = 26461;
	tsc->filterCoeffs[3] = 27619;
	tsc->filterCoeffs[4] = -27034;
	tsc->filterCoeffs[5] = 26461;
	tsc->filterCoeffs[6] = 32131;
	tsc->filterCoeffs[7] = -31506;
	tsc->filterCoeffs[8] = 32131;
	tsc->filterCoeffs[9] = -31506;
	tsc->filterCoeffs[10] = 27619;
	tsc->filterCoeffs[11] = -27034;
	tsc->filterCoeffs[12] = 26461;
	tsc->filterCoeffs[13] = 27619;
	tsc->filterCoeffs[14] = -27034;
	tsc->filterCoeffs[15] = 26461;
	tsc->filterCoeffs[16] = 32131;
	tsc->filterCoeffs[17] = -31506;
	tsc->filterCoeffs[18] = 32131;
	tsc->filterCoeffs[19] = -31506;
}

static uint16_t* tsc210xPrvGetResultRegPtr(struct Tsc210x *tsc, uint8_t which)
{
	switch (which) {
		case TSC_ADC_X:
			return &tsc->tscX;
		
		case TSC_ADC_Y:
			return &tsc->tscY;
		
		case TSC_ADC_Z1:
			return &tsc->tscZ1;
		
		case TSC_ADC_Z2:
			return &tsc->tscZ2;
		
		case TSC_ADC_BAT1:
			return &tsc->tscBat1;
		
		case TSC_ADC_BAT2:
			return (tsc->chipType == TscType2102) ? &tsc->tscBat2 : NULL;
		
		case TSC_ADC_AUX1:
			return &tsc->tscAux1;
		
		case TSC_ADC_AUX2:
			return &tsc->tscAux2;
		
		case TSC_ADC_TEMP1:
			return &tsc->tscTemp1;
		
		case TSC_ADC_TEMP2:
			return &tsc->tscTemp2;
		
		default:
			return NULL;
	}
}

static uint16_t tsc210xPrvGetStatusRegBitForMeas(struct Tsc210x *tsc, uint8_t which)
{
	(void)tsc;
	
	switch (which) {
		case TSC_ADC_X:
			return 0x0400;
		
		case TSC_ADC_Y:
			return 0x0200;
		
		case TSC_ADC_Z1:
			return 0x0100;
		
		case TSC_ADC_Z2:
			return 0x0080;
		
		case TSC_ADC_BAT1:
			return 0x0040;
		
		case TSC_ADC_BAT2:
			return 0x0020;
		
		case TSC_ADC_AUX1:
			return 0x0010;
		
		case TSC_ADC_AUX2:
			return 0x0008;
		
		case TSC_ADC_TEMP1:
			return 0x0004;
		
		case TSC_ADC_TEMP2:
			return 0x0002;
		
		default:
			return 0;
	}
}

static void tscPintdavUpdate(struct Tsc210x *tsc)
{
	bool gpioState = false;		//converted to active low later
	
	if (tsc->pintdavGpio < 0)
		return;
	
	switch (tsc->tscStatus >> 14) {
		
		case 0:
			gpioState = tsc->tscPenDown;
			break;
		
		case 1:
			gpioState = tsc->tscFullTouchSetDone && tsc->unreadConversions;
			break;
		
		case 2:
		case 3:
			gpioState = tsc->tscPenDown && !(tsc->tscFullTouchSetDone && tsc->unreadConversions);
			break;
	}
	socGpioSetState(tsc->gpio, tsc->pintdavGpio, !gpioState);
}

static uint_fast16_t tsc210xPrvSspClientProc(void* userData, uint_fast8_t nBits, uint_fast16_t got)
{
	struct Tsc210x *tsc = (struct Tsc210x*)userData;
	
	if (!tsc->selected)
		return 0;
	
	if (nBits != 16) {
		fprintf(stderr, "TSC210x expects 16-bit SSP data\n");
		return 0;
	}
	
	if (!tsc->gotAddr) {
		
		tsc->gotAddr = 1;
		tsc->isRead = got >> 15;
		tsc->pgNo = (got >> 11) & 0x0f;
		tsc->addr = (got >> 5) & 0x3f;
	}
	else if (tsc->isRead) {
		
		bool fail = false;
		uint16_t ret = 0;
		
		if (tsc->pgNo == 0) {
			
			uint16_t *resultRegP = tsc210xPrvGetResultRegPtr(tsc, tsc->addr);
			
			if (resultRegP) {
				tsc->unreadConversions &=~ (1 << tsc->addr);
				if (!tsc->unreadConversions) {
					tsc->tscFullTouchSetDone = 0;
					tsc->tscStatus &=~ 0x0800;
				}
				tsc->tscStatus &=~ tsc210xPrvGetStatusRegBitForMeas(tsc, tsc->addr);
				ret = *resultRegP;
			}
			else
				fail = true;
		}
		else if (tsc->pgNo == 1) switch (tsc->addr) {
			
			case 0:
				ret = (tsc->tscAdc & 0x3fff) | (tsc->tscPenDown ? 0x8000 : 0) | (tsc->tscBusy ? 0 : 0x4000);
				break;
			
			case 1:
				ret = tsc->tscStatus | (tsc->tscBusy ? 0 : 0x2000) | ((tsc->tscAdc & 0x8000) >> 3);
				break;
			
			case 2:
				if (tsc->chipType == TscType2101)
					ret = tsc->tscBufMode;
				else
					fail = true;
				break;
			
			case 3:
				ret = tsc->tscRef;
				break;
			
			case 4:
				ret = tsc->tscRstCtrl;
				break;
			
			case 5:
				ret = tsc->tscConfig;
				break;
			
			case 6:
				if (tsc->chipType == TscType2101)
					ret = tsc->tscTempMax;
				else
					fail = true;
				break;
			
			case 7:
				if (tsc->chipType == TscType2101)
					ret = tsc->tscTempMin;
				else
					fail = true;
				break;
			
			case 8:
				if (tsc->chipType == TscType2101)
					ret = tsc->tscAux1Max;
				else
					fail = true;
				break;
			
			case 9:
				if (tsc->chipType == TscType2101)
					ret = tsc->tscAux1Min;
				else
					fail = true;
				break;
			
			case 10:
				if (tsc->chipType == TscType2101)
					ret = tsc->tscAux2Max;
				else
					fail = true;
				break;
			
			case 11:
				if (tsc->chipType == TscType2101)
					ret = tsc->tscAux2Min;
				else
					fail = true;
				break;
			
			case 12:
				if (tsc->chipType == TscType2101)
					ret = tsc->tscMeasCfg;
				else
					fail = true;
				break;
			
			case 13:
				if (tsc->chipType == TscType2101)
					ret = tsc->tscProgDelay;
				else
					fail = true;
				break;
			
			default:
				fail = true;
				break;
		}
		else if (tsc->pgNo == 2) switch (tsc->addr) {
			
			case 0:
				ret = tsc->audioCtl1;
				break;
			
			case 1:
				if (tsc->chipType == TscType2101)
					ret = tsc->headsetPgaCtl;
				else
					fail = true;
				break;
			
			case 2:
				ret = tsc->dacPgaCtl;
				break;
			
			case 3:
				if (tsc->chipType == TscType2101)
					ret = tsc->mixerPgaCtl;
				else
					fail = true;
				break;
			
			case 4:
				ret = tsc->audioCtl2;
				break;
			
			case 5:
				ret = tsc->powerDownCtl;
				break;
			
			case 6:
				ret = tsc->audioCtl3;
				break;
			
			case 7 ... 26:
				ret = tsc->filterCoeffs[tsc->addr - 7];
				break;
			
			case 27:
				ret = tsc->pll0x1B;
				break;
			
			case 28:
				ret = tsc->pll0x1C;
				break;
			
			case 29:
				ret = tsc->audioCtl4;
				break;
			
			case 30:
				if (tsc->chipType == TscType2101)
					ret = tsc->handPgaCtl;
				else
					fail = true;
				break;
			
			case 31:
				if (tsc->chipType == TscType2101)
					ret = tsc->buzzPgaCtl;
				else
					fail = true;
				break;
			
			case 32:
				if (tsc->chipType == TscType2101)
					ret = tsc->audioCtl5 & 0xfffe;
				else
					fail = true;
				break;
			
			case 33:
				if (tsc->chipType == TscType2101)
					ret = tsc->audioCtl6;
				else
					fail = true;
				break;
			
			case 34:
				if (tsc->chipType == TscType2101)
					ret = tsc->audioCtl7;
				else
					fail = true;
				break;
			
			case 35:
				if (tsc->chipType == TscType2101)
					ret = tsc->gpioCtl;
				else
					fail = true;
				break;
			
			case 36:
				if (tsc->chipType == TscType2101)
					ret = tsc->agcCpInCtl;
				else
					fail = true;
				break;
			
			case 37:
				if (tsc->chipType == TscType2101)
					ret = tsc->driverPwdnSta;
				else
					fail = true;
				break;
			
			case 38:
				if (tsc->chipType == TscType2101)
					ret = tsc->micAgcCtl;
				else
					fail = true;
				break;
			
			case 39:
				if (tsc->chipType == TscType2101)
					ret = tsc->cellAgcCtl;
				else
					fail = true;
				break;
			
			default:
				fail = true;
				break;
		}
		else
			fail = true;
		
		if (fail)
			fprintf(stderr, "TSC read %d.0x%02x -> FAIL\n", tsc->pgNo, tsc->addr);
		else
			fprintf(stderr, "TSC read %d.0x%02x -> 0x%04x\n", tsc->pgNo, tsc->addr, ret);
			
		if (fail) {
			
			fprintf(stderr, "halt for now\n");
			while(1);
		}
		
		return ret;
	}
	else {
		
		fprintf(stderr, "TSC write %d.0x%02x <- 0x%04x\n", tsc->pgNo, tsc->addr, (unsigned)got);
		
		if (tsc->pgNo == 0) {
			
			fprintf(stderr, "TSC: ignoring write to page 0 (Reg 0x%02x)\n", tsc->addr);
			return 0;
		}
		else if (tsc->pgNo == 1) switch (tsc->addr) {
			
			case 0:
				if ((tsc->tscAdc ^ got) & 0xfc00)	//if config changes a lot, restart scan, if any
					tsc->scanPt = 0;
				tsc->tscAdc = got;
			
				tsc210xPeriodic(tsc);
				tscPintdavUpdate(tsc);
				return 0;
			
			case 1:
				tsc->tscStatus &= 0x3fff;
				tsc->tscStatus |= got & 0xc000;
				return 0;
			
			case 2:
				if (tsc->chipType == TscType2101) {
					tsc->tscBufMode &= 0x07ff;
					tsc->tscBufMode |= got & 0xf800;
				}
				else
					break;
				return 0;
			
			case 3:
				tsc->tscRef &= 0xffc0;
				tsc->tscRef |= got & 0x3f;
				return 0;
			
			case 4:
				if (got == 0xbb00)
					tsc210xPrvReset(tsc);
				return 0;
			
			case 5:
				tsc->tscConfig &= 0xff80;
				tsc->tscConfig |= got & 0x7f;
				return 0;
			
			case 6:
				if (tsc->chipType == TscType2101) {
					tsc->tscTempMax &= 0xe000;
					tsc->tscTempMax |= got & 0x1fff;
				}
				else
					break;
				return 0;
			
			case 7:
				if (tsc->chipType == TscType2101) {
					tsc->tscTempMin &= 0xe000;
					tsc->tscTempMin |= got & 0x1fff;
				}
				else
					break;
				return 0;
			
			case 8:
				if (tsc->chipType == TscType2101) {
					tsc->tscAux1Max &= 0xe000;
					tsc->tscAux1Max |= got & 0x1fff;
				}
				else
					break;
				return 0;
			
			case 9:
				if (tsc->chipType == TscType2101) {
					tsc->tscAux1Min &= 0xe000;
					tsc->tscAux1Min |= got & 0x1fff;
				}
				else
					break;
				return 0;
			
			case 10:
				if (tsc->chipType == TscType2101) {
					tsc->tscAux2Max &= 0xe000;
					tsc->tscAux2Max |= got & 0x1fff;
				}
				else
					break;
				return 0;
			
			case 11:
				if (tsc->chipType == TscType2101) {
					tsc->tscAux2Min &= 0xe000;
					tsc->tscAux2Min |= got & 0x1fff;
				}
				else
					break;
				return 0;
			
			case 12:
				if (tsc->chipType == TscType2101) {
					tsc->tscMeasCfg &= 0x01fb;
					tsc->tscMeasCfg |= got & 0xfe02;
				}
				else
					break;
				return 0;
			
			case 13:
				if (tsc->chipType == TscType2101)
					tsc->tscProgDelay = got;
				else
					break;
				return 0;
			
			default:
				break;
		}
		else if (tsc->pgNo == 2) switch (tsc->addr) {	
			case 0:
				tsc->audioCtl1 = got & 0xcf3f;
				return 0;
			
			case 1:
				if (tsc->chipType == TscType2101)
					tsc->headsetPgaCtl = got;
				//where reg does not exist, writes are ignored
				return 0;
			
			case 2:
				tsc->dacPgaCtl = got;
				tsc->audioCtl2 |= 0x000c;	//applied
				return 0;
			
			case 3:
				if (tsc->chipType == TscType2101) {
					tsc->mixerPgaCtl &= 0x0001;
					tsc->mixerPgaCtl |= got & 0xfff9;
				}
				//where reg does not exist, writes are ignored
				return 0;
			
			case 4:
				tsc->audioCtl2 &= 0x000d;
				tsc->audioCtl2 |= got & 0xfff2;
				return 0;
			
			case 5:
				tsc->powerDownCtl &= 0x003c;
				tsc->powerDownCtl |= got & 0xffc3;
				return 0;
			
			case 6:
				tsc->audioCtl3 &= 0x01c1;
				tsc->audioCtl3 |= got & 0xf808;
				return 0;
			
			case 7 ... 26:
				tsc->filterCoeffs[tsc->addr - 7] = got;
				return 0;
			
			case 27:
				tsc->pll0x1B = got & 0xfffc;
				return 0;
			
			case 28:
				tsc->pll0x1C = got & 0xfffc;
				return 0;
			
			case 29:
				tsc->audioCtl4 &= 0x0002;
				tsc->audioCtl4 |= got & 0xffc0;
				return 0;
			
			case 30:
				if (tsc->chipType == TscType2101)
					tsc->handPgaCtl = got;
				else
					break;
				return 0;
			
			case 31:
				if (tsc->chipType == TscType2101) {
					tsc->buzzPgaCtl &= 0x0083;
					tsc->buzzPgaCtl |= got & 0xff7c;
				}
				else
					break;
				return 0;
			
			case 32:
				if (tsc->chipType == TscType2101)
					tsc->audioCtl5 = got;
				else
					break;
				return 0;
			
			case 33:
				if (tsc->chipType == TscType2101)
					tsc->audioCtl6 = got & 0xfff8;
				else
					break;
				return 0;
			
			case 34:
				if (tsc->chipType == TscType2101) {
					tsc->audioCtl7 &= 0x7800;
					tsc->audioCtl7 |= got & 0x86df;
				}
				else
					break;
				return 0;
			
			case 35:
				if (tsc->chipType == TscType2101) {
					tsc->gpioCtl &= 0x1100;
					tsc->gpioCtl |= got & 0xee00;
				}
				else
					break;
				return 0;
			
			case 36:
				if (tsc->chipType == TscType2101) {
					tsc->agcCpInCtl &= 0x4000;
					tsc->agcCpInCtl |= got & 0x2fff;
				}
				else
					break;
				return 0;
			
			case 37:
				if (tsc->chipType == TscType2101) {
					tsc->driverPwdnSta &= 0xfc00;
					tsc->driverPwdnSta |= got & 0x0030;
				}
				else
					break;
				return 0;
			
			case 38:
				if (tsc->chipType == TscType2101)
					tsc->micAgcCtl = got & 0xfff8;
				else
					break;
				return 0;
			
			case 39:
				if (tsc->chipType == TscType2101) {
					tsc->cellAgcCtl &= 0x0038;
					tsc->cellAgcCtl |= got & 0xfe00;
				}
				else
					break;
				return 0;
		}
		
		fprintf(stderr, "halt for now\n");
		while(1);
	}
	
	return 0;
}

static uint_fast16_t tsc210xPrvUwireClientProc(void* userData, int_fast8_t bitsToDev, int_fast8_t bitsFromDev, uint_fast16_t sent)
{
	struct Tsc210x *tsc = (struct Tsc210x*)userData;
	
	if (bitsToDev < 0 && bitsFromDev < 0)
		tsc210xPrvSelect(tsc, sent);
	else {
		if (bitsToDev)
			(void)tsc210xPrvSspClientProc(tsc, bitsToDev, sent);
		if (bitsFromDev)
			return tsc210xPrvSspClientProc(tsc, bitsFromDev, 0);
	}
	return 0;
}

struct Tsc210x* tsc210xPrvInit(struct SocGpio *gpio, int8_t pintdavGpio /* negative for none */, enum TscChipType typ)
{
	struct Tsc210x *tsc = (struct Tsc210x*)malloc(sizeof(*tsc));
	
	if (!tsc)
		ERR("cannot alloc TSC210x");
	
	memset(tsc, 0, sizeof (*tsc));
	tsc->gpio = gpio;
	tsc->pintdavGpio = pintdavGpio;
	tsc->chipType = typ;
	
	tsc210xPrvReset(tsc);
	
	tsc->inputStimulusPenZ1 = 200;
	tsc->inputStimulusPenZ2 = 1800;

	return tsc;
}

struct Tsc210x* tsc210xInitSsp(struct SocSsp *ssp, struct SocGpio *gpio, int8_t chipSelectGpio, int8_t pintdavGpio /* negative for none */, enum TscChipType typ)
{
	struct Tsc210x *tsc = tsc210xPrvInit(gpio, pintdavGpio, typ);
	
	if (!tsc)
		return NULL;
	
	//grab our chip select line, if we have one
	if (chipSelectGpio >= 0)
		socGpioSetNotif(gpio, chipSelectGpio, &tsc210xPrvGpioHandler, tsc);
	
	//connect to the SSP
	if (!socSspAddClient(ssp, &tsc210xPrvSspClientProc, tsc))
		ERR("cannot add TSC210x to SSP\n");
	
	return tsc;
}

struct Tsc210x* tsc210xInitUWire(struct SocUwire *uw, uint_fast8_t uWireCsNo, struct SocGpio *gpio, int8_t pintdavGpio /* negative for none */, enum TscChipType typ)
{
	struct Tsc210x *tsc = tsc210xPrvInit(gpio, pintdavGpio, typ);
	
	if (!tsc)
		return NULL;
	
	//connect to the uWire bus
	if (!socUwireAddClient(uw, uWireCsNo, tsc210xPrvUwireClientProc, tsc))
		ERR("cannot add TSC210x to uWire\n");
	
	return tsc;
}

static void tsc210xPrvAdcResult(struct Tsc210x *tsc, uint_fast8_t which, uint_fast16_t val)
{
	uint16_t *resultP = tsc210xPrvGetResultRegPtr(tsc, which);
	
	//see if it needs to be vref-scaled
	switch (which) {
		case TSC_ADC_BAT1:
		case TSC_ADC_BAT2:
		case TSC_ADC_AUX1:
		case TSC_ADC_AUX2:
		case TSC_ADC_TEMP1:
		case TSC_ADC_TEMP2:
			val = (uint32_t)val * 4095 / ((tsc->tscRef & 1) ? 2500 : 1250);
			break;
		
		default:
			break;
	}
	
	//scale to desired size
	switch ((tsc->tscAdc >> 8) & 3) {
		case 0:
		case 3:
			break;
		case 1:
			val >>= 4;
			break;
		case 2:
			val >>= 2;
			break;
	}
	
	if (resultP)
		*resultP = val;
	
	tsc->tscStatus |= tsc210xPrvGetStatusRegBitForMeas(tsc, which);
	tsc->unreadConversions |= (1 << which);
	tsc->tscStatus |= 0x0800;
}

void tsc210xPeriodic(struct Tsc210x *tsc)
{
	if (tsc->inputStimulusPenDown && !tsc->tscPenDown)
		tsc->scanPt = 0;
	tsc->tscPenDown = tsc->inputStimulusPenDown;
	
	if (!(tsc->tscAdc & 0x4000)) switch ((tsc->tscAdc >> 10) & 0x0f) {
		case 0:
			break;
		
		case 1:
			if (tsc->tscPenDown) {
				//tsc->tscBusy = true;
				switch (tsc->scanPt++) {
					case 0:
						tsc210xPrvAdcResult(tsc, TSC_ADC_X, tsc->inputStimulusPenX);
						break;
					case 1:
						tsc210xPrvAdcResult(tsc, TSC_ADC_Y, tsc->inputStimulusPenY);
						tsc->scanPt = 0;
						tsc->tscFullTouchSetDone = 1;
						break;
				}
			}
			else
				tsc->tscBusy = false;
			break;
		
		case 2:
			if (tsc->tscPenDown) {
				//tsc->tscBusy = true;
				switch (tsc->scanPt++) {
					case 0:
						tsc210xPrvAdcResult(tsc, TSC_ADC_X, tsc->inputStimulusPenX);
						break;
					case 1:
						tsc210xPrvAdcResult(tsc, TSC_ADC_Y, tsc->inputStimulusPenY);
						break;
					case 2:
						tsc210xPrvAdcResult(tsc, TSC_ADC_Z1, tsc->inputStimulusPenZ1);
						break;
					case 3:
						tsc210xPrvAdcResult(tsc, TSC_ADC_Z2, tsc->inputStimulusPenZ2);
						tsc->scanPt = 0;
						tsc->tscFullTouchSetDone = 1;
						break;
				}
			}
			else
				tsc->tscBusy = false;
			break;
		
		case 3:
			tsc210xPrvAdcResult(tsc, TSC_ADC_X, tsc->inputStimulusPenX);
			tsc->tscBusy = false;
			break;
		
		case 4:
			tsc210xPrvAdcResult(tsc, TSC_ADC_Y, tsc->inputStimulusPenY);
			tsc->tscBusy = false;
			break;
		
		case 5:
			switch (tsc->scanPt++) {
				case 0:
					tsc210xPrvAdcResult(tsc, TSC_ADC_Z1, tsc->inputStimulusPenZ1);
					break;
				case 1:
					tsc210xPrvAdcResult(tsc, TSC_ADC_Z2, tsc->inputStimulusPenZ2);
					tsc->tscBusy = false;
					tsc->scanPt = 0;
					break;
			}
			break;
		
		case 6:
			tsc210xPrvAdcResult(tsc, TSC_ADC_BAT1, tsc->battery1Adc);
			tsc->tscBusy = false;
			break;
		
		case 7:
			if (tsc->chipType == TscType2102)
				tsc210xPrvAdcResult(tsc, TSC_ADC_BAT2, tsc->battery2Adc);
			else if (tsc->chipType == TscType2101)
				tsc210xPrvAdcResult(tsc, TSC_ADC_AUX2, tsc->auxAdc2);
			tsc->tscBusy = false;
			break;
		
		case 8:
			tsc210xPrvAdcResult(tsc, TSC_ADC_AUX1, tsc->auxAdc1);
			tsc->tscBusy = false;
			break;
		
		case 9:
			//not sure
			fprintf(stderr, "not sure how this mode works. halt\n");
			while(1);
			break;
		
		case 10:
			tsc210xPrvAdcResult(tsc, TSC_ADC_TEMP1, tsc->tempAdc1);
			tsc->tscBusy = false;
			break;
		
		case 11:
			switch (tsc->scanPt++) {
				case 0:
					//tsc->tscBusy = true;
					tsc210xPrvAdcResult(tsc, TSC_ADC_BAT1, tsc->battery1Adc);
					break;
				case 1:
					if (tsc->chipType == TscType2102)
						tsc210xPrvAdcResult(tsc, TSC_ADC_BAT2, tsc->battery2Adc);
					else if (tsc->chipType == TscType2101)
						tsc210xPrvAdcResult(tsc, TSC_ADC_AUX1, tsc->auxAdc1);
					break;
				case 2:
					if (tsc->chipType == TscType2102)
						tsc210xPrvAdcResult(tsc, TSC_ADC_AUX1, tsc->auxAdc1);
					else if (tsc->chipType == TscType2101)
						tsc210xPrvAdcResult(tsc, TSC_ADC_AUX2, tsc->auxAdc2);
					tsc->tscBusy = false;
					tsc->scanPt = 0;
					break;
			}
			break;
		
		case 12:
			tsc210xPrvAdcResult(tsc, TSC_ADC_TEMP2, tsc->tempAdc2);
			tsc->tscBusy = false;
			break;
		
		case 13:
		case 14:
		case 15:
			//nothing?
			break;
	}
	
	tscPintdavUpdate(tsc);
}


void tsc210xPenInput(struct Tsc210x *tsc, int16_t x, int16_t y)
{
	tsc->inputStimulusPenDown = x >=0 && y >= 0;
	
	tsc->inputStimulusPenX = x;
	tsc->inputStimulusPenY = y;
}

void tsc210xSetExtAdc(struct Tsc210x *tsc, enum TscExternalAdcType which, uint_fast16_t mV)
{
	uint_fast8_t battDiv = 0;
	
	switch (tsc->chipType) {
		case TscType2101:
			battDiv = 5;
			break;
		
		case TscType2102:
			battDiv = 6;
			break;
	}	
	
	switch (which) {
		case TscExternalAdcBat1:
			tsc->battery1Adc = mV / battDiv;	//as per spec
			break;
		
		case TscExternalAdcBat2:
			tsc->battery2Adc = mV / battDiv;	//as per spec
			break;
		
		case TscExternalAdcAux1:
			tsc->auxAdc1 = mV;
			break;
		
		case TscExternalAdcAux2:
			tsc->auxAdc2 = mV;
			break;
		
		case TscExternalAdcTemp1:
			tsc->tempAdc1 = mV;
			break;
		
		case TscExternalAdcTemp2:
			tsc->tempAdc2 = mV;
			break;
	}
}


