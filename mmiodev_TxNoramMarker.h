//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#ifndef _MMIODEV_TX_NO_RAM_MARKER_H_
#define _MMIODEV_TX_NO_RAM_MARKER_H_


#include <stdbool.h>
#include <stdint.h>
#include "mem.h"


struct TxNoRamMarker;



struct TxNoRamMarker* txNoRamMarkerInit(struct ArmMem *physMem);


#endif
