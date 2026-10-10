//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#ifndef _SOC_IC_H_
#define _SOC_IC_H_

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "mem.h"
#include "CPU.h"


struct SocIc;


struct SocIc* socIcInit(struct ArmCpu *cpu, struct ArmMem *physMem, uint_fast8_t socRev);

void socIcInt(struct SocIc *ic, uint_fast8_t intNum, bool raise);
bool socIcAnyUnmaskedPending(struct SocIc *ic);					//true if an interrupt unmasked in ICMR is pending (ends PXA idle mode)
bool socIcIsRaised(struct SocIc *ic, uint_fast8_t intNum);		//raw pending state, ignoring masks

#endif
