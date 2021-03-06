//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#ifndef _DIRECT_NAND_H_
#define _DIRECT_NAND_H_

#include "soc_GPIO.h"
#include <stdio.h> 
#include "nand.h"
#include "mem.h"
#include "CPU.h"


struct DirectNAND;


struct DirectNAND* directNandInit(struct ArmMem *physMem, uint32_t baseCleAddr, uint32_t baseAleAddr, uint32_t baseDataAddr, uint32_t maskBitsAddr, struct SocGpio* gpio, int rdyPin, const struct NandSpecs *specs, FILE *nandFile);

void directNandPeriodic(struct DirectNAND *nand);

#endif