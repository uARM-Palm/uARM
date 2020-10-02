//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#include "i2cdev_AN32502A.h"
#include <string.h>
#include <stdlib.h>
#include "util.h"


struct An32502A {
	
	uint8_t inTransaction	: 1;
	uint8_t addrSeen		: 1;
	uint8_t ourAddr			: 1;
	uint8_t isRead			: 1;
	uint8_t regAddrSeen		: 1;
	uint8_t reg;
};

static bool an32502aPrvDoRegWrite(struct An32502A *an, uint8_t reg, uint8_t val)
{
	fprintf(stderr, "AN: writing 0x%02x -> 0x%02x\n", val, reg);
	
	if (reg > 2)
		return false;
	
	//value not needed for emulated system
	
	return true;
}

static uint8_t an32502aPrvDoRegRead(struct An32502A *an, uint8_t reg)
{
	fprintf(stderr, "AN32502A does not support register reading\n");
	
	return 0;
}

static uint_fast8_t an32502aPrvI2cHandler(void *userData, enum ActionI2C stimulus, uint_fast8_t value)
{
	struct An32502A *an = (struct An32502A*)userData;
	
	switch (stimulus) {
		case i2cStart:
			an->inTransaction = 1;
			an->regAddrSeen = 0;
			//fallthrough
		
		case i2cRestart:
			an->addrSeen = 0;
			an->ourAddr = 0;
			return 0;
		
		case i2cStop:
			an->inTransaction = 0;
			return 0;
		
		case i2cTx:
			if (!an->inTransaction)
				return 0;
			if (!an->addrSeen) {
				an->addrSeen = 1;
				if ((value >> 1) != 0x73)
					return 0;
				an->ourAddr = 1;
				an->isRead = value & 1;
				return 1;
			}
			if (!an->ourAddr)
				return 0;
			if (an->isRead) {
				
				fprintf(stderr, "unexpected write in read mode\n");
				return 0;
			}
			//write to us
			if (!an->regAddrSeen) {
				an->regAddrSeen = 1;
				an->reg = value;
				
				return 1;
			}
			//write to a reg
			return an32502aPrvDoRegWrite(an, an->reg++, value) ? 1 : 0;
		
		case i2cRx:
			if (!an->inTransaction || !an->addrSeen || !an->ourAddr)
				return 0;
			if (!an->isRead) {
				
				fprintf(stderr, "unexpected read in write mode\n");
				return 0;
			}
			if (!an->regAddrSeen) {
				
				fprintf(stderr, "unexpected read before register specified\n");
				return 0;
			}
			return an32502aPrvDoRegRead(an, an->reg++);
		
		default:
			return 0;
	}
}

struct An32502A* an32502aInit(struct SocI2c* i2c)
{
	struct An32502A *an = (struct An32502A*)malloc(sizeof(*an));
	
	if (!an)
		ERR("cannot alloc AN32502A");
	
	memset(an, 0, sizeof (*an));
	
	if (!socI2cDeviceAdd(i2c, an32502aPrvI2cHandler, an))
		ERR("cannot add AN32502A to I2C\n");
	
	return an;
}

