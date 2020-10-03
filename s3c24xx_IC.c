//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#include "s3c24xx_IC.h"
#include <string.h>
#include <stdlib.h>
#include "util.h"
#include "mem.h"

#define S3C24XX_IC_BASE			0x4A000000UL
#define S3C24XX_IC_SIZE			0x20


//these irq numbers are internal as we have second-level irqs that cause them
#define S3C2440_I_CAMERA		6
#define S3C2440_I_WDT_AC97		9
#define S3C24XX_I_UART2			15
#define S3C24XX_I_UART1			23
#define S3C24XX_I_UART0			28
#define S3C24XX_I_ADC_TC		31

struct SocIc {

	struct ArmCpu *cpu;
	
	uint32_t srcpnd, intmod, intmsk, priority;
	uint16_t intsubmsk, subsrcpnd;
	int8_t intofst;	//set to -1 if none
	
	bool wasIrq, wasFiq, soc40;
};

static void socIcPrvRecalc(struct SocIc *ic)
{
	uint_fast16_t subsrcs;
	bool haveFiq, haveIrq;
	uint32_t activeIrqs;
	
	//account for sub-sources
	subsrcs = ic->subsrcpnd &~ ic->intsubmsk;
	ic->srcpnd &=~ ((1UL << S3C24XX_I_UART0) | (1UL << S3C24XX_I_UART1) | (1UL << S3C24XX_I_UART2) | (1UL << S3C24XX_I_ADC_TC) | (ic->soc40 ? ((1UL << S3C2440_I_CAMERA) | (1UL << S3C2440_I_WDT_AC97)) : 0));
	if (subsrcs & 0x0600)
		ic->srcpnd |= 1UL << S3C24XX_I_ADC_TC;
	if (subsrcs & 0x01c0)
		ic->srcpnd |= 1UL << S3C24XX_I_UART2;
	if (subsrcs & 0x0038)
		ic->srcpnd |= 1UL << S3C24XX_I_UART1;
	if (subsrcs & 0x0007)
		ic->srcpnd |= 1UL << S3C24XX_I_UART0;
	
	if (ic->soc40) {
		if (subsrcs & 0x1800)
			ic->srcpnd |= 1UL << S3C2440_I_CAMERA;
		if (subsrcs & 0x6000)
			ic->srcpnd |= 1UL << S3C2440_I_WDT_AC97;
	}
	
	//there is no mask for fiq
	haveFiq = !!(ic->srcpnd & ic->intmod);
	
	//active irqs?
	activeIrqs = ic->srcpnd & (~ic->intmod) & (~ic->intmsk);
	if (ic->intofst < 0 && activeIrqs)	//only overwrite it if it is not currently larady signalling something
		ic->intofst = __builtin_ctzl(activeIrqs);
	haveIrq = ic->intofst >= 0;
	
	if (haveIrq != ic->wasIrq)
		cpuIrq(ic->cpu, false, haveIrq);
	if (haveFiq != ic->wasFiq)
		cpuIrq(ic->cpu, true, haveFiq);
	
	ic->wasFiq = haveFiq;
	ic->wasIrq = haveIrq;
}

static bool socIcPrvMemAccessF(void *userData, uint32_t pa, uint_fast8_t size, bool write, void* buf)
{
	struct SocIc *ic = (struct SocIc*)userData;
	uint32_t val = 0, paorig = pa;
	
	if (size != 4) {
		fprintf(stderr, "%s: Unexpected %s of %u bytes to 0x%08lx\n", __func__, write ? "write" : "read", size, (unsigned long)pa);
		return false;
	}
	
	pa -= S3C24XX_IC_BASE;
	pa >>= 2;
	
	if (write)
		val = *(uint32_t*)buf;
	
	//if (write)
	//	fprintf(stderr, "IC write to 0x%08x => [0x%08x]\n", val, paorig);
	
	switch (pa) {
		
		case 0x00 / 4:
			if (write)
				ic->srcpnd &=~ val;
			else
				val = ic->srcpnd;
			break;
		
		case 0x04 / 4:
			if (write)
				ic->intmod = val;
			else
				val = ic->intmod;
			break;
		
		case 0x08 / 4:
			if (write)
				ic->intmsk = val;
			else
				val = ic->intmsk;
			break;
		
		case 0x0c / 4:
			if (write)
				ic->priority = val;
			else
				val = ic->priority;
			break;
		
		case 0x10 / 4:
			if (write) {
				
				//clearing non-current-top interrupt is pointless
				if (val & (1UL << ic->intofst))
					ic->intofst = -1;	//recalc does all the work
			}
			else
				val = (ic->intofst >= 0) ? 1UL << ic->intofst : 0;
			break;
		
		case 0x14 / 4:
			if (write)
				return false;
			else
				val = (ic->intofst >= 0) ? ic->intofst : 0xff;
			break;
		
		case 0x18 / 4:
			if (write)
				ic->subsrcpnd &=~ val;
			else
				val = ic->subsrcpnd;
			break;
		
		case 0x1c / 4:
			if (write)
				ic->intsubmsk = val & 0xffff;
			else
				val = ic->intsubmsk;
			break;
		
		default:
			return false;
	}
	
	if (write)
		socIcPrvRecalc(ic);
	
	//if (!write)
	//	fprintf(stderr, "IC READ [0x%08x] -> 0x%08x\n", paorig, val);
	
	if (!write)
		*(uint32_t*)buf = val;
	
	return true;
}

struct SocIc* socIcInit(struct ArmCpu *cpu, struct ArmMem *physMem, uint_fast8_t socRev)
{
	struct SocIc *ic = (struct SocIc*)malloc(sizeof(*ic));
	uint_fast8_t i;
	
	if (!ic)
		ERR("cannot alloc IC");
	
	memset(ic, 0, sizeof (*ic));
	
	ic->cpu = cpu;
	ic->intmsk = 0xfffffffful;
	ic->priority = 0x0000007ful;
	ic->intsubmsk = 0xffff;
	ic->intofst = -1;
	ic->soc40 = !!socRev;
	
	if (!memRegionAdd(physMem, S3C24XX_IC_BASE, S3C24XX_IC_SIZE, socIcPrvMemAccessF, ic))
		ERR("cannot add IC to MEM\n");
	
	socIcPrvRecalc(ic);
	
	return ic;
}

void socIcInt(struct SocIc *ic, uint_fast8_t intNum, bool raise)		//interrupt caused by emulated hardware
{
	if (intNum >= S3C24XX_SUB_INTS_START) {
		intNum -= S3C24XX_SUB_INTS_START;
		if (raise)
			ic->subsrcpnd |= 1UL << intNum;
		else
			ic->subsrcpnd &=~ (1UL << intNum);
	}
	else {
		
		if (raise)
			ic->srcpnd |= 1UL << intNum;
		else
			ic->srcpnd &=~ (1UL << intNum);
	}
	
	socIcPrvRecalc(ic);
}
