//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#ifndef _PXA_PWR_CLK_H_
#define _PXA_PWR_CLK_H_

#include "mem.h"
#include "CPU.h"

struct PxaPwrClk;


struct PxaPwrClk* pxaPwrClkInit(struct ArmCpu *cpu, struct ArmMem *physMem, bool isPXA270);

bool pxaPwrClkIsSleeping(struct PxaPwrClk *pc);						//while true, the CPU must not execute
bool pxaPwrClkIsIdle(struct PxaPwrClk *pc);							//PWRMODE=1 requested and not yet ended
void pxaPwrClkEndIdle(struct PxaPwrClk *pc);							//an unmasked interrupt arrived
void pxaPwrClkRtcAlarm(struct PxaPwrClk *pc);							//RTC alarm pending: wakes from sleep if PWER.WERTC
void pxaPwrClkGpioEdge(void *userData, uint32_t gpioNum, bool rising);	//GpioEdgeF, wakes from sleep per PWER/PRER/PFER





#endif
