//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#ifndef _SD_H_
#define _SD_H_


#include "types.h"


typedef struct{
	
	uint32_t numSec;
	uint8_t HC	: 1;
	uint8_t inited	: 1;
	uint8_t SD	: 1;
	
}SD;

#define SD_BLOCK_SIZE		512

bool sdInit(SD* sd);
uint32_t sdGetNumSec(SD* sd);
bool sdSecRead(SD* sd, uint32_t sec, void* buf);
bool sdSecWrite(SD* sd, uint32_t sec, void* buf);



#endif



