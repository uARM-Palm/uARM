//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#include "i2sdev_AK4534.h"
#include <string.h>
#include <stdlib.h>
#include "util.h"




struct AK4534 {
	
	struct SocGpio *gpio;
	
	//regs
	uint8_t regs[16];	//last two forced to zero
	
	//i2c state machine state
	struct {
		uint8_t inTransaction	: 1;
		uint8_t addrSeen		: 1;
		uint8_t ourAddr			: 1;
		uint8_t isRead			: 1;
		uint8_t regAddrSeen		: 1;
		uint8_t reg;
	} i2c;
};

static bool ak4534prvDoRegWrite(struct AK4534 *ak, uint8_t reg, uint8_t val)
{
	//fprintf(stderr, "AK4534: writing 0x%02x -> [0x%02x]\n", val, reg);
	
	if (reg < sizeof(ak->regs) / sizeof(*ak->regs)) {
		ak->regs[reg] = val;
		return true;
	}
	
	fprintf(stderr, "UNKNOWN REG WRITTEN  0x%02x -> [0x%02x]\n", val, reg);
	return false;
}

static uint8_t ak4534prvDoRegRead(struct AK4534 *ak, uint8_t reg)
{
	//fprintf(stderr, "AK4534: reading [0x%02x]\n", reg);
	
	if (reg < sizeof(ak->regs) / sizeof(*ak->regs))
		return ak->regs[reg];
	
	fprintf(stderr, "UNKNOWN REG READ: 0x%02x\n", reg);
	return 0;
}

static uint_fast8_t ak4534prvI2cHandler(void *userData, enum ActionI2C stimulus, uint_fast8_t value)
{
	struct AK4534 *ak = (struct AK4534*)userData;
	
	switch (stimulus) {
		case i2cStart:
			ak->i2c.inTransaction = 1;
			ak->i2c.regAddrSeen = 0;
			//fallthrough
		
		case i2cRestart:
			ak->i2c.addrSeen = 0;
			ak->i2c.ourAddr = 0;
			return 0;
		
		case i2cStop:
			ak->i2c.inTransaction = 0;
			return 0;
		
		case i2cTx:
			if (!ak->i2c.inTransaction)
				return 0;
			if (!ak->i2c.addrSeen) {
				ak->i2c.addrSeen = 1;
				if ((value >> 1) != 0x10)
					return 0;
				ak->i2c.ourAddr = 1;
				ak->i2c.isRead = value & 1;
				return 1;
			}
			if (!ak->i2c.ourAddr)
				return 0;
			if (ak->i2c.isRead) {
				
				fprintf(stderr, "unexpected write in read mode\n");
				return 0;
			}
			//write to us
			if (!ak->i2c.regAddrSeen) {
				ak->i2c.regAddrSeen = 1;
				ak->i2c.reg = value;
				
				return 1;
			}
			//write to a reg
			return ak4534prvDoRegWrite(ak, ak->i2c.reg++, value) ? 1 : 0;
		
		case i2cRx:
			if (!ak->i2c.inTransaction || !ak->i2c.addrSeen || !ak->i2c.ourAddr)
				return 0;
			if (!ak->i2c.isRead) {
				
				fprintf(stderr, "unexpected read in write mode\n");
				return 0;
			}
			return ak4534prvDoRegRead(ak, ak->i2c.reg++);
		
		default:
			return 0;
	}
}


struct AK4534* ak4534Init(struct SocI2c *i2c, struct SocI2s *i2s, struct SocGpio *gpio)
{
	struct AK4534 *ak = (struct AK4534*)malloc(sizeof(*ak));
	
	if (!ak)
		ERR("cannot alloc AK4534");
	
	memset(ak, 0, sizeof (*ak));
	
	ak->regs[1] = 0x80;
	ak->regs[3] = 0x03;
	ak->regs[4] = 0x02;
	ak->regs[6] = 0x11;
	ak->regs[7] = 0x01;
	ak->regs[9] = 0x40;
	ak->regs[10] = 0x36;
	ak->regs[11] = 0x10;
	
	if (!socI2cDeviceAdd(i2c, ak4534prvI2cHandler, ak))
		ERR("cannot add TSC2101 to I2C\n");
	
	return ak;
}
