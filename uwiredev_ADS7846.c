//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#include "uwiredev_ADS7846.h"
#include <string.h>
#include <stdlib.h>
#include "util.h"

struct Ads7846 {
	struct SocGpio *gpio;
	int8_t penDownGpio;
	uint16_t selected	: 1;
	uint16_t haveReply	: 1;
	uint16_t reply		: 12;
	
	//stimuli
	uint16_t x, y, z1, z2, temp0, temp1, aux, batt;
	
	//status
	uint32_t txBuffer	: 13;
	uint32_t rxBits		: 8;
	uint32_t nRxBits 	: 4;
	uint32_t gotStart	: 1;
};


static uint_fast16_t ads7846prvMeasure(struct Ads7846 *ads, uint_fast8_t req)
{
	uint_fast8_t meas = (req >> 4) & 7;
	bool differential = !(req & 0x04);
	uint_fast16_t reply = 0;
	
	if (!(req & 0x80))
		ERR("ADS7846 expects top bit set in RXed byte. Got 0x%02x\n", (unsigned)req);
	
	if (differential) switch (meas) {
		case 1:
			reply = ads->y;
			break;
		
		case 3:
			reply = ads->z1;
			break;
		
		case 4:
			reply = ads->z2;
			break;
		
		case 5:
			reply = ads->x;
			break;
		
		default:
			fprintf(stderr, "ADS7846 cannot perform measurement %u in differential mode\n", meas);
			reply = 0;
			break;
	}
	else switch (meas) {
		case 0:
			reply = ads->temp0;
			break;
		
		case 1:
			reply = ads->y;
			break;
		
		case 2:
			reply = ads->batt;
			break;
		
		case 3:
			reply = ads->z1;
			break;
		
		case 4:
			reply = ads->z2;
			break;
		
		case 5:
			reply = ads->x;
			break;
		
		case 6:
			reply = ads->aux;
			break;
		
		case 7:
			reply = ads->temp1;
			break;
	}
	
	//truncate to 8 bits as needed
	if (req & 0x08)
		reply <<= 4;
	
	return reply;
}

static bool ads7846prvBit(struct Ads7846 *ads, bool hi)
{
	bool ret = false;
	
	if (ads->gotStart || hi) {
		if (!ads->gotStart)
			ads->nRxBits = 0;
		ads->gotStart = true;
		ads->rxBits <<= 1;
		if (hi)
			ads->rxBits++;
		ads->nRxBits++;
		
		if (ads->nRxBits == 8) {
			
			ads->gotStart = false;
			ads->txBuffer = ads7846prvMeasure(ads, ads->rxBits);
			//result begins streaming out one cycle AFTER this
		}
	}
	
	ret = (ads->txBuffer >> 12) & 1;	//this is correct since we need a bit delay before result is sent after it is prepared
	ads->txBuffer <<= 1;
	
	return ret;
}

static void ads7846prvRx(struct Ads7846 *ads, int_fast8_t numBitsGot, uint_fast16_t valGot)
{
	int_fast8_t i;
	
	valGot <<= 16 - numBitsGot;
	
	for (i = 0; i < numBitsGot; i++, valGot <<= 1)
		(void)ads7846prvBit(ads, (valGot >> 15) & 1);
}

static uint_fast16_t ads7846prvTx(struct Ads7846 *ads, int_fast8_t numBitsReqd)
{
	uint_fast16_t ret = 0;
	uint_fast8_t i;
	
	for (i = 0; i < numBitsReqd; i++) {
		
		ret <<= 1;
		ret += ads7846prvBit(ads, 0);
	}
	
	return ret;
}

static uint_fast16_t ads7846prvUwireClientProc(void* userData, int_fast8_t bitsToDev, int_fast8_t bitsFromDev, uint_fast16_t sent)
{
	struct Ads7846 *ads = (struct Ads7846*)userData;
	
	if (bitsToDev < 0 && bitsFromDev < 0) {
		ads->selected = !!sent;
		
		if (ads->selected) {
			ads->rxBits = 0;
			ads->nRxBits = 0;
			ads->haveReply = false;
		}
	}
	else if (ads->selected) {
		
		ads7846prvRx(ads, bitsToDev, sent);
		return ads7846prvTx(ads, bitsFromDev);
	}
	return 0;
}

struct Ads7846* ads7846init(struct SocUwire *uw, uint_fast8_t uWireCsNo, struct SocGpio *gpio, int8_t penDownGpio /* negative for none */)
{
	struct Ads7846 *ads = (struct Ads7846*)malloc(sizeof(*ads));
	
	if (!ads)
		ERR("cannot alloc ADS7846");
	
	memset(ads, 0, sizeof (*ads));
	ads->gpio = gpio;
	ads->penDownGpio = penDownGpio;
	
	//connect to the uWire bus
	if (!socUwireAddClient(uw, uWireCsNo, ads7846prvUwireClientProc, ads))
		ERR("cannot add ADS7846 to uWire\n");
	
	if (ads->penDownGpio >= 0)
		socGpioSetState(ads->gpio, ads->penDownGpio, true);
	
	return ads;
}

void ads7846setAdc(struct Ads7846 *ads, enum Ads7846auxType adc, uint32_t mV)
{
	uint16_t *dst;
	
	switch (adc) {
		case Ads7846auxTypeBatt:
			mV /= 4;
			dst = &ads->batt;
			break;
		
		case Ads7846auxTypeAux:
			dst = &ads->aux;
			break;
		
		case Ads7846auxTypeTemp0:
			dst = &ads->temp0;
			break;
		
		case Ads7846auxTypeTemp1:
			dst = &ads->temp1;
			break;
		
		default:
			ERR("not sure how to set aux %u in ADS7846\n", adc);
			break;
	}
	
	*dst = mV * 4095 / 2500;
}

void ads7846penInput(struct Ads7846 *ads, uint16_t x, uint16_t y, uint16_t z)	//zero z for pen up
{
	bool penDown = z > 0;
	
	ads->z1 = z;
	ads->z2 = 4095 - z;
	ads->x = x;
	ads->y = y;
	
	if (ads->penDownGpio >= 0)
		socGpioSetState(ads->gpio, ads->penDownGpio, !penDown);
}
