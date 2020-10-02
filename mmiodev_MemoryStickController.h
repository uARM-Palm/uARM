//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#ifndef _MMIO_MS_CTRLR_H_
#define _MMIO_MS_CTRLR_H_


#include "mem.h"


struct MemoryStickController;


struct MemoryStickController* msCtrlrInit(struct ArmMem *physMem, uint32_t baseAddr);


#endif
