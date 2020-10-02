//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#include "i2cdev_TPS65010.h"
#include <string.h>
#include <stdlib.h>
#include "util.h"


#define REG_CHGSTATUS	0x01
#define REG_REGSTATUS	0x02
#define REG_MASK1		0x03
#define REG_MASK2		0x04
#define REG_ACKINT1		0x05
#define REG_ACKINT2		0x06
#define REG_CHGCONFIG	0x07
#define REG_LED1_ON		0x08
#define REG_LED1_PER	0x09
#define REG_LED2_ON		0x0a
#define REG_LED2_PER	0x0b
#define REG_VDCDC1		0x0c
#define REG_VDCDC2		0x0d
#define REG_VREGS1		0x0e
#define REG_MASK3		0x0f
#define REG_DEFGPRIO	0x10


struct Tps65010 {
	
	uint8_t inTransaction	: 1;
	uint8_t addrSeen		: 1;
	uint8_t ourAddr			: 1;
	uint8_t isRead			: 1;
	uint8_t regAddrSeen		: 1;
	uint8_t reg;
	
	uint8_t regVals[17];
};



static bool tpsDoRegWrite(struct Tps65010 *tps, uint8_t reg, uint8_t val)
{
	uint8_t applyMask = 0xff;
	
	//fprintf(stderr, "TPS: writing 0x%02x -> 0x%02x\n", val, reg);
	
	if (reg > 0x10)
		return false;
	
	switch (reg) {
		case REG_CHGSTATUS:
			applyMask = 0x0e;
			break;
		
		case REG_REGSTATUS:		//some reags are entirely read-only
		case REG_ACKINT1:
		case REG_ACKINT2:
			applyMask = 0x00;
			break;
		
		case REG_DEFGPRIO:
			applyMask = 0xf0 | ((val & 0xf0) >> 4);	//only output pins can be written
			break;
	}
	tps->regVals[reg] &=~ applyMask;
	tps->regVals[reg] |= val & applyMask;
	
	return true;
}

static uint8_t tpsDoRegRead(struct Tps65010 *tps, uint8_t reg)
{
	uint8_t ret = 0;
	
	if (reg > 0x10)
		return 0;
	else
		ret = tps->regVals[reg];
	
	//fprintf(stderr, "TPS: reading reg 0x%02x -> 0x%02x\n", reg, ret);
	
	return ret;
}

static uint_fast8_t tps65010i2cHandler(void *userData, enum ActionI2C stimulus, uint_fast8_t value)
{
	struct Tps65010 *tps = (struct Tps65010*)userData;
	
	switch (stimulus) {
		case i2cStart:
			tps->inTransaction = 1;
			tps->regAddrSeen = 0;
			//fallthrough
		
		case i2cRestart:
			tps->addrSeen = 0;
			tps->ourAddr = 0;
			return 0;
		
		case i2cStop:
			tps->inTransaction = 0;
			return 0;
		
		case i2cTx:
			if (!tps->inTransaction)
				return 0;
			if (!tps->addrSeen) {
				tps->addrSeen = 1;
				if ((value >> 1) != 0x48)
					return 0;
				tps->ourAddr = 1;
				tps->isRead = value & 1;
				return 1;
			}
			if (!tps->ourAddr)
				return 0;
			if (tps->isRead) {
				
				fprintf(stderr, "unexpected write in read mode\n");
				return 0;
			}
			//write to us
			if (!tps->regAddrSeen) {
				tps->regAddrSeen = 1;
				tps->reg = value;
				return 1;
			}
			//write to a reg
			return tpsDoRegWrite(tps, tps->reg++, value) ? 1 : 0;
		
		case i2cRx:
			if (!tps->inTransaction || !tps->addrSeen || !tps->ourAddr)
				return 0;
			if (!tps->isRead) {
				
				fprintf(stderr, "unexpected read in write mode\n");
				return 0;
			}
			if (!tps->regAddrSeen) {
				
				fprintf(stderr, "unexpected read before register specified\n");
				return 0;
			}
			return tpsDoRegRead(tps, tps->reg++);
		
		default:
			return 0;
	}
}

struct Tps65010* tps65010Init(struct SocI2c* i2c)
{
	struct Tps65010 *tps = (struct Tps65010*)malloc(sizeof(*tps));
	
	if (!tps)
		ERR("cannot alloc TPS65010");
	
	memset(tps, 0, sizeof (*tps));
	
	tps->regVals[REG_MASK1] = 0xff;
	tps->regVals[REG_MASK2] = 0xff;
	tps->regVals[REG_CHGCONFIG] = 0x1b;
	tps->regVals[REG_VDCDC1] = 0x73;
	tps->regVals[REG_VDCDC2] = 0x68;
	tps->regVals[REG_VREGS1] = 0x88;
	
	if (!socI2cDeviceAdd(i2c, tps65010i2cHandler, tps))
		ERR("cannot add TPS65010 to I2C\n");
	
	return tps;
}

