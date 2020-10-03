//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#include "mmiodev_TG50uc.h"
#include <string.h>
#include <stdlib.h>
#include "util.h"


//0x100 seems to need to contain 'GA' (it contains 'IO' when an io interrupt has ben triggered it seems)
//0x0c (gpioD lower 4 bits) seems to be device revision id in lower 4 bits. nonzero is required to consider reading battery voltage at all

//164 is also likely an interrupt status word, with mask at 0x140 (or reversed)

//if 100 has 'IO' then irqs are masked by writing 1 to 0x170. unmasked by setting bit 0x0002 in 0x170
// in all cases, also 0x120 has mask which can be read and set (more than bit)

//184, 188, 18c is IRC stuff

//state on a normal clie:
/*	GPIO PORT/REG	00		20		40		60		80		a0		c0		120	
	0				e13d	1e40	0000	0000	a010	a110	0000	a110
	1				aaec	9fef	0000	0000	0000	0000	0000	0000
	2				ff00	00ff	0000	0000	ff00	ff00	0000	ff00
	3				0004	0000	0000	0000	0000	0000	0000	0000

	it is thus clear that each gpio port has 8 registers controlling it
	@0x000 is readable input state reg
	@0x020 is direction (1 = out)
	@0x040 is w1 "set hi" reg
	@0x060 is w1 "set lo" reg
	@0x080 is falling edge detect
	@0x0a0 is rising edge detect
	@0x0c0 is accumulates triggered edges, w1c
	@0x120 is whether triggered edges interrupt the main cpu!
*/


#define TG50_UC_BASE	0x08000000
#define TG50_UC_SIZE 	0x00000220


struct TG50ucGpioPort {
	//sure
	uint16_t latches;
	uint16_t inputs;
	uint16_t direction;
	uint16_t detectRise;
	uint16_t detectFall;
	uint16_t edgesDetected;
	uint16_t edgesCausingIrq;
	
	//calced
	uint16_t curState, prevState;
};


struct TG50uc {
	
	struct SocGpio *gpio;
	int8_t gpioChangeIrqNo;
	
	struct TG50ucGpioPort port[4];			//gpio[3] has revision straps on low 4 bits

	bool keys[8][8];
	uint32_t keyVals[8][8];
};



static void tg50ucPrvRecalcGpioVals(struct TG50uc *uc, struct TG50ucGpioPort *port);

static void tg50ucPrvRecalcKeypadMatrix(struct TG50uc *uc)
{
	unsigned row, col;
	
	for (col = 0; col < 8; col++) {
		
		bool hi = true, sure = false;
		
		for (row = 0; row < 8; row++) {
			
			//nothing to do if row is not energized or key is not pressed
			if ((uc->port[2].direction & (1 << row)) && uc->keys[row][col]) {
				
				if (uc->port[2].latches & (1 << row)) {
					
					if (!sure) {
						
						sure = true;
						hi = true;
					}
					else if (!hi) {
						
						fprintf(stderr, "KEYPAD COL %u conflicting values, acting as is hi\n", col);
						hi = true;
					}
				}
				else {
					
					if (!sure) {
						
						sure = true;
						hi = false;
					}
					else if (hi) {
						
						fprintf(stderr, "KEYPAD COL %u conflicting values, acting as is hi\n", col);
						hi = true;
					}
				}
			}
			if (hi)
				uc->port[2].inputs |= (1 << (col + 8));
			else
				uc->port[2].inputs &=~ (1 << (col + 8));
		}
	}
	fprintf(stderr, " UC kp 0x%04x\n", (uc->port[2].inputs & 0xff00) | (uc->port[2].latches & 0x00ff));
	tg50ucPrvRecalcGpioVals(uc, &uc->port[2]);
}

static void tg50ucPrvRecalcGpioIrqs(struct TG50uc *uc)
{
	static bool prevIrq = false;
	bool irq = false;
	unsigned i;
	
	for (i = 0; i < sizeof(uc->port) / sizeof(*uc->port); i++) {
		
		irq = irq || (uc->port[i].edgesDetected & uc->port[i].edgesCausingIrq);
	}
	
	if (prevIrq != irq) {
		fprintf(stderr, "key irq %d\n", irq);
		prevIrq  = prevIrq;
	}
	
	if (uc->gpioChangeIrqNo >= 0)
		socGpioSetState(uc->gpio, uc->gpioChangeIrqNo, !irq);
}

static void tg50ucPrvRecalcGpioVals(struct TG50uc *uc, struct TG50ucGpioPort *port)
{
	uint16_t diff, wentHi, wentLo;
	
	port->prevState = port->curState;
	port->curState = (port->latches & port->direction) | (port->inputs & ~port->direction);
	
	diff = port->prevState ^ port->curState;
	wentHi = diff & port->curState;
	wentLo = diff & port->prevState;
	
	port->edgesDetected |= wentLo & port->detectFall;
	port->edgesDetected |= wentHi & port->detectRise;
	tg50ucPrvRecalcGpioIrqs(uc);
}

static void tg50ucPrvReset(struct TG50uc *uc)
{
	uint32_t i;
	
	for (i = 0; i < sizeof(uc->port) / sizeof(*uc->port); i++) {
		uc->port[i].direction = 0;
		uc->port[i].latches = 0;
		uc->port[i].detectRise = 0;
		uc->port[i].detectFall = 0;
		uc->port[i].edgesDetected = 0;
		uc->port[i].edgesCausingIrq = 0;
		uc->port[i].curState = 0;
		uc->port[i].prevState = 0;
		tg50ucPrvRecalcGpioVals(uc, &uc->port[i]);
	}
}

static bool tg50ucPrvMemAccessF(void* userData, uint32_t pa, uint_fast8_t size, bool write, void* buf)
{
	struct TG50uc *uc = (struct TG50uc*)userData;
	bool handled = true;
	uint32_t val = 0;
	
	pa -= TG50_UC_BASE;
	
	
	if (size == 4) {
		
		if (write)
			val = *(uint32_t*)buf;
		
		switch (pa) {
			case 0x00:	//GPIOA state (RO)
			case 0x04:	//GPIOB state (RO)
			case 0x08:	//GPIOC state (RO)
			case 0x0c:	//GPIOD state (RO)
				if (write)
					handled = false;
				else
					val = uc->port[(pa >> 2) & 3].curState;
				break;
			
			case 0x20:	//GPIOA direction (RW)
			case 0x24:	//GPIOD direction (RW)
			case 0x28:	//GPIOC direction (RW)
			case 0x2c:	//GPIOD direction (RW)
				if (write)
					uc->port[(pa >> 2) & 3].direction = val;
				else {
					val = uc->port[(pa >> 2) & 3].direction;
					tg50ucPrvRecalcKeypadMatrix(uc);
					tg50ucPrvRecalcGpioVals(uc, &uc->port[(pa >> 2) & 3]);
				}
				break;
			
			case 0x40:	//GPIOA set (WO)
			case 0x44:	//GPIOB set (WO)
			case 0x48:	//GPIOC set (WO)
			case 0x4c:	//GPIOD set (WO)
				if (!write)
					handled = false;
				else {
					uc->port[(pa >> 2) & 3].latches |= val;
					tg50ucPrvRecalcKeypadMatrix(uc);
					tg50ucPrvRecalcGpioVals(uc, &uc->port[(pa >> 2) & 3]);
				}
				break;
			
			case 0x60:	//GPIOA clear (WO)
			case 0x64:	//GPIOB clear (WO)
			case 0x68:	//GPIOC clear (WO)
			case 0x6c:	//GPIOD clear (WO)
				if (!write)
					handled = false;
				else {
					uc->port[(pa >> 2) & 3].latches &=~ val;
					tg50ucPrvRecalcKeypadMatrix(uc);
					tg50ucPrvRecalcGpioVals(uc, &uc->port[(pa >> 2) & 3]);
				}
				break;
			
			case 0x80:	//GPIOA fallingEdgeDetect (RW)
			case 0x84:	//GPIOA fallingEdgeDetect (RW)
			case 0x88:	//GPIOA fallingEdgeDetect (RW)
			case 0x8c:	//GPIOA fallingEdgeDetect (RW)
				if (write)
					uc->port[(pa >> 2) & 3].detectFall = val;
				else
					val = uc->port[(pa >> 2) & 3].detectFall;
				break;
			
			case 0xa0:	//GPIOA risingEdgeDetect (RW)
			case 0xa4:	//GPIOA risingEdgeDetect (RW)
			case 0xa8:	//GPIOA risingEdgeDetect (RW)
			case 0xac:	//GPIOA risingEdgeDetect (RW)
				if (write)
					uc->port[(pa >> 2) & 3].detectRise = val;
				else
					val = uc->port[(pa >> 2) & 3].detectRise;
				break;
			
			case 0xc0:	//GPIOA edgesDetected (R, w1c)
			case 0xc4:	//GPIOB edgesDetected (R, w1c)
			case 0xc8:	//GPIOC edgesDetected (R, w1c)
			case 0xcc:	//GPIOD edgesDetected (R, w1c)
				if (write) {
					uc->port[(pa >> 2) & 3].edgesDetected &=~ val;
					tg50ucPrvRecalcGpioIrqs(uc);
				}
				else
					val = uc->port[(pa >> 2) & 3].edgesDetected;
				break;

			case 0x100:	//version query?
				if (write)
					handled = false;
				else
					val = 0x4741;	//'GA'
				break;
			
			case 0x120:	//GPIOA edgeDetectCausesIrq (RW)
			case 0x124:	//GPIOB edgeDetectCausesIrq (RW)
			case 0x128:	//GPIOC edgeDetectCausesIrq (RW)
			case 0x12c:	//GPIOD edgeDetectCausesIrq (RW)
				if (write) {
					uc->port[(pa >> 2) & 3].edgesCausingIrq = val;
					tg50ucPrvRecalcGpioIrqs(uc);
				}
				else
					val = uc->port[(pa >> 2) & 3].edgesCausingIrq;
				break;
			
			//high-power ir things
			case 0x180:
			case 0x184:
			case 0x188:
			case 0x18c:
			case 0x194:
				//fallthrough
			
			case 0x140:
			case 0x164:
			case 0x200:
			case 0x204:
			case 0x210:		//same as reset ???
				break;
			
			default:
				handled = false;
				break;
		}
		
		if (!write)
			*(uint32_t*)buf = val;
		
		if (handled) {
			
			if (write)
				fprintf(stderr, " * UC 0x%08lx -> [0x%08lx]\n", (unsigned long)val, (unsigned long)pa);
			else
				fprintf(stderr, " * UC [0x%08lx] -> 0x%08lx\n", (unsigned long)pa, (unsigned long)val);
		}
		else {
			
			if (write)
				fprintf(stderr, " * UC 0x%08lx -> [0x%08lx] FAILED\n", (unsigned long)val, (unsigned long)pa);
			else
				fprintf(stderr, " * UC [0x%08lx] -> (4 bytes) FAILED\n", (unsigned long)pa);
		}
		
		return handled;
	}
	else if (size == 1) {
		
		if (pa == 0x210 && write && *(uint8_t*)buf == 0xff) {	//reset
			
			tg50ucPrvReset(uc);
			fprintf(stderr, " * UC reset\n");
			return true;
		}
		
		if (write)
			fprintf(stderr, " * UC 0x%02x -> [0x%08x] FAILED\n", *(uint8_t*)buf, pa);
		else
			fprintf(stderr, " * UC [0x%08x] -> (1 byte) FAILED\n", pa);
		
		return false;
	}
	else {
		
		fprintf(stderr, "%s: Unexpected %s of %u bytes to 0x%08lx\n", __func__, write ? "write" : "read", size, (unsigned long)pa);
		return false;
	}
	
	return true;
}

struct TG50uc* tg50ucInit(struct ArmMem *physMem, struct SocGpio *gpio, int8_t gpioChangeIrqNo, const uint32_t *keyMap)
{
	struct TG50uc *uc = (struct TG50uc*)malloc(sizeof(*uc));
	unsigned r, c;
	
	if (!uc)
		ERR("cannot alloc TG50's UC");
	
	memset(uc, 0, sizeof (*uc));
	
	uc->gpio = gpio;
	uc->gpioChangeIrqNo = gpioChangeIrqNo;
	
	tg50ucPrvReset(uc);

	tg50ucGpioSetInState(uc, 0, 4, true);	//hold switch off
	tg50ucGpioSetInState(uc, 0, 13, true);	//in cradle
	tg50ucGpioSetInState(uc, 0, 14, true);	//charging
	tg50ucGpioSetInState(uc, 0, 15, true);	//headphones removed
	tg50ucGpioSetInState(uc, 3, 2, true);	//revision 4 (0100)
	
	for (r = 0; r < 8; r++) {
		for (c = 0; c < 8; c++)
			uc->keyVals[r][c] = *keyMap++;
	}
	
	if (!memRegionAdd(physMem, TG50_UC_BASE, TG50_UC_SIZE, tg50ucPrvMemAccessF, uc))
		ERR("cannot add TG50's UC to MEM\n");
	
	return uc;
}

void tg50ucGpioSetInState(struct TG50uc *uc, unsigned port, unsigned pin, bool hi)
{
	if (hi)
		uc->port[port].inputs |= 1 << pin;
	else
		uc->port[port].inputs &=~ (1 << pin);
	
	tg50ucPrvRecalcGpioVals(uc, &uc->port[port]);
}

void tg50ucSetKeyPressed(struct TG50uc *uc, uint32_t sdlKey, bool pressed)
{
	bool changed = false;
	unsigned r, c;
	
	for (r = 0; r < 8; r++) {
		for (c = 0; c < 8; c++) {
			if (uc->keyVals[r][c] == sdlKey) {
				if (!uc->keys[r][c] != !pressed) {
					
					changed = true;
					uc->keys[r][c] = pressed;
				}
			}
		}
	}
	
	if (changed)
		tg50ucPrvRecalcKeypadMatrix(uc);
}
