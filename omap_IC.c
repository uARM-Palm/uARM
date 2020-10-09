//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

//p 313
//p 352



#include "omap_IC.h"
#include <string.h>
#include <stdlib.h>
#include "util.h"
#include "mem.h"

#define OMAP_IC_LV1_BASE		0xFFFECB00UL
#define OMAP_IC_LV2_BASE		0xFFFE0000UL
#define OMAP_IC_SIZE			256


struct OmapIc {	//two of these - one for each level
	
	uint32_t itr;		//state
	uint32_t mir;		//mask
	
	uint8_t irqNo, fiqNo;	//currently highest prio
	uint8_t ilr[32];
	
	
	//state
	int8_t higestIrq, higestFiq;
	bool wasIrq, wasFiq;
};

struct SocIc {

	struct ArmCpu *cpu;
	
	struct OmapIc level1;
	struct OmapIc level2;
};

static void socIcPrvRecalc(struct SocIc *ic, struct OmapIc *oic)
{
	int_fast8_t highestIrqPrio = -1, highestFiqPrio = -1, highestIrqNo, highestFiqNo;
	uint32_t unmasked = oic->itr &~ oic->mir, bit;
	bool nowIrq, nowFiq;
	uint_fast8_t i;
	
	
	for (i = 0, bit = 1; bit; bit <<= 1, i++) {
		
		int_fast8_t prio = oic->ilr[i] >> 2;
		int_fast8_t *highestP, *irqNoP;
		
		if (!(unmasked & bit))
			continue;
		
		if (oic->ilr[i] & 1) {
			highestP = &highestFiqPrio;
			irqNoP = &highestFiqNo;
		}
		else {
			highestP = &highestIrqPrio;
			irqNoP = &highestIrqNo;
		}
		
		if (*highestP < prio) {
			*highestP = prio;
			*irqNoP = i;
		}
	}
	
	nowFiq = highestFiqPrio >= 0;
	nowIrq = highestIrqPrio >= 0;
	
	oic->higestFiq = nowFiq ? highestFiqNo : -1;
	oic->higestIrq = nowIrq ? highestIrqNo : -1;
	
	if (nowFiq != oic->wasFiq && oic == &ic->level1) {		//only level 1 can generate FIQ
		cpuIrq(ic->cpu, true, nowFiq);
		//fprintf(stderr, "IRQ: cpu FIQ %s. highest prio %d for irq %d\n", nowFiq ? "ON" : "OFF", highestFiqPrio, highestFiqNo);
	}
	
	if (oic == &ic->level1) {								//level 1 triggers cpy
		if (nowIrq != oic->wasIrq)
			cpuIrq(ic->cpu, false, nowIrq);
		//fprintf(stderr, "IRQ: cpu IRQ %s. highest prio %d for irq %d\n", nowIrq ? "ON" : "OFF", highestIrqPrio, highestIrqNo);
	}
	else {
		//fprintf(stderr, "IRQ: 2nd LVL IRQ %s. highest prio %d for irq %d\n", nowIrq ? "ON" : "OFF", highestIrqPrio, highestIrqNo);
		socIcInt(ic, OMAP_I_LEVEL_2, nowIrq);
	}

	oic->wasFiq = nowFiq;
	oic->wasIrq = nowIrq;
}

static bool socIcPrvMemAccessF(struct SocIc *ic, struct OmapIc *oic, uint32_t pa, uint_fast8_t size, bool write, void* buf)
{
	uint32_t val = 0;
	uint32_t paorig = pa;
	
	if (size != 4) {
		fprintf(stderr, "%s: Unexpected %s of %u bytes to 0x%08lx\n", __func__, write ? "write" : "read", size, (unsigned long)pa);
		return false;
	}
	
	pa >>= 2;
	
	if (write)
		val = *(uint32_t*)buf;
	
	//if (write)
	//	fprintf(stderr, "IC write to 0x%08lx => [0x%08lx]\n", (unsigned long)val, (unsigned long)paorig);
	
	switch (pa) {
		case 0x00 / 4:
			if (write)
				oic->itr &= val;
			else
				val = oic->itr;
			break;
		
		case 0x04 / 4:
			if (write)
				oic->mir = val;
			else
				val = oic->mir;
			break;
		
		case 0x10 / 4:
			if (write)
				return false;
			else {
				if (oic->higestIrq < 0)
					val = 0xf000000ful;	//why not?
				else {
					val = oic->higestIrq;
					if (!(oic->ilr[val] & 2))	//edge triggered gets cleared
						oic->itr &=~ 1UL << val;
				}
			}
			break;
		
		case 0x14 / 4:
			if (write)
				return false;
			else {
				if (oic->higestFiq < 0)
					val = 0xf000000ful;	//why not?
				else {
					val = oic->higestFiq;
					if (!(oic->ilr[val] & 2))	//edge triggered gets cleared
						oic->itr &=~ 1UL << val;
				}
			}
			break;
		
		case 0x18 / 4:
			//no need to do anything - we clear right away in recalc - i hope that is ok
			val = 0;
			break;
		
		case 0x1c / 4 ... 0x98 / 4:
			if (write)
				oic->ilr[pa - 0x1c / 4] = val & 0x7f;
			else
				val = oic->ilr[pa - 0x1c / 4];
			break;
		
		case 0x9c / 4:
			if (write) {
				uint_fast8_t i;
				
				for (i = 0; i < 32; i++) {
					if ((val & (1UL << i)) && !(oic->ilr[val] & 2))
						oic->itr |= 1UL << i;
				}
			}
			else
				val = 0;
			break;
		
		default:
			return false;
	}
	
	//if (!write)
	//	fprintf(stderr, "IC READ [0x%08lx] -> 0x%08lx\n", (unsigned long)paorig, (unsigned long)val);
	
	socIcPrvRecalc(ic, oic);
	
	if (!write)
		*(uint32_t*)buf = val;
	
	return true;
}

static bool socIcPrvLv1memAccessF(void* userData, uint32_t pa, uint_fast8_t size, bool write, void* buf)
{
	struct SocIc *ic = (struct SocIc*)userData;
	
	return socIcPrvMemAccessF(ic, &ic->level1, pa - OMAP_IC_LV1_BASE, size, write, buf);
}

static bool socIcPrvLv2memAccessF(void* userData, uint32_t pa, uint_fast8_t size, bool write, void* buf)
{
	struct SocIc *ic = (struct SocIc*)userData;
	
	return socIcPrvMemAccessF(ic, &ic->level2, pa - OMAP_IC_LV2_BASE, size, write, buf);
}

struct SocIc* socIcInit(struct ArmCpu *cpu, struct ArmMem *physMem, uint_fast8_t socRev)
{
	struct SocIc *ic = (struct SocIc*)malloc(sizeof(*ic));
	uint_fast8_t i;
	
	if (!ic)
		ERR("cannot alloc IC");
	
	memset(ic, 0, sizeof (*ic));
	
	ic->cpu = cpu;
	//most are level-triggered
	for (i = 0; i < 16; i++) {
		ic->level1.ilr[i] |= 2;
		ic->level1.ilr[i] |= 2;
	}
	
	//some irqs start as edge-triggered
	ic->level1.ilr[3] &=~ 2;
	ic->level1.ilr[4] &=~ 2;
	ic->level1.ilr[5] &=~ 2;
	ic->level1.ilr[26] &=~ 2;
	ic->level2.ilr[1] &=~ 2;
	ic->level2.ilr[2] &=~ 2;
	ic->level2.ilr[3] &=~ 2;
	ic->level2.ilr[4] &=~ 2;
	ic->level2.ilr[10] &=~ 2;
	ic->level2.ilr[11] &=~ 2;
	ic->level2.ilr[12] &=~ 2;
	ic->level2.ilr[13] &=~ 2;
	ic->level2.ilr[22] &=~ 2;
	ic->level2.ilr[25] &=~ 2;
	
	if (!memRegionAdd(physMem, OMAP_IC_LV1_BASE, OMAP_IC_SIZE, socIcPrvLv1memAccessF, ic))
		ERR("cannot add IC.1 to MEM\n");
	
	if (!memRegionAdd(physMem, OMAP_IC_LV2_BASE, OMAP_IC_SIZE, socIcPrvLv2memAccessF, ic))
		ERR("cannot add IC.2 to MEM\n");
	
	return ic;
}

static void socIcPrvInt(struct SocIc *ic, struct OmapIc *oic, uint_fast8_t localIrqNo, bool raise)
{
	if (raise)
		oic->itr |= 1UL << localIrqNo;
	else if (oic->ilr[localIrqNo] & 2)	//only level irqs can get deasserted
		oic->itr &=~ (1UL << localIrqNo);

	socIcPrvRecalc(ic, oic);
}

void socIcInt(struct SocIc *ic, uint_fast8_t intNum, bool raise)		//interrupt caused by emulated hardware
{
	//int8 for compiler quieting
	if ((int8_t)intNum >= OMAP_LV1_MIN && intNum <= OMAP_LV1_MAX)
		socIcPrvInt(ic, &ic->level1, intNum - OMAP_LV1_MIN, raise);
	else if (intNum >= OMAP_LV2_MIN && intNum <= OMAP_LV2_MAX)
		socIcPrvInt(ic, &ic->level2, intNum - OMAP_LV2_MIN, raise);
	else
		ERR("irq %u doesn't exist\n", intNum);
}
