//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#ifndef _MMIO_TG50_UC_H_
#define _MMIO_TG50_UC_H_


#include "soc_GPIO.h"
#include "mem.h"

struct TG50uc;


struct TG50uc* tg50ucInit(struct ArmMem *physMem, struct SocGpio *gpio, int8_t gpioChangeIrqNo, const uint32_t *keyMap);
void tg50ucGpioSetInState(struct TG50uc* uc, unsigned port, unsigned pin, bool hi);
void tg50ucSetKeyPressed(struct TG50uc* uc, uint32_t sdlKey, bool pressed);


#endif
