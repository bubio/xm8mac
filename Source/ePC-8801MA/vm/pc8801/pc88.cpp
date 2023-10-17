/*
	NEC PC-98DO Emulator 'ePC-98DO'
	NEC PC-8801MA Emulator 'ePC-8801MA'
	NEC PC-8001mkIISR Emulator 'ePC-8001mkIISR'

	Author : Takeda.Toshiya
	Date   : 2011.12.29-

	[ PC-8801 ]
*/

#include "pc88.h"
#include "../event.h"
#include "../i8251.h"
#include "../pcm1bit.h"
#include "../upd1990a.h"
#ifdef SDL
#include "../fmsound.h"
#include "../i8255.h"
#else
#include "../ym2203.h"
#endif // SDL
#include "../z80.h"

#define DEVICE_JOYSTICK	0
#define DEVICE_MOUSE	1
#define DEVICE_JOYMOUSE	2	// not supported yet

#define EVENT_TIMER	0
#define EVENT_BUSREQ	1
#define EVENT_CMT_SEND	2
#define EVENT_CMT_DCD	3
#define EVENT_BEEP	4

#define IRQ_USART	0
#define IRQ_VRTC	1
#define IRQ_TIMER	2
#define IRQ_SOUND	4

#define Port30_40	!(port[0x30] & 0x01)
#define Port30_COLOR	!(port[0x30] & 0x02)
#define Port30_MTON	(port[0x30] & 0x08)
#define Port30_CMT	!(port[0x30] & 0x20)
#define Port30_RS232C	(port[0x30] & 0x20)

#define Port31_MMODE	(port[0x31] & 0x02)
#define Port31_RMODE	(port[0x31] & 0x04)
#define Port31_GRAPH	(port[0x31] & 0x08)
#define Port31_HCOLOR	(port[0x31] & 0x10)
#define Port31_400LINE	!(port[0x31] & 0x11)

#define Port31_V1_320x200	(port[0x31] & 0x10)	// PC-8001 (V1)
#define Port31_V1_MONO		(port[0x31] & 0x04)	// PC-8001 (V1)

#define Port31_COLOR	(port[0x31] & 0x10)	// PC-8001
#define Port31_320x200	(port[0x31] & 0x04)	// PC-8001

#define Port32_EROMSL	(port[0x32] & 0x03)
#define Port32_TMODE	(port[0x32] & 0x10)
#define Port32_PMODE	(port[0x32] & 0x20)
#define Port32_GVAM	(port[0x32] & 0x40)
#define Port32_SINTM	(port[0x32] & 0x80)

#define Port33_SINTM	(port[0x33] & 0x02)	// PC-8001
#define Port33_GVAM	(port[0x33] & 0x40)	// PC-8001
#define Port33_N80SR	(port[0x33] & 0x80)	// PC-8001

#define Port34_ALU	port[0x34]

#define Port35_PLN0	(port[0x35] & 0x01)
#define Port35_PLN1	(port[0x35] & 0x02)
#define Port35_PLN2	(port[0x35] & 0x04)
#define Port35_GDM	(port[0x35] & 0x30)
#define Port35_GAM	(port[0x35] & 0x80)

#define Port40_GHSM	(port[0x40] & 0x10)
#define Port40_JOP1	(port[0x40] & 0x40)

#define Port44_OPNCH	port[0x44]

#define Port53_TEXTDS	(port[0x53] & 0x01)
#define Port53_G0DS	(port[0x53] & 0x02)
#define Port53_G1DS	(port[0x53] & 0x04)
#define Port53_G2DS	(port[0x53] & 0x08)
#define Port53_G3DS	(port[0x53] & 0x10)	// PC-8001
#define Port53_G4DS	(port[0x53] & 0x20)	// PC-8001
#define Port53_G5DS	(port[0x53] & 0x40)	// PC-8001

#define Port70_TEXTWND	port[0x70]

#define Port71_EROM	port[0x71]

#ifdef SDL
#define PortA8_OPNCH	port[0xa8]
#define PortAA_S2INTM	(port[0xaa] & 0x80)
#endif // SDL

#define PortE2_RDEN	(port[0xe2] & 0x01)
#define PortE2_WREN	(port[0xe2] & 0x10)

#ifdef PC88_IODATA_EXRAM
#define PortE3_ERAMSL	port[0xe3]
#else
#define PortE3_ERAMSL	(port[0xe3] & 0x0f)
#endif

#define PortE8E9_KANJI1	(port[0xe8] | (port[0xe9] << 8))
#define PortECED_KANJI2	(port[0xec] | (port[0xed] << 8))

#define PortF0_DICROMSL	(port[0xf0] & 0x1f)
#define PortF1_DICROM	!(port[0xf1] & 0x01)

#ifdef SDL

// xm8_ext_flags (reset() clears upper 16bits)
#define XM8_EXT_NO_DICROM		0x00000001
#define XM8_EXT_SB2_IRQ			0x00010000
#define XM8_EXT_BAUDRATE		0x001e0000
#define XM8_EXT_BAUDRATE_SHIFT	17

// config.dipswitch
#define XM8_DIP_MEMWAIT			0x00000001
#define XM8_DIP_8MHZH			0x00000002
#define XM8_DIP_NOEXRAM			0x00000004
#define XM8_DIP_TERMMODE		0x00000008
#define XM8_DIP_WIDTH40			0x00000010
#define XM8_DIP_LINE25			0x00000020
#define XM8_DIP_BOOTROM			0x00000040
#define XM8_DIP_BAUDRATE		0x00000780
#define XM8_DIP_BAUDRATE_SHIFT	7
#define XM8_DIP_HALFDUPLEX		0x00000800
#define XM8_DIP_DATA7BIT		0x00001000
#define XM8_DIP_STOP2BIT		0x00002000
#define XM8_DIP_DISABLEX		0x00004000
#define XM8_DIP_ENABLES			0x00008000
#define XM8_DIP_DISABLEDEL		0x00010000
#define XM8_DIP_PARITY			0x00060000
#define XM8_DIP_PARITY_SHIFT	17

// NORMAL ACCESS(0) or ALU/TextWND/DIC ACCESS(1)
#define GVAMTDIC_BIT_SHIFT	14
#define GVAMTDIC_BIT_MASK	0x4000
// STD SPEED(0) or HIGH SPEED(1)
#define GHSM_BIT_SHIFT		13
#define GHSM_BIT_MASK		0x2000
// GRAPHIC OFF(0) or GRAPHIC ON(1)
#define GRPHE_BIT_SHIFT		12
#define GRPHE_BIT_MASK		0x1000
// VDISP(0) or VBLANK(1)
#define VRTC_BIT_SHIFT		11
#define VRTC_BIT_MASK		0x0800
// ROMRAM(0) or 64K RAM(1)
#define MMODE_BIT_SHIFT		10
#define MMODE_BIT_MASK		0x0400
// N88-BASIC ROM(0) or N-BASIC ROM(1)
#define RMODE_BIT_SHIFT		9
#define RMODE_BIT_MASK		0x0200
// N88-BASIC EXT ROM(0) or N88-BASIC ROM(1)
#define IEROM_BIT_SHIFT		8
#define IEROM_BIT_MASK		0x0100
// N88-BASIC EXT ROM BANK SELECT(0-3)
#define EROMSL_BIT_SHIFT	6
#define EROMSL_BIT_MASK		0x00C0
// MAIN RAM(0) or EXRAM(1)
#define REWE_BIT_SHIFT		5
#define REWE_BIT_MASK		0x0020
// EXRAM BANK SELECT(0-3)
#define BS_BIT_SHIFT		3
#define BS_BIT_MASK			0x0018
// MAIN RAM(0) or GVRAM(1-3)
#define GVRAM_BIT_SHIFT		1
#define GVRAM_BIT_MASK		0x0006
// TVRAM(0) or MAIN RAM(1)
#define TMODE_BIT_SHIFT		0
#define TMODE_BIT_MASK		0x0001

int PC88::get_main_wait(int pattern, bool read, bool c000)
{
	int wait;

	// GVAMTDIC
	wait = 0;
	if ((pattern & GVAMTDIC_BIT_MASK) != 0) {
		if (c000 == true) {
			// 0xc000-0xffff
			wait = GVAMTDIC_BIT_MASK;
		}
	}

	if (cpu_clock_low == true) {
		// 4MHz
		if ((config.dipswitch & XM8_DIP_MEMWAIT) != 0) {
			if (read == true) {
				// memory wait + read
				wait += 1;
			}
		}
	}
	else {
		// 8MHz
		if ((config.dipswitch & XM8_DIP_8MHZH) == 0) {
			// not 8MHzH
			wait += 1;
		}
		if ((config.dipswitch & XM8_DIP_MEMWAIT) != 0) {
			// memory wait (read and write)
			wait += 1;
		}
	}

	return wait;
}

int PC88::get_tvram_wait(int pattern, bool read)
{
	int wait;

	// GVAMTDIC
	wait = 0;
	if ((pattern & GVAMTDIC_BIT_MASK) != 0) {
		// 0xf000-0xffff
		wait = GVAMTDIC_BIT_MASK;
	}

	if (cpu_clock_low == true) {
		// 4MHz
		if (read == true) {
			if ((config.dipswitch & XM8_DIP_MEMWAIT) != 0) {
				// memory wait + read
				wait += 1;
			}
		}
	}
	else {
		// 8MHz -> memory wait do not effect
		if (read == true) {
			// read
			wait += 2;
		}
		else {
			// write
			wait += 1;
		}
	}

	return wait;
}

int PC88::get_gvram_wait(int pattern, bool read)
{
	int wait;

	// GVAMTDIC
	wait = 0;
	if ((pattern & GVAMTDIC_BIT_MASK) != 0) {
		// 0xc000-0xffff
		wait = GVAMTDIC_BIT_MASK;
	}

	if ((pattern & GRPHE_BIT_MASK) != 0) {
		// graphic on
		if (cpu_clock_low == true) {
			// 4MHz
			if ((config.boot_mode == MODE_PC88_V1S) || (config.boot_mode == MODE_PC88_N)) {
				// V1S
				if ((pattern & (GHSM_BIT_MASK | VRTC_BIT_MASK)) == 0) {
					// V1S + not GHSM, V-DISP
					if (config.monitor_type == 0) {
						wait += 114;
					}
					else {
						wait += 68;
					}
				}
				else {
					if ((pattern & VRTC_BIT_MASK) != 0) {
						wait += 0;
					}
					else {
						wait += 2;
					}
				}
			}
			else {
				// V1H or V2
				if ((pattern & VRTC_BIT_MASK) != 0) {
					wait += 0;
				}
				else {
					wait += 2;
				}
			}
		}
		else {
			// 8MHz
			if ((config.boot_mode == MODE_PC88_V1S) || (config.boot_mode == MODE_PC88_N)) {
				// V1S
				if ((pattern & (GHSM_BIT_MASK | VRTC_BIT_MASK)) == 0) {
					// V1S + not GHSM, V-DISP
					if (config.monitor_type == 0) {
						wait += 141;
					}
					else {
						wait += 90;
					}
				}
				else {
					if ((pattern & VRTC_BIT_MASK) != 0) {
						wait += 3;
					}
					else {
						wait += 5;
					}
				}
			}
			else {
				// V1H or V2
				if ((pattern & VRTC_BIT_MASK) != 0) {
					wait += 3;
				}
				else {
					wait += 5;
				}
			}
		}
	}
	else {
		// graphic off
		if (cpu_clock_low == true) {
			// 4MHz
			if ((config.dipswitch & XM8_DIP_MEMWAIT) != 0) {
				if (read == true) {
					// memory wait + read
					wait += 1;
				}
			}
		}
		else {
			// 8MHz -> memory wait do not effect
			wait += 3;
		}
	}

	return wait;
}

int PC88::get_m1_wait(int pattern, uint32 addr)
{
	int wait;

	// initialize
	wait = 0;

	if ((config.boot_mode == MODE_PC88_V1S) || (config.boot_mode == MODE_PC88_N)) {
		// V1S or N
		if ((config.dipswitch & XM8_DIP_MEMWAIT) == 0) {
			// memory wait = off
			if (cpu_clock_low == true) {
				// 4MHz
				wait += 1;
			}
		}
	}
	else {
		// V1H or V2
		if ((config.dipswitch & XM8_DIP_MEMWAIT) == 0) {
			// no memory wait
			if (addr >= 0xf000) {
				// TVRAM
				if ((pattern & (GVRAM_BIT_MASK | TMODE_BIT_MASK)) == 0) {
					if (cpu_clock_low == true) {
						// TVRAM, 4MHz only
						wait += 1;
					}
				}
			}
		}
	}

	return wait;
}

void PC88::create_pattern(int pattern, bool read)
{
	uint8 **bank_ptr;
	int *wait_ptr;
	int *m1_ptr;
	uint8 *top;
	int loop;
	int wait;
	int m1;

	// initialize pointer
	if (read == true) {
		bank_ptr = &read_banks[(pattern & ~(GVAMTDIC_BIT_MASK | GHSM_BIT_MASK | GRPHE_BIT_MASK | VRTC_BIT_MASK)) * 0x40];
		wait_ptr = &read_waits[pattern * 0x40];
		m1_ptr = &m1_waits[(pattern & ~(GVAMTDIC_BIT_MASK | GHSM_BIT_MASK | GRPHE_BIT_MASK | VRTC_BIT_MASK)) * 0x40];
	}
	else {
		bank_ptr = &write_banks[(pattern & ~(GVAMTDIC_BIT_MASK | GHSM_BIT_MASK | GRPHE_BIT_MASK | VRTC_BIT_MASK)) * 0x40];
		wait_ptr = &write_waits[pattern * 0x40];
		m1_ptr = NULL;
	}

	// 0x0000 - 0x5fff
	top = &n88rom[0];
	wait = get_main_wait(pattern, read, false);
	m1 = get_m1_wait(pattern, 0x0000);
	if ((pattern & REWE_BIT_MASK) != 0) {
		// EXRAM
		top = &exram[((pattern & BS_BIT_MASK) >> BS_BIT_SHIFT) * 0x8000];
	}
	else {
		if (read == true) {
			// read
			if ((pattern & MMODE_BIT_MASK) != 0) {
				// 64K RAM
				top = &ram[0];
			}
			else {
				// ROMRAM
				if ((pattern & RMODE_BIT_MASK) != 0) {
					// N-BASIC ROM
					top = &n80rom[0];
				}
				else {
					// N88-BASIC ROM
					top = &n88rom[0];
				}
			}
		}
		else {
			// write
			top = &ram[0];
		}
	}

	// 0x0000-0x5fff
	for (loop=0; loop<24; loop++) {
		*bank_ptr++ = top;
		*wait_ptr++ = wait;
		if (m1_ptr != NULL) {
			*m1_ptr++ = m1;
		}
		top += 0x400;
	}

	// 0x6000 - 0x7fff
	if ((pattern & REWE_BIT_MASK) != 0) {
		// EXRAM
		top = &exram[((pattern & BS_BIT_MASK) >> BS_BIT_SHIFT) * 0x8000 + 0x6000];
	}
	else {
		if (read == true) {
			// read
			if ((pattern & MMODE_BIT_MASK) != 0) {
				// 64K RAM
				top = &ram[0x6000];
			}
			else {
				// ROMRAM
				if ((pattern & RMODE_BIT_MASK) != 0) {
					// N-BASIC ROM
					top = &n80rom[0x6000];
				}
				else {
					// N88-BASIC ROM
					if ((pattern & IEROM_BIT_MASK) == 0) {
						// N88-BASIC EXT ROM
						top = &n88exrom[((pattern & EROMSL_BIT_MASK) >> EROMSL_BIT_SHIFT) * 0x2000];
					}
					else {
						top = &n88rom[0x6000];
					}
				}
			}
		}
		else {
			// write
			top = &ram[0x6000];
		}
	}

	// 0x6000-0x7fff
	for (loop=0; loop<8; loop++) {
		*bank_ptr++ = top;
		*wait_ptr++ = wait;
		if (m1_ptr != NULL) {
			*m1_ptr++ = m1;
		}
		top += 0x400;
	}

	// 8000-bfff
	top = &ram[0x8000];

	// 8000-83ff
	if (((pattern & MMODE_BIT_MASK) == 0) && ((pattern & RMODE_BIT_MASK) == 0)) {
		// text window on
		*bank_ptr++ = top;
		*wait_ptr++ = (GVAMTDIC_BIT_MASK | wait);
		if (m1_ptr != NULL) {
			*m1_ptr++ = m1;
		}
	}
	else {
		// text window off
		*bank_ptr++ = top;
		*wait_ptr++ = wait;
		if (m1_ptr != NULL) {
			*m1_ptr++ = m1;
		}
	}
	top += 0x400;

	// 8400-bfff
	for (loop=0; loop<15; loop++) {
		*bank_ptr++ = top;
		*wait_ptr++ = wait;
		if (m1_ptr != NULL) {
			*m1_ptr++ = m1;
		}
		top += 0x400;
	}

	// c000-efff
	if ((pattern & GVRAM_BIT_MASK) != 0) {
		// GVRAM
		wait = get_gvram_wait(pattern, read);
		m1 = get_m1_wait(pattern, 0xc000);
		top = &gvram[(((pattern & GVRAM_BIT_MASK) >> GVRAM_BIT_SHIFT) - 1) * 0x4000];
	}
	else {
		// 64K RAM
		wait = get_main_wait(pattern, read, true);
		m1 = get_m1_wait(pattern, 0xc000);
		top = &ram[0xc000];
	}

	// c000-efff
	for (loop=0; loop<12; loop++) {
		*bank_ptr++ = top;
		*wait_ptr++ = wait;
		if (m1_ptr != NULL) {
			*m1_ptr++ = m1;
		}
		top += 0x400;
	}

	// f000-ffff
	if ((pattern & GVRAM_BIT_MASK) != 0) {
		// GVRAM
		wait = get_gvram_wait(pattern, read);
		m1 = get_m1_wait(pattern, 0xf000);
		top = &gvram[(((pattern & GVRAM_BIT_MASK) >> GVRAM_BIT_SHIFT) - 1) * 0x4000 + 0x3000];
	}
	else {
		if ((pattern & TMODE_BIT_MASK) != 0) {
			// 64K RAM
			wait = get_main_wait(pattern, read, true);
			m1 = get_m1_wait(pattern, 0xf000);
			top = &ram[0xf000];
		}
		else {
			// TVRAM
			wait = get_tvram_wait(pattern, read);
			m1 = get_m1_wait(pattern, 0xf000);
			top = &tvram[0];
		}
	}

	// f000-ffff
	for (loop=0; loop<4; loop++) {
		*bank_ptr++ = top;
		*wait_ptr++ = wait;
		if (m1_ptr != NULL) {
			*m1_ptr++ = m1;
		}
		top += 0x400;
	}
}

void PC88::update_memmap(int pattern, bool read)
{
	// create pattern
	if (pattern < 0) {
		pattern = 0;

		// GVAM
		if ((gvram_sel == 8) || (PortF1_DICROM != 0)) {
			pattern |= GVAMTDIC_BIT_MASK;
		}

		// GHSM
		if (Port40_GHSM) {
			pattern |= GHSM_BIT_MASK;
		}

		// GRPHE
		if (Port31_GRAPH) {
			pattern |= GRPHE_BIT_MASK;
		}

		// VRTC
		if (crtc.vblank) {
			pattern |= VRTC_BIT_MASK;
		}

		// MMODE
		if (Port31_MMODE) {
			pattern |= MMODE_BIT_MASK;
		}

		// RMODE
		if (Port31_RMODE) {
			pattern |= RMODE_BIT_MASK;
		}

		// IEROM
		if ((Port71_EROM & 1) != 0) {
			pattern |= IEROM_BIT_MASK;
		}

		// EROMSL
		pattern |= ((Port32_EROMSL) << EROMSL_BIT_SHIFT);

		// GVRAM
		switch (gvram_sel) {
		case 1:
			pattern |= (1 << GVRAM_BIT_SHIFT);
			break;
		case 2:
			pattern |= (2 << GVRAM_BIT_SHIFT);
			break;
		case 4:
			pattern |= (3 << GVRAM_BIT_SHIFT);
			break;
		case 8:
			// for memory wait
			pattern |= (1 << GVRAM_BIT_SHIFT);
			break;
		}

		// REWE
		if (read == true) {
			if (PortE2_RDEN) {
				if (PortE3_ERAMSL < PC88_EXRAM_BANKS) {
					pattern |= REWE_BIT_MASK;
					pattern |= (PortE3_ERAMSL << BS_BIT_SHIFT);
				}
				else {
					// if over EXRAM_BANKS, main RAM should be assigned
					pattern |= MMODE_BIT_MASK;
				}
			}
		}
		else {
			if (PortE2_WREN) {
				if (PortE3_ERAMSL < PC88_EXRAM_BANKS) {
					pattern |= REWE_BIT_MASK;
					pattern |= (PortE3_ERAMSL << BS_BIT_SHIFT);
				}
			}
		}

		// TVRAM
		if ((config.boot_mode == MODE_PC88_V1S) || (config.boot_mode == MODE_PC88_N)) {
			// always main RAM
			pattern |= TMODE_BIT_MASK;
		}
		else {
			// V1H or V2
			if (Port32_TMODE) {
				pattern |= TMODE_BIT_MASK;
			}
		}

		// save pattern
		if (read == true) {
			read_pattern = pattern;
		}
		else {
			write_pattern = pattern;
		}
	}

	// set ptr
	if (read == true) {
		read_bank_ptr = &read_banks[0x40 * (pattern & ~(GVAMTDIC_BIT_MASK | GHSM_BIT_MASK | GRPHE_BIT_MASK | VRTC_BIT_MASK))];
		read_wait_ptr = &read_waits[0x40 * pattern];
		m1_wait_ptr = &m1_waits[0x40 * (pattern & ~(GVAMTDIC_BIT_MASK | GHSM_BIT_MASK | GRPHE_BIT_MASK | VRTC_BIT_MASK))];
	}
	else {
		write_bank_ptr = &write_banks[0x40 * (pattern & ~(GVAMTDIC_BIT_MASK | GHSM_BIT_MASK | GRPHE_BIT_MASK | VRTC_BIT_MASK))];
		write_wait_ptr = &write_waits[0x40 * pattern];
	}
}

void PC88::update_gvram_sel()
{
	if(Port32_GVAM) {
		if(Port35_GAM) {
			// ALU access
			gvram_sel = 8;
		} else {
			// main RAM access
			gvram_sel = 0;
		}
		gvram_plane = 0; // from M88
	} else {
		gvram_sel = gvram_plane;
	}

	if ((gvram_sel != 8) && (PortF1_DICROM == 0)) {
		read_pattern &= ~GVAMTDIC_BIT_MASK;
		write_pattern &= ~GVAMTDIC_BIT_MASK;
	}

	// select GVRAM or main RAM
	switch (gvram_sel) {
	// main RAM
	case 0:
		read_pattern &= ~GVRAM_BIT_MASK;
		write_pattern &= ~GVRAM_BIT_MASK;
		break;

	// blue
	case 1:
		read_pattern &= ~GVRAM_BIT_MASK;
		read_pattern |= (1 << GVRAM_BIT_SHIFT);
		write_pattern &= ~GVRAM_BIT_MASK;
		write_pattern |= (1 << GVRAM_BIT_SHIFT);
		break;

	// red
	case 2:
		read_pattern &= ~GVRAM_BIT_MASK;
		read_pattern |= (2 << GVRAM_BIT_SHIFT);
		write_pattern &= ~GVRAM_BIT_MASK;
		write_pattern |= (2 << GVRAM_BIT_SHIFT);
		break;

	// green
	case 4:
		read_pattern &= ~GVRAM_BIT_MASK;
		read_pattern |= (3 << GVRAM_BIT_SHIFT);
		write_pattern &= ~GVRAM_BIT_MASK;
		write_pattern |= (3 << GVRAM_BIT_SHIFT);
		break;

	// ALU
	case 8:
		read_pattern |= GVAMTDIC_BIT_MASK;
		write_pattern |= GVAMTDIC_BIT_MASK;

		// select blue plane for wait
		read_pattern &= ~GVRAM_BIT_MASK;
		read_pattern |= (1 << GVRAM_BIT_SHIFT);
		write_pattern &= ~GVRAM_BIT_MASK;
		write_pattern |= (1 << GVRAM_BIT_SHIFT);
		break;
	}

	// update memory map
	update_memmap(read_pattern, true);
	update_memmap(write_pattern, false);
}

uint32 PC88::port30_in()
{
	uint32 val;

	// initialize
	val = 0xc0;

	// bit5:PDEL
	if ((config.dipswitch & XM8_DIP_DISABLEDEL) != 0) {
		val |= 0x20;
	}

	// bit4:SPRM
	if ((config.dipswitch & XM8_DIP_ENABLES) == 0) {
		val |= 0x10;
	}

	// bit3:LN25
	if ((config.dipswitch & XM8_DIP_LINE25) == 0) {
		val |= 0x08;
	}

	// bit2:CH80
	if ((config.dipswitch & XM8_DIP_WIDTH40) != 0) {
		val |= 0x04;
	}

	// bit1:TERM
	if ((config.dipswitch & XM8_DIP_TERMMODE) == 0) {
		val |= 0x02;
	}

	// bit0:SW4S1
	if (config.boot_mode != MODE_PC88_N) {
		val |= 0x01;
	}

	return val;
}

void PC88::port31_out(uint8 mod)
{
	bool update;
	update = false;

	if (mod & 0x04) {
		// RMODE
		if (Port31_RMODE) {
			read_pattern |= RMODE_BIT_MASK;
			write_pattern |= RMODE_BIT_MASK;
		}
		else {
			read_pattern &= ~RMODE_BIT_MASK;
			write_pattern &= ~RMODE_BIT_MASK;
		}

		// need to update memory map
		update = true;
	}

	if (mod & 0x02) {
		// MMODE
		if (Port31_MMODE) {
			read_pattern |= MMODE_BIT_MASK;
			write_pattern |= MMODE_BIT_MASK;
		}
		else {
			// see port e2
			if (PortE2_RDEN != 0) {
				if (PortE3_ERAMSL >= PC88_EXRAM_BANKS) {
					read_pattern &= ~MMODE_BIT_MASK;
				}
			}
			else {
				read_pattern &= ~MMODE_BIT_MASK;
			}
			write_pattern &= ~MMODE_BIT_MASK;
		}

		// need to update memory map
		update = true;
	}

	if (mod & 0x08) {
		// GRPHE
		if (Port31_GRAPH) {
			read_pattern |= GRPHE_BIT_MASK;
			write_pattern |= GRPHE_BIT_MASK;
		}
		else {
			read_pattern &= ~GRPHE_BIT_MASK;
			write_pattern &= ~GRPHE_BIT_MASK;
		}

		// need to update memory map (wait only)
		update = true;

		// ePC-8801MA 2017.11.26
		update_palette = true;
	}

	if (update == true) {
		// update memory map
		update_memmap(read_pattern, true);
		update_memmap(write_pattern, false);
	}


	// 20line/25line, 400line/200line
	if(mod & 0x21) {
		update_timing();
		update_palette = true;
	}

	// mono/color
	if (mod & 0x10) {
		update_palette = true;
	}
}

uint32 PC88::port31_in()
{
	uint32 val;
	uint32 parity;

	// initialize
	switch (config.boot_mode) {
		case MODE_PC88_V2:
			val = 0x40;
			break;

		case MODE_PC88_V1H:
			val = 0xc0;
			break;

		// V1S or N
		default:
			val = 0x80;
			break;
	}

	// bit5:HDPX
	if ((config.dipswitch & XM8_DIP_HALFDUPLEX) == 0) {
		val |= 0x20;
	}

	// bit4:XPRM
	if ((config.dipswitch & XM8_DIP_DISABLEX) != 0) {
		val |= 0x10;
	}

	// bit3:ST2B
	if ((config.dipswitch & XM8_DIP_STOP2BIT) == 0) {
		val |= 0x08;
	}

	// bit2:DT8B
	if ((config.dipswitch & XM8_DIP_DATA7BIT) != 0) {
		val |= 0x04;
	}

	// bit1:EVPTY
	// bit0:ENPTY
	parity = (config.dipswitch & XM8_DIP_PARITY) >> XM8_DIP_PARITY_SHIFT;
	switch (parity) {
	// no parity
	case 0:
		val |= 0x01;
		break;
	// even parity
	case 1:
		val |= 0x00;
		break;
	// odd parity
	default:
		val |= 0x02;
		break;
	}

	return val;
}

void PC88::port32_out(uint8 mod)
{
	bool update;

	// initialize
	update = false;

	if ((mod & 0x03) != 0) {
		// EROMSL
		read_pattern &= ~EROMSL_BIT_MASK;
		read_pattern |= (Port32_EROMSL << EROMSL_BIT_SHIFT);
		write_pattern &= ~EROMSL_BIT_MASK;
		write_pattern |= (Port32_EROMSL << EROMSL_BIT_SHIFT);

		// need to update memory map
		update = true;
	}

	if ((mod & 0x10) != 0) {
		// TMODE
		if ((config.boot_mode == MODE_PC88_V1S) || (config.boot_mode == MODE_PC88_N)) {
			// V1S or N (always main RAM)
			read_pattern |= TMODE_BIT_MASK;
			write_pattern |= TMODE_BIT_MASK;
		}
		else {
			// V1H or V2
			if (Port32_TMODE) {
				read_pattern |= TMODE_BIT_MASK;
				write_pattern |= TMODE_BIT_MASK;
			}
			else {
				read_pattern &= ~TMODE_BIT_MASK;
				write_pattern &= ~TMODE_BIT_MASK;
			}
		}

		// need to update memory map
		update = true;
	}

	if((mod & 0x40) != 0) {
		// GVAM
		update_gvram_sel();
	}

	if((mod & 0x80) != 0) {
		// SINTM
		if ((intr_req_sound == true) && (Port32_SINTM == 0)) {
			// request sound interrupt again
			request_intr(IRQ_SOUND, true);
		}
	}

	// update memory map
	if (update == true) {
		update_memmap(read_pattern, true);
		update_memmap(write_pattern, false);
	}

	// analog/digital
	if (mod & 0x20) {
		update_palette = true;
	}
}

void PC88::port35_out(uint8 mod)
{
	if((mod & 0x80) != 0) {
		update_gvram_sel();
	}
}

uint32 PC88::port40_in()
{
	uint32 val;

	// initialize
	val = 0xc0;

	// bit5:VRTC
	if (crtc.vblank == true) {
		val |= 0x20;
	}

	// bit4:CDI
	if (d_rtc->read_signal(0) != 0) {
		val |= 0x10;
	}

	// bit3:BOOT
	if ((config.dipswitch & XM8_DIP_BOOTROM) != 0) {
		val |= 0x08;
	}

	// bit2:DCD
	if (usart_dcd == true) {
		val |= 0x04;
	}

	// bit1:SHG
	if (hireso == false) {
		val |= 0x02;
	}

	// bit0:BUSY
	val |= 0x01;

	return val;
}

void PC88::port40_out(uint32 data, uint8 mod)
{
		emu->printer_strobe((data & 1) == 0);
		d_rtc->write_signal(SIG_UPD1990A_STB, ~data, 2);
		d_rtc->write_signal(SIG_UPD1990A_CLK, data, 4);

		// bit3: crtc i/f sync mode
		if ((mod & 0x10) != 0) {
			if (Port40_GHSM) {
				read_pattern |= GHSM_BIT_MASK;
				write_pattern |= GHSM_BIT_MASK;
			}
			else {
				read_pattern &= ~GHSM_BIT_MASK;
				write_pattern &= ~GHSM_BIT_MASK;
			}
			update_memmap(read_pattern, false);
			update_memmap(write_pattern, true);
		}

		beep_on = ((data & 0x20) != 0);
		sing_signal = ((data & 0x80) != 0);

#ifdef SDL
		if (beep_on == true) {
			if (beep_event_id < 0) {
				register_event(this, EVENT_BEEP, 1000000.0 / 4800.0, true, &beep_event_id);
				beep_signal = true;
			}
		}
#endif // SDL

		d_pcm->write_signal(SIG_PCM1BIT_SIGNAL, ((beep_on && beep_signal) || sing_signal) ? 1 : 0, 1);
}

void PC88::port44_out(uint32 addr, uint32 data)
{
	switch (addr & 3) {
	// OPN/OPNA register
	case 0:
		d_opn->write_io8(0, data);
		break;

	// OPN/OPNA data
	case 1:
		d_opn->write_io8(1, data);
		break;

#ifdef SUPPORT_PC88_OPNA
	// OPNA extended register
	case 2:
		if (d_opn->is_ym2608() == true) {
			d_opn->write_io8(2, data);
		}
		break;

	// OPN extended data
	case 3:
		if (d_opn->is_ym2608() == true) {
			d_opn->write_io8(3, data);
		}
		break;
#endif // SUPPORT_PC88_OPNA
	}
}

uint32 PC88::port44_in(uint32 addr)
{
	switch (addr & 3) {
	// OPN/OPNA status
	case 0:
		return d_opn->read_io8(0);

	// OPN/OPNA data
	case 1:
#ifdef SUPPORT_PC88_JOYSTICK
		switch (Port44_OPNCH) {
		// joystick data 1
		case 14:
			return (~(joystick_status[0] >> 0) & 0x0f) | 0xf0;

		// joystick data 2
		case 15:
			return (~(joystick_status[0] >> 4) & 0x03) | 0xfc;

		default:
			break;
		}
#endif // SUPPORT_PC88_JOYSTICK
		return d_opn->read_io8(1);

	// OPNA extended register
	case 2:
		if (d_opn->is_ym2608() == true) {
			return d_opn->read_io8(2);
		}
		break;

	// OPNA exnteded data
	case 3:
		if (d_opn->is_ym2608() == true) {
			return d_opn->read_io8(3);
		}
		break;
	}

	return 0xff;
}

void PC88::port5c_out()
{
	if (gvram_plane != 1) {
		// select GVRAM(blue)
		gvram_plane = 1;
		update_gvram_sel();
	}
}

void PC88::port5d_out()
{
	if (gvram_plane != 2) {
		// select GVRAM(red)
		gvram_plane = 2;
		update_gvram_sel();
	}
}

void PC88::port5e_out()
{
	if (gvram_plane != 4) {
		// select GVRAM(green)
		gvram_plane = 4;
		update_gvram_sel();
	}
}

void PC88::port5f_out()
{
	if (gvram_plane != 0) {
		// select main RAM
		gvram_plane = 0;
		update_gvram_sel();
	}
}

void PC88::port6f_out()
{
	uint32 baudrate;

	if (is_sr_mr() == false) {
		// can set value 0x09-0x0f (what baudrate ?)
		baudrate = port[0x6f] & 0x0f;
		xm8_ext_flags &= ~XM8_EXT_BAUDRATE;
		xm8_ext_flags |= (baudrate << XM8_EXT_BAUDRATE_SHIFT);
	}
}

uint32 PC88::port6e_in()
{
	uint32 val;

	if (is_sr_mr() == true) {
		// PC-8801mkIISR/TR/FR/MR -> return 0xff(4MHz)
		val = 0x7f;
	}
	else {
		// PC-8801FH/MH or later
		val = 0x10;
	}

	if (cpu_clock_low == true) {
		val |= 0x80;
	}

	return val;
}

uint32 PC88::port6f_in()
{
	uint32 baudrate;
	uint32 val;

	if (is_sr_mr() == true) {
		// PC-8801mkIISR/TR/FR/MR
		val = 0xff;
	}
	else {
		// PC-8801FH/MH or later
		baudrate = (xm8_ext_flags & XM8_EXT_BAUDRATE) >> XM8_EXT_BAUDRATE_SHIFT;
		val = 0xf0 | baudrate;
	}

	return val;
}

void PC88::port71_out(uint8 mod)
{
	if ((mod & 1) != 0) {
		if ((Port71_EROM & 1) != 0) {
			read_pattern |= IEROM_BIT_MASK;
			write_pattern |= IEROM_BIT_MASK;
		}
		else {
			read_pattern &= ~IEROM_BIT_MASK;
			write_pattern &= ~IEROM_BIT_MASK;
		}

		// update memory map
		update_memmap(read_pattern, true);
		update_memmap(write_pattern, false);
	}
}

void PC88::port78_out()
{
	Port70_TEXTWND++;
}

void PC88::porta8_out(uint32 addr, uint32 data)
{
#ifdef SUPPORT_PC88_OPNA
	// PC-8801mkIISR/TR/FR/MR + sound board II only
	if (d_sb2->is_enable() == false) {
		return;
	}

	switch (addr & 7) {
	// OPNA register
	case 0:
		d_sb2->write_io8(0, data);
		break;

	// OPNA data
	case 1:
		d_sb2->write_io8(1, data);
		break;

	// interrupt mask(S2INTM)
	case 2:
		if ((xm8_ext_flags & XM8_EXT_SB2_IRQ) != 0) {
			if (PortAA_S2INTM == 0) {
				request_intr(IRQ_SOUND, true);
			}
		}
		break;

	// OPN extended register
	case 4:
		d_sb2->write_io8(2, data);
		break;

	// OPN extended data
	case 5:
		d_sb2->write_io8(3, data);
		break;
	}
#endif // PC88_SUPPORT_OPNA
}

uint32 PC88::porta8_in(uint32 addr)
{
#ifdef SUPPORT_PC88_OPNA
	// PC-8801mkIISR/TR/FR/MR + sound board II only
	if (d_sb2->is_enable() == false) {
		return 0xff;
	}

	switch (addr & 7) {
	// OPNA status
	case 0:
		return d_sb2->read_io8(0);

	// OPNA data
	case 1:
#ifdef SUPPORT_PC88_JOYSTICK
		switch (PortA8_OPNCH) {
		// joystick data 1
		case 14:
			return (~(joystick_status[0] >> 0) & 0x0f) | 0xf0;

		// joystick data 2
		case 15:
			return (~(joystick_status[0] >> 4) & 0x03) | 0xfc;

		default:
			break;
		}
#endif // SUPPORT_PC88_JOYSTICK
		return d_sb2->read_io8(1);

	// interrupt mask(S2INTM)
	case 2:
		return (PortAA_S2INTM) | 0x7f;

	// OPNA extended register
	case 4:
		return d_sb2->read_io8(2);

	// OPNA exnteded data
	case 5:
		return d_sb2->read_io8(3);
		break;
	}
#endif // PC88_SUPPORT_OPNA

	// not assigned (0xab, 0xae, 0xaf)
	return 0xff;
}

void PC88::porte2_out()
{
	if ((config.dipswitch & XM8_DIP_NOEXRAM) != 0) {
		// 64K RAM only
		port[0xe2] = 0x11;
		return;
	}

	// read
	if (PortE2_RDEN != 0) {
		if (PortE3_ERAMSL < PC88_EXRAM_BANKS) {
			// EXT RAM
			read_pattern |= REWE_BIT_MASK;
		}
		else {
			// 64K RAM or ROM
			read_pattern &= ~REWE_BIT_MASK;

			// for Rune Worth (inculdes Juke Box)
			if (Port31_MMODE == 0) {
				read_pattern &= ~MMODE_BIT_MASK;
			}
			else {
				read_pattern |= MMODE_BIT_MASK;
			}
		}
	}
	else {
		read_pattern &= ~REWE_BIT_MASK;
		if (Port31_MMODE == 0) {
			read_pattern &= ~MMODE_BIT_MASK;
		}
	}
	update_memmap(read_pattern, true);

	// write
	if (PortE2_WREN != 0) {
		if (PortE3_ERAMSL < PC88_EXRAM_BANKS) {
			// EXT RAM
			write_pattern |= REWE_BIT_MASK;
		}
		else {
			write_pattern &= ~REWE_BIT_MASK;
		}
	}
	else {
		write_pattern &= ~REWE_BIT_MASK;
	}
	update_memmap(write_pattern, false);
}

void PC88::porte3_out()
{
	// support 4 bank (128KB)
	read_pattern &= ~BS_BIT_MASK;
	read_pattern |= (((PortE3_ERAMSL) & 3) << BS_BIT_SHIFT);
	write_pattern &= ~BS_BIT_MASK;
	write_pattern |= (((PortE3_ERAMSL) & 3) << BS_BIT_SHIFT);

	// common
	porte2_out();
}

void PC88::portf0_out(uint8 mod)
{
#ifdef SUPPORT_PC88_DICTIONARY
	if (((xm8_ext_flags & XM8_EXT_NO_DICROM) != 0) || ((config.dipswitch & XM8_DIP_NOEXRAM) != 0)) {
		port[0xf0] = 0xff;
	}
	else {
		if (port[0xf0] >= 0x20) {
			// no effect if data >= 0x20
			port[0xf0] ^= mod;
		}
	}
#else
	port[0xf0] = 0xff;
#endif // SUPPORT_PC88_DICTIONARY
}

uint32 PC88::portf0_in()
{
	// always 0xff
	return 0xff;
}

void PC88::portf1_out(uint8 mod)
{
#ifdef SUPPORT_PC88_DICTIONARY
	if ((xm8_ext_flags & XM8_EXT_NO_DICROM) == 1) {
		port[0xf1] = 0xff;
	}
	else {
		switch (port[0xf1]) {
		// on
		case 0x00:
			read_pattern |= GVAMTDIC_BIT_MASK;
			update_memmap(read_pattern, true);
			break;

		// off
		case 0x01:
			if (gvram_sel != 8) {
				read_pattern &= ~GVAMTDIC_BIT_MASK;
				write_pattern &= ~GVAMTDIC_BIT_MASK;
				update_memmap(read_pattern, true);
				update_memmap(write_pattern, false);
			}
			break;

		// effect only 0x00 or 0x01
		default:
			port[0xf1] ^= mod;
		}
	}
#else
	port[0xf1] = 0xff;
#endif // SUPPORT_PC88_DICTIONARY
}

uint32 PC88::portf1_in()
{
	// always 0xff
	return 0xff;
}

void PC88::portfc_out(uint32 addr, uint32 data)
{
	uint32 port_c;

	switch (addr & 3) {
	// port A
	case 0:
		d_pio->write_io8(0, data);
		break;

	// port B
	case 1:
		d_pio->write_io8(1, data);
		break;

	// port C
	case 2:
		port_c = d_pio->read_signal(SIG_I8255_PORT_C);
		d_pio->write_io8(2, data);
		if (port_c != d_pio->read_signal(SIG_I8255_PORT_C)) {
			// port C has been changed
			request_single_exec();
		}
		break;

	// control
	case 3:
		port_c = d_pio->read_signal(SIG_I8255_PORT_C);
		d_pio->write_io8(3, data);
		if (port_c != d_pio->read_signal(SIG_I8255_PORT_C)) {
			// port C has been changed
			request_single_exec();
		}
		break;
	}
}

uint32 PC88::portfc_in(uint32 addr)
{
	return d_pio->read_io8(addr & 3);
}

#endif // SDL

static const int key_table[15][8] = {
	{ 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67 },
	{ 0x68, 0x69, 0x6a, 0x6b, 0x92, 0x6c, 0x6e, 0x0d },
	{ 0xc0, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47 },
	{ 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f },
	{ 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57 },
	{ 0x58, 0x59, 0x5a, 0xdb, 0xdc, 0xdd, 0xde, 0xbd },
	{ 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37 },
	{ 0x38, 0x39, 0xba, 0xbb, 0xbc, 0xbe, 0xbf, 0xe2 },
	{ 0x24, 0x26, 0x27, 0x2e, 0x12, 0x15, 0x10, 0x11 },
	{ 0x13, 0x70, 0x71, 0x72, 0x73, 0x74, 0x20, 0x1b },
	{ 0x09, 0x28, 0x25, 0x23, 0x7b, 0x6d, 0x6f, 0x14 },
	{ 0x21, 0x22, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
#ifdef SDL
	{ 0x75, 0x76, 0x77, 0x78, 0x79, 0x08, 0x2d, 0x3b },
#else
	{ 0x75, 0x76, 0x77, 0x78, 0x79, 0x08, 0x2d, 0x2e },
#endif // SDL
	{ 0x1c, 0x1d, 0x18, 0x19, 0x00, 0x00, 0x00, 0x00 },
	{ 0x1a, 0x5e, 0xa0, 0xa1, 0x00, 0x00, 0x00, 0x00 } 
};

static const int key_conv_table[9+4][3] = {
	{0x2d, 0x2e, 1}, // INS	-> SHIFT + DEL
	{0x75, 0x70, 1}, // F6	-> SHIFT + F1
	{0x76, 0x71, 1}, // F7	-> SHIFT + F2
	{0x77, 0x72, 1}, // F8	-> SHIFT + F3
	{0x78, 0x73, 1}, // F9	-> SHIFT + F4
	{0x79, 0x74, 1}, // F10	-> SHIFT + F5
	{0x08, 0x2e, 0}, // BS	-> DEL
	{0x1c, 0x20, 0}, // •ÏŠ·-> SPACE
	{0x1d, 0x20, 0}, // Œˆ’è-> SPACE
	{0xa0, 0x10, 0}, // LSHIFT
	{0xa1, 0x10, 0}, // RSHIFT
	{0x5e, 0x0d, 0}, // RETURN(TEN)
	{0x1a, 0x0d, 0}  // RETURN(JIS)
};

static const uint8 intr_mask2_table[8] = {
	~7, ~3, ~5, ~1, ~6, ~2, ~4, ~0
};

#ifdef SDL

uint32 PC88::get_key_code(uint32 port, uint32 bit)
{
	if ((port < 15) && (bit < 8)) {
		return key_table[port][bit];
	}

	return 0;
}

#endif // SDL

void PC88::initialize()
{
	memset(rdmy, 0xff, sizeof(rdmy));
#ifdef PC88_EXRAM_BANKS
	memset(exram, 0, sizeof(exram));
#endif
	memset(gvram, 0, sizeof(gvram));
	memset(gvram_null, 0, sizeof(gvram_null));
	memset(tvram, 0, sizeof(tvram));
	memset(n88rom, 0xff, sizeof(n88rom));
	memset(n88exrom, 0xff, sizeof(n88exrom));
	memset(n80rom, 0xff, sizeof(n80rom));
	memset(kanji1, 0xff, sizeof(kanji1));
	memset(kanji2, 0xff, sizeof(kanji2));
#ifdef SUPPORT_PC88_DICTIONARY
	memset(dicrom, 0xff, sizeof(dicrom));
#endif
	
	// load rom images
	FILEIO* fio = new FILEIO();
	if(fio->Fopen(emu->bios_path(_T("PC88.ROM")), FILEIO_READ_BINARY)) {
		fio->Fread(n88rom, 0x8000, 1);
		fio->Fread(n80rom + 0x6000, 0x2000, 1);
		fio->Fseek(0x2000, FILEIO_SEEK_CUR);
		fio->Fread(n88exrom, 0x8000, 1);
		fio->Fseek(0x2000, FILEIO_SEEK_CUR);
		fio->Fread(n80rom, 0x6000, 1);
		fio->Fclose();
	}
	if(fio->Fopen(emu->bios_path(_T("N88.ROM")), FILEIO_READ_BINARY)) {
		fio->Fread(n88rom, 0x8000, 1);
		fio->Fclose();
	}
	if(fio->Fopen(emu->bios_path(_T("N88_0.ROM")), FILEIO_READ_BINARY)) {
		fio->Fread(n88exrom + 0x0000, 0x2000, 1);
		fio->Fclose();
	}
	if(fio->Fopen(emu->bios_path(_T("N88_1.ROM")), FILEIO_READ_BINARY)) {
		fio->Fread(n88exrom + 0x2000, 0x2000, 1);
		fio->Fclose();
	}
	if(fio->Fopen(emu->bios_path(_T("N88_2.ROM")), FILEIO_READ_BINARY)) {
		fio->Fread(n88exrom + 0x4000, 0x2000, 1);
		fio->Fclose();
	}
	if(fio->Fopen(emu->bios_path(_T("N88_3.ROM")), FILEIO_READ_BINARY)) {
		fio->Fread(n88exrom + 0x6000, 0x2000, 1);
		fio->Fclose();
	}
	if(fio->Fopen(emu->bios_path(_T("N80.ROM")), FILEIO_READ_BINARY)) {
		fio->Fread(n80rom, 0x8000, 1);
		fio->Fclose();
	}
	if(fio->Fopen(emu->bios_path(_T("KANJI1.ROM")), FILEIO_READ_BINARY)) {
		fio->Fread(kanji1, 0x20000, 1);
		fio->Fclose();
	}
	if(fio->Fopen(emu->bios_path(_T("KANJI2.ROM")), FILEIO_READ_BINARY)) {
		fio->Fread(kanji2, 0x20000, 1);
		fio->Fclose();
	}
#ifdef SUPPORT_PC88_DICTIONARY
#ifdef SDL
	xm8_ext_flags = XM8_EXT_NO_DICROM;
#endif // SDL
	if(fio->Fopen(emu->bios_path(_T("JISYO.ROM")), FILEIO_READ_BINARY)) {
		fio->Fread(dicrom, 0x80000, 1);
		fio->Fclose();
#ifdef SDL
		xm8_ext_flags &= ~XM8_EXT_NO_DICROM;
#endif // SDL
	}
#endif
	delete fio;
	
	// memory pattern
	for(int i = 0, ofs = 0; i < 256; i++) {
		for(int j = 0; j < 16; j++) {
			static const uint8 p0[256] = {
				0,1,0,1,0,1,0,0,0,0,0,0,1,0,1,0, // 0000
				0,1,0,1,0,1,0,0,0,0,1,0,1,0,1,0, // 1000
				0,1,0,1,0,0,0,0,0,0,1,0,1,0,1,0, // 2000
				0,1,0,1,0,0,0,0,0,0,1,0,1,0,1,0, // 3000
				1,0,1,0,1,0,1,1,1,1,1,1,0,1,0,1, // 4000
				1,0,1,0,1,0,1,1,1,1,0,1,0,1,0,1, // 5000
				1,0,1,0,1,1,1,1,1,1,0,1,0,1,0,1, // 6000
				1,0,1,0,1,1,1,1,1,1,0,1,0,1,0,1, // 7000
				1,0,1,0,1,0,1,1,1,1,1,1,0,1,0,1, // 8000
				1,0,1,0,1,0,1,1,1,1,1,1,0,1,0,1, // 9000
				1,0,1,0,1,0,1,1,1,1,0,1,0,1,0,1, // a000
				1,0,1,0,1,1,1,1,1,1,0,1,0,1,0,1, // b000
				0,1,0,1,0,1,0,0,0,0,0,0,1,0,1,0, // c000
				0,1,0,1,0,1,0,0,0,0,0,0,1,0,1,0, // d000
				0,1,0,1,0,1,0,0,0,0,1,0,1,0,1,0, // e000
				0,1,0,1,0,0,0,0,0,0,1,0,1,0,1,0, // f000
			};
			static const uint8 p1[16] = {
				0x00,0xff,0x00,0xff,0xff,0x00,0xff,0x00,0x00,0xff,0x00,0xff,0xff,0x00,0xff,0x00,
			};
			memset(ram + ofs, (p0[i] == 0) ? p1[j] : ~p1[j], 16);
			ofs += 16;
		}
	}
	
	// create semi graphics pattern
	for(int i = 0; i < 256; i++) {
		uint8 *dest = sg_pattern + 8 * i;
		dest[0] = dest[1] = ((i & 1) ? 0xf0 : 0) | ((i & 0x10) ? 0x0f : 0);
		dest[2] = dest[3] = ((i & 2) ? 0xf0 : 0) | ((i & 0x20) ? 0x0f : 0);
		dest[4] = dest[5] = ((i & 4) ? 0xf0 : 0) | ((i & 0x40) ? 0x0f : 0);
		dest[6] = dest[7] = ((i & 8) ? 0xf0 : 0) | ((i & 0x80) ? 0x0f : 0);
	}
	
	// initialize text/graph palette
	for (int i = 0; i < 8; i++) {
		palette_text_pc[i] = RGB_COLOR((i & 2) ? 255 : 0, (i & 4) ? 255 : 0, (i & 1) ? 255 : 0); // A is a flag for crt filter
	}
	for(int i = 0; i < 8; i++) {
		palette_graph_pc[i] = RGB_COLOR((i & 2) ? 255 : 0, (i & 4) ? 255 : 0, (i & 1) ? 255 : 0);
	}
	palette_text_pc[8] = palette_graph_pc[8] = 0;

#ifdef SUPPORT_PC88_HIGH_CLOCK
	cpu_clock_low = (config.cpu_type != 0);
#else
	cpu_clock_low = true;
#endif
	
#ifdef SUPPORT_PC88_JOYSTICK
	joystick_status = emu->joy_buffer();
	mouse_status = emu->mouse_buffer();
	mouse_strobe_clock_lim = (int)((cpu_clock_low ? 720 : 1440) * 1.25);
#endif
	
	// initialize cmt
	cmt_fio = new FILEIO();
	cmt_play = cmt_rec = false;
	
	register_frame_event(this);
	register_vline_event(this);
	register_event(this, EVENT_TIMER, 1000000.0 / 600.0, true, NULL);
#ifdef SDL
	beep_event_id = -1;

	// version 1.20
	for (int pattern=0; pattern<0x8000; pattern++) {
		create_pattern(pattern, true);
		create_pattern(pattern, false);
	}
	if (cpu_clock_low == true) {
		// 4MHz
		gvram_access_limit[0] = 0x1b00;
		gvram_access_limit[1] = 0x1b00;
	}
	else {
		// 8MHz
		gvram_access_limit[0] = 0x2b0;
		gvram_access_limit[1] = 0x29c;
	}
#else
	register_event(this, EVENT_BEEP, 1000000.0 / 4800.0, true, NULL);
#endif // !SDL
}

void PC88::release()
{
	release_tape();
	delete cmt_fio;
}

void PC88::reset()
{
	hireso = (config.monitor_type == 0);
	
	// memory
	memset(port, 0, sizeof(port));
	port[0x31] = 0x01;
	port[0x32] = 0x98;
	for(int i = 0; i < 8; i++) {
		port[0x54 + i] = i;
	}
	port[0x70] = 0x80;
	port[0x71] = port[0xf1] = 0xff;
	
	memset(alu_reg, 0, sizeof(alu_reg));
	gvram_plane = gvram_sel = 0;
	

#ifdef SDL
	// version 1.10
	port[0x70] = 0x00;

	// version 1.20
	update_memmap(-1, true);
	update_memmap(-1, false);
#else
	SET_BANK(0x0000, 0x7fff, ram, n88rom);
	SET_BANK(0x8000, 0xffff, ram + 0x8000, ram + 0x8000);
#endif // SDL
	
	// misc
	usart_dcd = false;
	opn_busy = true;
	
	// memory wait
	mem_wait_on = ((config.dipswitch & 1) != 0);
#ifndef SDL
	update_mem_wait();
	tvram_wait_clocks_r = tvram_wait_clocks_w = 0;
#endif // !SDL

	// crtc
	memset(&crtc, 0, sizeof(crtc));
	crtc.reset(hireso);
	update_timing();
	
	for(int i = 0; i < 9; i++) {
		palette[i].b = (i & 1) ? 7 : 0;
		palette[i].r = (i & 2) ? 7 : 0;
		palette[i].g = (i & 4) ? 7 : 0;
	}
	update_palette = true;
	
	// dma
	memset(&dmac, 0, sizeof(dmac));
	dmac.mem = dmac.ch[2].io = this;
	dmac.ch[0].io = dmac.ch[1].io = dmac.ch[3].io = vm->dummy;
	
	// keyboard
	key_kana = key_caps = 0;
	
	// mouse
#ifdef SUPPORT_PC88_JOYSTICK
	mouse_strobe_clock = current_clock();
	mouse_phase = -1;
	mouse_dx = mouse_dy = mouse_lx = mouse_ly = 0;
#endif
	
	// interrupt
	intr_req = intr_mask1 = intr_mask2 = 0;
	intr_req_sound = false;
	
	// fdd i/f
	d_pio->write_io8(1, 0);
	d_pio->write_io8(2, 0);
	
	// data recorder
	close_tape();
	cmt_play = cmt_rec = false;
	cmt_register_id = -1;
	
	// beep/sing
	beep_on = beep_signal = sing_signal = false;
	
#ifdef SUPPORT_PC88_PCG8100
	// pcg
	memcpy(pcg_pattern, kanji1 + 0x1000, sizeof(pcg_pattern));
	write_io8(1, 0);
	write_io8(2, 0);
	write_io8(3, 0);
#endif
#ifdef NIPPY_PATCH
	// dirty patch for NIPPY
	nippy_patch = false;
#endif
#ifdef SDL
	if (beep_event_id >= 0) {
		cancel_event(this, beep_event_id);
		beep_event_id = -1;
	}
	xm8_ext_flags &= 0x0000ffff;

	// version 1.10
	dmac.ch[0].addr.b.l = 0x56;
	dmac.ch[0].addr.b.h = 0x56;
	dmac.ch[1].addr.b.l = 0x7a;
	dmac.ch[1].addr.b.h = 0x7a;

	// version 1.20
	usart_dcd = true;
	if ((config.dipswitch & XM8_DIP_BAUDRATE) == 0) {
		// 1200bps
		xm8_ext_flags |= 4 << XM8_EXT_BAUDRATE_SHIFT;
	}
	else {
		// 75bps-19200bps
		xm8_ext_flags |= (((config.dipswitch & XM8_DIP_BAUDRATE) >> XM8_DIP_BAUDRATE_SHIFT) - 7) << XM8_EXT_BAUDRATE_SHIFT;
	}
	gvram_access_count = 0;
#endif // SDL
}

void PC88::insert_gvram_wait(int index, int *wait)
{
	if ((read_pattern & GRPHE_BIT_MASK) != 0) {
		// graphic on
		gvram_access_count += 0x100;
		if (gvram_access_count >= gvram_access_limit[index]) {
			gvram_access_count -= gvram_access_limit[index];
			// add +1 wait
			*wait += 1;
		}
	}
}

void PC88::write_data8w(uint32 addr, uint32 data, int* wait)
{
	uint32 bank;
	uint16 addr16;

	bank = addr >> 10;

	// GVAM or TEXTWND or DICROM
	if ((write_wait_ptr[bank] & GVAMTDIC_BIT_MASK) != 0) {
		if (addr < 0xc000) {
			// text window
			addr16 = (uint16)Port70_TEXTWND;
			addr16 = (addr16 << 8) + (addr & 0x03ff);
			*wait = write_wait_ptr[bank] & ~GVAMTDIC_BIT_MASK;
			ram[addr16] = (uint8)data;
			return;
		}

		// GVAM
		*wait = write_wait_ptr[bank] & ~GVAMTDIC_BIT_MASK;

		// GVRAM access wait
		insert_gvram_wait(1, wait);
		addr &= 0x3fff;

		switch(Port35_GDM) {
		// write ALU out
		case 0x00:
			for(int i = 0; i < 3; i++) {
				switch((Port34_ALU >> i) & 0x11) {
				case 0x00:	// reset
					gvram[addr | (0x4000 * i)] &= ~data;
					break;
				case 0x01:	// set
					gvram[addr | (0x4000 * i)] |= data;
					break;
				case 0x10:	// reverse
					gvram[addr | (0x4000 * i)] ^= data;
					break;
				}
			}
			break;
		// write ALU register
		case 0x10:
			gvram[addr | 0x0000] = alu_reg[0];
			gvram[addr | 0x4000] = alu_reg[1];
			gvram[addr | 0x8000] = alu_reg[2];
			break;
		// GVRAM1 -> GVRAM0
		case 0x20:
			gvram[addr | 0x0000] = alu_reg[1];
			break;
		// GVRAM0 -> GVRAM1
		case 0x30:
			gvram[addr | 0x4000] = alu_reg[0];
			break;
		}
		return;
	}

	*wait = write_wait_ptr[bank];

	if (((write_pattern & GVRAM_BIT_MASK) != 0) && (addr >= 0xc000)) {
		// GVRAM access wait
		insert_gvram_wait(1, wait);
	}

	write_bank_ptr[bank][addr & 0x3ff] = (uint8)data;
}

uint32 PC88::read_data8w(uint32 addr, int* wait)
{
	uint32 bank;
	uint16 addr16;
	uint8 b;
	uint8 r;
	uint8 g;

	bank = addr >> 10;

	// GVAM or TEXTWND or DICROM
	if ((read_wait_ptr[bank] & GVAMTDIC_BIT_MASK) != 0) {
		if (addr < 0xc000) {
			// text window
			addr16 = (uint16)Port70_TEXTWND;
			addr16 = (addr16 << 8) + (addr & 0x03ff);
			*wait = read_wait_ptr[bank] & ~GVAMTDIC_BIT_MASK;
			return ram[addr16];
		}

#ifdef SUPPORT_PC88_DICTIONARY
		if (PortF1_DICROM != 0) {
			// DICROM
			if (cpu_clock_low == true) {
				// 4MHz
				if (mem_wait_on == true) {
					*wait = 1;
				}
				else {
					*wait = 0;
				}
			}
			else {
				// 8MHz
				*wait = 2;
			}
			return dicrom[(addr & 0x3fff) | (0x4000 * PortF0_DICROMSL)];
		}
#endif // SUPPORT_PC88_DICTIONARY

		// GVAM
		if (gvram_sel == 8) {
			*wait = read_wait_ptr[bank] & ~GVAMTDIC_BIT_MASK;

			// GVRAM access wait
			insert_gvram_wait(0, wait);
			addr &= 0x3fff;

			alu_reg[0] = gvram[addr | 0x0000];
			alu_reg[1] = gvram[addr | 0x4000];
			alu_reg[2] = gvram[addr | 0x8000];

			b = alu_reg[0]; if(!Port35_PLN0) b ^= 0xff;
			r = alu_reg[1]; if(!Port35_PLN1) r ^= 0xff;
			g = alu_reg[2]; if(!Port35_PLN2) g ^= 0xff;

			return b & r & g;
		}
	}

	*wait = read_wait_ptr[bank];

	if (((read_pattern & GVRAM_BIT_MASK) != 0) && (addr >= 0xc000)) {
		// GVRAM access wait
		insert_gvram_wait(0, wait);
	}

	return read_bank_ptr[bank][addr & 0x3ff];
}

uint32 PC88::fetch_op(uint32 addr, int *wait)
{
	uint32 data;

	// read memory
	data = read_data8w(addr, wait);

	// add M1 wait cycles
	*wait += m1_wait_ptr[addr >> 10];

	return data;
}

void PC88::write_io8(uint32 addr, uint32 data)
{
	addr &= 0xff;
#ifdef _IO_DEBUG_LOG
	emu->out_debug_log(_T("%6x\tOUT8\t%02x,%02x\n"), d_cpu->get_pc(), addr, data);
#endif
#ifdef NIPPY_PATCH
	// dirty patch for NIPPY
	if(addr == 0x31 && data == 0x3f && d_cpu->get_pc() == 0xaa4f && nippy_patch) {
		data = 0x39; // select n88rom
	}
	// poke &haa4e, &h39
#endif
	uint8 mod = port[addr] ^ data;
	port[addr] = data;
	
	switch(addr) {
	case 0x00:
#ifdef SUPPORT_PC88_PCG8100
		pcg_data = data;
#endif
		// load tape image ??? (from QUASI88)
		if(cmt_play) {
			while(cmt_buffer[cmt_bufptr++] != 0x3a) {
				if(!(cmt_bufptr <= cmt_bufcnt)) return;
			}
			int val, sum, ptr, len, wait;
			sum = (val = cmt_buffer[cmt_bufptr++]);
			ptr = val << 8;
			sum += (val = cmt_buffer[cmt_bufptr++]);
			ptr |= val;
			sum += (val = cmt_buffer[cmt_bufptr++]);
			if((sum & 0xff) != 0) return;
			
			while(1) {
				while(cmt_buffer[cmt_bufptr++] != 0x3a) {
					if(!(cmt_bufptr <= cmt_bufcnt)) return;
				}
				sum = (len = cmt_buffer[cmt_bufptr++]);
				if(len == 0) break;
				for(; len; len--) {
					sum += (val = cmt_buffer[cmt_bufptr++]);
					write_data8w(ptr++, val, &wait);
				}
				sum += cmt_buffer[cmt_bufptr++];
				if((sum & 0xff) != 0) return;
			}
		}
		break;
#ifdef SUPPORT_PC88_PCG8100
	case 0x01:
		pcg_addr = (pcg_addr & 0x300) | data;
		break;
	case 0x02:
		if((pcg_ctrl & 0x10) && !(data & 0x10)) {
			if(pcg_ctrl & 0x20) {
				pcg_pattern[0x400 | pcg_addr] = kanji1[0x1400 | pcg_addr];
			} else {
				pcg_pattern[0x400 | pcg_addr] = pcg_data;
			}
		}
		pcg_addr = (pcg_addr & 0x0ff) | ((data & 3) << 8);
		pcg_ctrl = data;
		d_pcg_pcm0->write_signal(SIG_PCM1BIT_ON, data, 0x08);
		d_pcg_pcm1->write_signal(SIG_PCM1BIT_ON, data, 0x40);
		d_pcg_pcm2->write_signal(SIG_PCM1BIT_ON, data, 0x80);
		break;
	case 0x0c:
	case 0x0d:
	case 0x0e:
	case 0x0f:
		d_pcg_pit->write_io8(addr & 3, data);
#endif
		break;
	case 0x10:
		emu->printer_out(data);
		d_rtc->write_signal(SIG_UPD1990A_C0, data, 1);
		d_rtc->write_signal(SIG_UPD1990A_C1, data, 2);
		d_rtc->write_signal(SIG_UPD1990A_C2, data, 4);
		d_rtc->write_signal(SIG_UPD1990A_DIN, data, 8);
		break;
	case 0x20:
	case 0x21:
		d_sio->write_io8(addr, data);
		break;
	case 0x30:
		if(mod & 0x08) {
			if(Port30_MTON) {
				// start motor
				if(cmt_play && cmt_bufptr < cmt_bufcnt) {
#if 0
					// skip to the top of next block
					int tmp = cmt_bufptr;
					while(cmt_bufptr < cmt_bufcnt) {
						if(check_data_carrier()) {
							break;
						}
						cmt_bufptr++;
					}
					if(cmt_bufptr == cmt_bufcnt) {
						cmt_bufptr = tmp;
					}
#endif
					if(cmt_register_id != -1) {
						cancel_event(this, cmt_register_id);
					}
					register_event(this, EVENT_CMT_SEND, 5000, false, &cmt_register_id);
				}
			} else {
				// stop motor
				if(cmt_register_id != -1) {
					cancel_event(this, cmt_register_id);
					cmt_register_id = -1;
				}
				usart_dcd = true; // for Jackie Chan no Spartan X
			}
		}
		break;

	case 0x31:
		port31_out(mod);
		break;

	case 0x32:
		port32_out(mod);
		break;

	case 0x35:
		port35_out(mod);
		break;

	case 0x40:
		port40_out(data, mod);
		break;

	// OPN/OPNA(PC-8801FH/MH or later)
	case 0x44:
	case 0x45:
	case 0x46:
	case 0x47:
		port44_out(addr, data);
		break;

	case 0x50:
		crtc.write_param(data);
		if(crtc.timing_changed) {
			update_timing();
			crtc.timing_changed = false;
		}
		break;
	case 0x51:
		crtc.write_cmd(data);
		break;
	case 0x52:
		palette[8].b = (data & 0x10) ? 7 : 0;
		palette[8].r = (data & 0x20) ? 7 : 0;
		palette[8].g = (data & 0x40) ? 7 : 0;
		update_palette = true;
		break;
	case 0x54:
	case 0x55:
	case 0x56:
	case 0x57:
	case 0x58:
	case 0x59:
	case 0x5a:
	case 0x5b:
		if(Port32_PMODE) {
			int n = (data & 0x80) ? 8 : (addr - 0x54);
			if(data & 0x40) {
				palette[n].g = data & 7;
			} else {
				palette[n].b = data & 7;
				palette[n].r = (data >> 3) & 7;
			}
		} else {
			int n = addr - 0x54;
			palette[n].b = (data & 1) ? 7 : 0;
			palette[n].r = (data & 2) ? 7 : 0;
			palette[n].g = (data & 4) ? 7 : 0;
		}
		update_palette = true;
		break;

	// gvram_plane
	case 0x5c:
		port5c_out();
		break;
	case 0x5d:
		port5d_out();
		break;
	case 0x5e:
		port5e_out();
		break;
	case 0x5f:
		port5f_out();
		break;

	case 0x60:
	case 0x61:
	case 0x62:
	case 0x63:
	case 0x64:
	case 0x65:
	case 0x66:
	case 0x67:
	case 0x68:
		dmac.write_io8(addr, data);
		break;

	case 0x6f:
		port6f_out();
		break;

	// EROM
	case 0x71:
		port71_out(mod);
		break;

	// TEXTWND
	case 0x78:
		port78_out();
		break;

	// EXRAM
	case 0xe2:
		porte2_out();
		break;
	case 0xe3:
		porte3_out();
		break;

	// PC-8801mkIISR/TR/FR/MR + sound board II
	case 0xa8:
	case 0xa9:
	case 0xaa:
	case 0xab:
	case 0xac:
	case 0xad:
	case 0xae:
	case 0xaf:
		porta8_out(addr, data);
		break;

	case 0xe4:
		intr_mask1 = ~(0xff << (data < 8 ? data : 8));
		update_intr();
		break;
	case 0xe6:
#ifdef SDL
		// for Romancia
		if (intr_mask2_table[data & 7] != intr_mask2) {
			intr_req &= (intr_mask2_table[data & 7] & intr_mask2);
		}
#endif // SDL
		intr_mask2 = intr_mask2_table[data & 7];
		intr_req &= intr_mask2;
		update_intr();
		break;

	// Dictionary ROM
	case 0xf0:
		portf0_out(mod);
		break;
	case 0xf1:
		portf1_out(mod);
		break;

	// PIO
	case 0xfc:
	case 0xfd:
	case 0xfe:
	case 0xff:
		portfc_out(addr, data);
	}
}

uint32 PC88::read_io8(uint32 addr)
#ifdef _IO_DEBUG_LOG
{
	uint32 val = read_io8_debug(addr);
	emu->out_debug_log(_T("%06x\tIN8\t%02x = %02x\n"), d_cpu->get_pc(), addr & 0xff, val);
	return val;
}

uint32 PC88::read_io8_debug(uint32 addr)
#endif
{
	uint32 val = 0xff;
	
	addr &= 0xff;
	switch(addr) {
	case 0x00:
	case 0x01:
	case 0x02:
	case 0x03:
	case 0x04:
	case 0x05:
	case 0x06:
	case 0x07:
	case 0x08:
	case 0x09:
	case 0x0a:
	case 0x0b:
	case 0x0c:
	case 0x0d:
	case 0x0e:
		for(int i = 0; i < 8; i++) {
			if(key_status[key_table[addr & 0x0f][i]]) {
				val &= ~(1 << i);
			}
		}
#ifdef SDL
		// PC-8801MA2 with Type C keyboard returns 0x80
#else
		if(addr == 0x0e) {
			val &= ~0x80; // http://www.maroon.dti.ne.jp/youkan/pc88/iomap.html
		}
#endif // SDL
		return val;
	case 0x20:
	case 0x21:
		return d_sio->read_io8(addr);

	case 0x30:
		return port30_in();

	case 0x31:
		return port31_in();

	case 0x32:
		return port[0x32];

	case 0x40:
		return port40_in();

	// OPN/OPNA(PC-8801FH/MH or later)
	case 0x44:
	case 0x45:
	case 0x46:
	case 0x47:
		return port44_in(addr);

	case 0x50:
		return crtc.read_param();
	case 0x51:
		return crtc.read_status();
	case 0x5c:
		return gvram_plane | 0xf8;
	case 0x60:
	case 0x61:
	case 0x62:
	case 0x63:
	case 0x64:
	case 0x65:
	case 0x66:
	case 0x67:
	case 0x68:
		return dmac.read_io8(addr);

	case 0x6e:
		return port6e_in();

	case 0x6f:
		return port6f_in();

	case 0x70:
		// PC-8001mkIISR returns the constant value
		// this port is used to detect PC-8001mkIISR or 8801mkIISR
		return port[0x70];
	case 0x71:
		return port[0x71];


	// PC-8801mkIISR/TR/FR/MR + sound board II
	case 0xa8:
	case 0xa9:
	case 0xaa:
	case 0xab:
	case 0xac:
	case 0xad:
	case 0xae:
	case 0xaf:
		return porta8_in(addr);

	case 0xe2:
		return (~port[0xe2]) | 0xee;
	case 0xe3:
#ifdef PC88_IODATA_EXRAM
		return port[0xe3];
#else
		return port[0xe3] | 0xf0;
#endif
	case 0xe8:
		return kanji1[PortE8E9_KANJI1 * 2 + 1];
	case 0xe9:
		return kanji1[PortE8E9_KANJI1 * 2];
	case 0xec:
		return kanji2[PortECED_KANJI2 * 2 + 1];
	case 0xed:
		return kanji2[PortECED_KANJI2 * 2];

	// Dictionary ROM
	case 0xf0:
		return portf0_in();
	case 0xf1:
		return portf1_in();

	// PIO
	case 0xfc:
	case 0xfd:
	case 0xfe:
	case 0xff:
		return portfc_in(addr);
	}

	return 0xff;
}

uint32 PC88::read_dma_data8(uint32 addr)
{
#if defined(_PC8001SR)
	return ram[addr & 0xffff];
#else
	if((addr & 0xf000) == 0xf000 && (config.boot_mode == MODE_PC88_V1H || config.boot_mode == MODE_PC88_V2)) {
		return tvram[addr & 0xfff];
	} else {
		return ram[addr & 0xffff];
	}
#endif
}

void PC88::write_dma_io8(uint32 addr, uint32 data)
{
	// to crtc
	crtc.write_buffer(data);
}

void PC88::update_timing()
{
	int lines_per_frame = (crtc.height + crtc.vretrace) * crtc.char_height;
#ifdef SDL
	// 56.4229Hz (25line) on PC-8801MA2
	double frames_per_sec = (hireso ? 24860.0 * 56.423 / 56.5 : 15980.0) / (double)lines_per_frame;
#else
	double frames_per_sec = (hireso ? 24860.0 * 56.424 / 56.5 : 15980.0) / (double)lines_per_frame;
#endif // SDL
	
	set_frames_per_sec(frames_per_sec);
	set_lines_per_frame(lines_per_frame);

#ifdef SDL
	// version 1.70 (for Xak2)
	reset_vblank_clocks();
#endif // SDL
}


void PC88::update_gvram_wait()
{
	if(Port31_GRAPH) {
#if defined(_PC8001SR)
		if((config.boot_mode == MODE_PC80_V1 || config.boot_mode == MODE_PC80_N) && !Port40_GHSM) {
#else
		if((config.boot_mode == MODE_PC88_V1S || config.boot_mode == MODE_PC88_N) && !Port40_GHSM) {
#endif
#ifdef SDL
			// from memory access test on PC-8801MA2
			static const int wait[8] = {96,1, 140,3, 232,1, 285,3};
#else
			static const int wait[8] = {96,0, 116,3, 138,0, 178,3};
#endif // SDL
			gvram_wait_clocks_r = gvram_wait_clocks_w = wait[(crtc.vblank ? 1 : 0) | (cpu_clock_low ? 0 : 2) | (hireso ? 4 : 0)];
		} else {
#ifdef SDL
			// from memory access test on PC-8801MA2
			static const int wait[4] = {2,1, 5,3};
#else
			static const int wait[4] = {2,0, 5,3};
#endif // SDL
			gvram_wait_clocks_r = gvram_wait_clocks_w = wait[(crtc.vblank ? 1 : 0) | (cpu_clock_low ? 0 : 2)];
		}
	} else {
		if(cpu_clock_low && mem_wait_on) {
			gvram_wait_clocks_r = cpu_clock_low ? 1 : 2;
			gvram_wait_clocks_w = cpu_clock_low ? 0 : 2;
		} else {
			gvram_wait_clocks_r = gvram_wait_clocks_w = cpu_clock_low ? 0 : 3;
		}
	}

}

void PC88::write_signal(int id, uint32 data, uint32 mask)
{
	switch (id) {
	// USART RxD
	case SIG_PC88_USART_IRQ:
		if ((data & mask) != 0) {
			request_intr(IRQ_USART, true);
		}
		else {
			request_intr(IRQ_USART, false);
		}
		break;

	// CMT OUT
	case SIG_PC88_USART_OUT:
		if (Port30_CMT && (cmt_rec == true) && (Port30_MTON != 0)) {
			cmt_buffer[cmt_bufptr++] = data & mask;
			if(cmt_bufptr >= CMT_BUFFER_SIZE) {
				cmt_fio->Fwrite(cmt_buffer, cmt_bufptr, 1);
				cmt_bufptr = 0;
			}
		}
		break;

	// OPN IRQ
	case SIG_PC88_SOUND_IRQ:
		if ((data & mask) != 0) {
			intr_req_sound = true;
			if (Port32_SINTM == 0) {
				request_intr(IRQ_SOUND, true);
			}
		}
		else {
			intr_req_sound = false;
		}
		break;

	// SB2 IRQ
	case SIG_PC88_SB2_IRQ:
		if ((data & mask) != 0) {
			xm8_ext_flags |= XM8_EXT_SB2_IRQ;
			if (PortAA_S2INTM == 0) {
				request_intr(IRQ_SOUND, true);
			}
		}
		else {
			xm8_ext_flags &= ~XM8_EXT_SB2_IRQ;
		}
		break;
	}
}

void PC88::event_callback(int event_id, int err)
{
	switch(event_id) {
	case EVENT_TIMER:
		request_intr(IRQ_TIMER, true);
		break;
	case EVENT_BUSREQ:
		d_cpu->write_signal(SIG_CPU_BUSREQ, 0, 0);
		break;
	case EVENT_CMT_SEND:
		// check data carrier
		if(cmt_play && cmt_bufptr < cmt_bufcnt && Port30_MTON) {
			// detect the data carrier at the top of next block
			if(check_data_carrier()) {
#ifdef SDL
				if (cmt_register_id >= 0) {
					cancel_event(this, cmt_register_id);
				}
#endif // SDL
				register_event(this, EVENT_CMT_DCD, 1000000, false, &cmt_register_id);
				usart_dcd = true;
				break;
			}
		}
		// fall through

	case EVENT_CMT_DCD:
		// send data to sio
		usart_dcd = false;
		if(cmt_play && cmt_bufptr < cmt_bufcnt && Port30_MTON) {
			d_sio->write_signal(SIG_I8251_RECV, cmt_buffer[cmt_bufptr++], 0xff);
			if(cmt_bufptr < cmt_bufcnt) {
#ifdef SDL
				if (cmt_register_id >= 0) {
					cancel_event(this, cmt_register_id);
				}
#endif // SDL

				register_event(this, EVENT_CMT_SEND, 5000, false, &cmt_register_id);
				break;
			}
		}
		usart_dcd = true; // Jackie Chan no Spartan X
#ifdef SDL
				if (cmt_register_id >= 0) {
					cancel_event(this, cmt_register_id);
				}
#endif // SDL
		cmt_register_id = -1;
		break;
	case EVENT_BEEP:
		beep_signal = !beep_signal;
		d_pcm->write_signal(SIG_PCM1BIT_SIGNAL, ((beep_on && beep_signal) || sing_signal) ? 1 : 0, 1);
		break;
	}
}

void PC88::event_frame()
{
	// update key status
	memcpy(key_status, emu->key_buffer(), sizeof(key_status));
	
#ifdef SDL
	if (key_status[0x3b]) {
		key_status[0x2e] = 1;
	}
#endif // SDL
	for(int i = 0; i < 9+4; i++) {
		// INS or F6-F10 -> SHIFT + DEL or F1-F5
		if(key_status[key_conv_table[i][0]]) {
			key_status[key_conv_table[i][1]] = 1;
			key_status[0x10] |= key_conv_table[i][2];
		}
	}
	if(key_status[0x11] && (key_status[0xbc] || key_status[0xbe])) {
		// CTRL + "," or "." -> NumPad "," or "."
		key_status[0x6c] = key_status[0xbc];
		key_status[0x6e] = key_status[0xbe];
		key_status[0x11] = key_status[0xbc] = key_status[0xbe] = 0;
	}
	key_status[0x14] = key_caps;
	key_status[0x15] = key_kana;
	
	crtc.update_blink();
	
#ifdef SUPPORT_PC88_JOYSTICK
	mouse_dx += mouse_status[0];
	mouse_dy += mouse_status[1];
#endif
}

int PC88::event_vline(int v)
{
	int disp_line;
	int next_line;

	// initialize
	disp_line = crtc.height * crtc.char_height;
	next_line = 1;

	if (v == 0) {
		// V-DISP
		if(crtc.status & 0x10) {
			// start dma transfer to crtc
			dmac.start(2);

			// DMAC error check
			if (dmac.ch[2].running == false) {
				// DMA underrun
				crtc.status |= 8;
			} else {
				// clear underrun on each frame
				crtc.status &= ~8;
			}

			// dma wait cycles
			busreq_clocks = (int)((double)(dmac.ch[2].count.sd + 1) * (cpu_clock_low ? 5.95 : 10.58) / (double)disp_line + 0.5);
		}

		// start crtc
		crtc.start();

		// update wait pattern
		read_pattern &= ~VRTC_BIT_MASK;
		write_pattern &= ~VRTC_BIT_MASK;
		update_memmap(read_pattern, true);
		update_memmap(write_pattern, false);
	}

	if ((v >= 0) && (v < disp_line)) {
		if (dmac.ch[2].running == true) {
			// V1S or N needs to halt cpu
			if ((config.boot_mode == MODE_PC88_V1S) || (config.boot_mode == MODE_PC88_N)) {
				d_cpu->write_signal(SIG_CPU_BUSREQ, 1, 1);
				register_event_by_clock(this, EVENT_BUSREQ, busreq_clocks, false, NULL);
			}

			// run dma transfer to crtc
			if((v % crtc.char_height) == 0) {
				dmac.run(2, 80 + crtc.attrib.num * 2);
			}
		}
	}

	if (v == disp_line) {
		// start V-BLANK
		if (dmac.ch[2].running == true) {
			// terminate DMAC transfer
			dmac.finish(2);
		}

		// for Romancia
		crtc.expand_buffer(hireso, Port31_400LINE);
		crtc.finish();

		// raise VRTC interrupt
		request_intr(IRQ_VRTC, true);

		// update wait pattern
		read_pattern |= VRTC_BIT_MASK;
		write_pattern |= VRTC_BIT_MASK;
		update_memmap(read_pattern, true);
		update_memmap(write_pattern, false);

		// can execute all rest lines
		next_line = -1;
	}

	return next_line;
}

void PC88::key_down(int code, bool repeat)
{
	if(!repeat) {
		if(code == 0x14) {
			key_caps ^= 1;
		} else if(code == 0x15) {
			key_kana ^= 1;
		}
	}
}

void PC88::play_tape(_TCHAR* file_path)
{
	close_tape();
	
	if(cmt_fio->Fopen(file_path, FILEIO_READ_BINARY)) {
		if(check_file_extension(file_path, _T(".n80"))) {
			cmt_fio->Fread(ram + 0x8000, 0x7f40, 1);
			cmt_fio->Fclose();
			d_cpu->set_sp(ram[0xff3e] | (ram[0xff3f] << 8));
			d_cpu->set_pc(0xff3d);
			return;
		}
		
		cmt_fio->Fseek(0, FILEIO_SEEK_END);
		cmt_bufcnt = cmt_fio->Ftell();
		cmt_bufptr = 0;
		cmt_data_carrier_cnt = 0;
		cmt_fio->Fseek(0, FILEIO_SEEK_SET);
		memset(cmt_buffer, 0, sizeof(cmt_buffer));
		cmt_fio->Fread(cmt_buffer, sizeof(cmt_buffer), 1);
		cmt_fio->Fclose();
		
		if(strncmp((char *)cmt_buffer, "PC-8801 Tape Image(T88)", 23) == 0) {
			// this is t88 format
			int ptr = 24, tag = -1, len = 0, prev_bufptr = 0;
			while(!(tag == 0 && len == 0)) {
				tag = cmt_buffer[ptr + 0] | (cmt_buffer[ptr + 1] << 8);
				len = cmt_buffer[ptr + 2] | (cmt_buffer[ptr + 3] << 8);
				ptr += 4;
				
				if(tag == 0x0101) {
					// data tag
					for(int i = 12; i < len; i++) {
						cmt_buffer[cmt_bufptr++] = cmt_buffer[ptr + i];
					}
				} else if(tag == 0x0102 || tag == 0x0103) {
					// data carrier
					if(prev_bufptr != cmt_bufptr) {
						cmt_data_carrier[cmt_data_carrier_cnt++] = prev_bufptr = cmt_bufptr;
					}
				}
				ptr += len;
			}
			cmt_bufcnt = cmt_bufptr;
			cmt_bufptr = 0;
		}
		cmt_play = (cmt_bufcnt != 0);
		
		if(cmt_play && Port30_MTON) {
			// start motor and detect the data carrier at the top of tape
			if(cmt_register_id != -1) {
				cancel_event(this, cmt_register_id);
			}
			register_event(this, EVENT_CMT_SEND, 5000, false, &cmt_register_id);
		}
	}
}

void PC88::rec_tape(_TCHAR* file_path)
{
	close_tape();
	
	if(cmt_fio->Fopen(file_path, FILEIO_READ_WRITE_NEW_BINARY)) {
		_tcscpy_s(rec_file_path, _MAX_PATH, file_path);
		cmt_bufptr = 0;
		cmt_rec = true;
	}
}

void PC88::close_tape()
{
	// close file
	release_tape();
	
	// clear sio buffer
	d_sio->write_signal(SIG_I8251_CLEAR, 0, 0);
}

void PC88::release_tape()
{
	// close file
	if(cmt_fio->IsOpened()) {
		if(cmt_rec && cmt_bufptr) {
			cmt_fio->Fwrite(cmt_buffer, cmt_bufptr, 1);
		}
		cmt_fio->Fclose();
	}
	cmt_play = cmt_rec = false;
}

bool PC88::now_skip()
{
	return (cmt_play && cmt_bufptr < cmt_bufcnt && Port30_MTON);
}

bool PC88::check_data_carrier()
{
	if(cmt_bufptr == 0) {
		return true;
	} else if(cmt_data_carrier_cnt) {
		for(int i = 0; i < cmt_data_carrier_cnt; i++) {
			if(cmt_data_carrier[i] == cmt_bufptr) {
				return true;
			}
		}
	} else if(cmt_buffer[cmt_bufptr] == 0xd3) {
		for(int i = 1; i < 10; i++) {
			if(cmt_buffer[cmt_bufptr + i] != cmt_buffer[cmt_bufptr]) {
				return false;
			}
		}
		return true;
	} else if(cmt_buffer[cmt_bufptr] == 0x9c) {
		for(int i = 1; i < 6; i++) {
			if(cmt_buffer[cmt_bufptr + i] != cmt_buffer[cmt_bufptr]) {
				return false;
			}
		}
		return true;
	}
	return false;
}

void PC88::draw_screen()
{
	// render text screen
	draw_text();

	// render graph screen
	bool disp_color_graph = true;
#if defined(_PC8001SR)
	if (config.boot_mode != MODE_PC80_V2) {
		if (Port31_V1_320x200) {
			disp_color_graph = draw_320x200_4color_graph();
		}
		else if (Port31_V1_MONO) {
			draw_640x200_mono_graph();
		}
		else {
			draw_640x200_attrib_graph();
		}
	}
	else {
		if (Port31_HCOLOR) {
			if (Port31_320x200) {
				disp_color_graph = draw_320x200_color_graph();
			}
			else {
				disp_color_graph = draw_640x200_color_graph();
			}
		}
		else {
			if (Port31_320x200) {
				draw_320x200_attrib_graph();
			}
			else {
				draw_640x200_attrib_graph();
			}
		}
	}
#else
	if (Port31_HCOLOR) {
		disp_color_graph = draw_640x200_color_graph();
	}
	else if (!Port31_400LINE) {
		draw_640x200_attrib_graph();
		//		draw_640x200_mono_graph();
	}
	else {
		draw_640x400_attrib_graph();
		//		draw_640x400_mono_graph();
	}
#endif

	// update palette
	if (update_palette) {
		static const int pex[8] = {
			0, 36, 73, 109, 146, 182, 219, 255 // from m88
		};
#if defined(_PC8001SR)
		if (config.boot_mode != MODE_PC80_V2) {
			if (Port31_V1_320x200) {
				for (int i = 0; i < 3; i++) {
					uint8 b = (port[0x31] & 4) ? 7 : 0;
					uint8 r = (i & 1) ? 7 : 0;
					uint8 g = (i & 2) ? 7 : 0;
					palette_graph_pc[i] = RGB_COLOR(pex[r], pex[g], pex[b]);
				}
				palette_graph_pc[3] = RGB_COLOR(pex[palette[8].r], pex[palette[8].g], pex[palette[8].b]);
			}
			else if (Port31_V1_MONO) {
				palette_graph_pc[0] = 0;
				palette_graph_pc[1] = RGB_COLOR(pex[palette[8].r], pex[palette[8].g], pex[palette[8].b]);
			}
			else {
				for (int i = 1; i < 8; i++) {
					palette_graph_pc[i] = RGB_COLOR((i & 2) ? 255 : 0, (i & 4) ? 255 : 0, (i & 1) ? 255 : 0);
				}
				palette_graph_pc[0] = RGB_COLOR(pex[palette[8].r], pex[palette[8].g], pex[palette[8].b]);
			}
			if (Port31_V1_320x200) {
				palette_text_pc[0] = 0;
			}
			else {
				palette_text_pc[0] = palette_graph_pc[0];
			}
		}
		else {
			for (int i = 0; i < 8; i++) {
				uint8 b = (port[0x54 + i] & 1) ? 7 : 0;
				uint8 r = (port[0x54 + i] & 2) ? 7 : 0;
				uint8 g = (port[0x54 + i] & 4) ? 7 : 0;
				palette_graph_pc[i] = RGB_COLOR(pex[r], pex[g], pex[b]);
			}
			if (!Port31_HCOLOR) {
				palette_graph_pc[0] = RGB_COLOR(pex[palette[8].r], pex[palette[8].g], pex[palette[8].b]);
			}
			palette_text_pc[0] = palette_graph_pc[0];
		}
#else
		for (int i = 0; i < 8; i++) {
			palette_graph_pc[i] = RGB_COLOR(pex[palette[i].r], pex[palette[i].g], pex[palette[i].b]);
		}
		if (!Port31_HCOLOR && !Port32_PMODE) {
			palette_graph_pc[0] = RGB_COLOR(pex[palette[8].r], pex[palette[8].g], pex[palette[8].b]);
		}
		palette_text_pc[0] = palette_graph_pc[0];
#endif
		update_palette = false;
	}

	// set back color to black if cg screen is off in color mode
	scrntype palette_text_back = palette_text_pc[0];
	scrntype palette_graph_back = palette_graph_pc[0];

	if (!disp_color_graph) {
		palette_text_pc[0] = palette_graph_pc[0] = 0;
	}
	palette_graph_pc[8] = /*palette_text_pc[8] = */palette_text_pc[0];

	// copy to screen buffer
#if !defined(_PC8001SR)
	if (!Port31_400LINE) {
#endif
		for (int y = 0; y < 200; y++) {
			scrntype* dest0 = emu->screen_buffer(y * 2);
			scrntype* dest1 = emu->screen_buffer(y * 2 + 1);
			uint8* src_t = text[y];
			uint8* src_g = graph[y];

#if defined(_PC8001SR)
			if (port[0x33] & 8) {
				for (int x = 0; x < 640; x++) {
					uint32_t g = src_g[x];
					dest0[x] = g ? palette_graph_pc[g] : palette_text_pc[src_t[x]];
				}
			}
			else {
				for (int x = 0; x < 640; x++) {
					uint32_t t = src_t[x];
					dest0[x] = t ? palette_text_pc[t] : palette_graph_pc[src_g[x]];
				}
			}
#else
			if (Port31_HCOLOR) {
#ifdef SDL
				if (config.scan_line) {
					for (int x = 0; x < 640; x++) {
						uint32 t = src_t[x];
						if (t) {
							dest1[x] = dest0[x] = palette_text_pc[t];
						}
						else {
							dest0[x] = palette_graph_pc[src_g[x]];
							dest1[x] = dest0[x] & 0xff3f3f3f;
						}
					}
				}
				else {
					for (int x = 0; x < 640; x++) {
						uint32 t = src_t[x];
						dest1[x] = dest0[x] = t ? palette_text_pc[t] : palette_graph_pc[src_g[x]];
					}
				}
#else
				for (int x = 0; x < 640; x++) {
					uint32_t t = src_t[x];
					dest0[x] = t ? palette_text_pc[t] : palette_graph_pc[src_g[x]];
				}
#endif // SDL
			}
			else if (Port32_PMODE) {
#ifdef SDL
				if (config.scan_line) {
					for (int x = 0; x < 640; x++) {
						uint32 t = src_t[x];
						if (t) {
							dest1[x] = dest0[x] = palette_graph_pc[t];
						}
						else {
							dest0[x] = palette_graph_pc[src_g[x]];
							dest1[x] = dest0[x] & 0xff3f3f3f;
						}
					}
				}
				else {
					for (int x = 0; x < 640; x++) {
						uint32 t = src_t[x];
						dest1[x] = dest0[x] = palette_graph_pc[t ? t : src_g[x]];
					}
				}
#else
				for (int x = 0; x < 640; x++) {
					uint32_t t = src_t[x];
					dest0[x] = palette_graph_pc[t ? t : src_g[x]];
				}
#endif // SDL
			}
			else {
#ifdef SDL
				if (config.scan_line) {
					for (int x = 0; x < 640; x++) {
						uint32 t = src_t[x];
						if (t) {
							dest1[x] = dest0[x] = palette_text_pc[t];
						}
						else {
							dest0[x] = palette_text_pc[src_g[x]];
							dest1[x] = dest0[x] & 0xff3f3f3f;
						}
					}
				}
				else {
					for (int x = 0; x < 640; x++) {
						uint32 t = src_t[x];
						dest1[x] = dest0[x] = palette_text_pc[t ? t : src_g[x]];
					}
				}
#else
				for (int x = 0; x < 640; x++) {
					uint32_t t = src_t[x];
					dest0[x] = palette_text_pc[t ? t : src_g[x]];
				}
#endif // SDL
			}
#endif
#ifndef SDL
			if (config.scan_line) {
				memset(dest1, 0, 640 * sizeof(scrntype));
			}
			else {
				for (int x = 0; x < 640; x++) {
					dest1[x] = dest0[x];
				}
			}
#endif // !SDL
		}
		emu->screen_skip_line = true;
#if !defined(_PC8001SR)
	}
	else {
		for (int y = 0; y < 400; y++) {
			scrntype* dest = emu->screen_buffer(y);
			uint8* src_t = text[y >> 1];
			uint8* src_g = graph[y];

			//			if(Port31_HCOLOR) {
			//				for(int x = 0; x < 640; x++) {
			//					uint32_t t = src_t[x];
			//					dest[x] = t ? palette_text_pc[t] : palette_graph_pc[src_g[x]];
			//				}
			//			} else
			if (Port32_PMODE) {
				for (int x = 0; x < 640; x++) {
					uint32 t = src_t[x];
					dest[x] = palette_graph_pc[t ? t : src_g[x]];
				}
			}
			else {
				for (int x = 0; x < 640; x++) {
					uint32 t = src_t[x];
					dest[x] = palette_text_pc[t ? t : src_g[x]];
				}
			}
		}
		emu->screen_skip_line = false;
	}
#endif

	// restore back color palette
	palette_text_pc[0] = palette_text_back;
	palette_graph_pc[0] = palette_graph_back;

#ifdef SDL
	// get dummy screen buffer (see Video::GetFrameBuf)
	emu->screen_buffer(400);
#endif // SDL
}

/*
	attributes:
	
	bit7: green
	bit6: red
	bit5: blue
	bit4: graph=1/character=0
	bit3: under line
	bit2: upper line
	bit1: secret
	bit0: reverse
*/

void PC88::draw_text()
{
	if (crtc.status & 0x88) {
		// dma underrun
		crtc.status &= ~0x80;
		memset(crtc.text.expand, 0, 200 * 80);
		memset(crtc.attrib.expand, crtc.reverse ? 3 : 2, 200 * 80);
	}

	// for Advanced Fantasian Opening (20line) (XM8 version 1.00)
	if (!(crtc.status & 0x10) || Port53_TEXTDS) {
		//	if(!(crtc.status & 0x10) || (crtc.status & 8) || Port53_TEXTDS) {
		memset(crtc.text.expand, 0, 200 * 80);
		for (int y = 0; y < 200; y++) {
			for (int x = 0; x < 80; x++) {
				crtc.attrib.expand[y][x] &= 0xe0;
				crtc.attrib.expand[y][x] |= 0x02;
			}
		}
		//		memset(crtc.attrib.expand, 2, 200 * 80);
	}

	// for Xak2 opening
	memset(text, 8, sizeof(text));
	memset(text_color, 7, sizeof(text_color));
	memset(text_reverse, 0, sizeof(text_reverse));

	int char_height = crtc.char_height;
	uint8 color_mask = Port30_COLOR ? 0 : 7;

	if (!hireso) {
		char_height <<= 1;
	}
	//	if(Port31_400LINE || !crtc.skip_line) {
	//		char_height >>= 1;
	//	}
	if (crtc.skip_line) {
		char_height <<= 1;
	}
	//	for(int cy = 0, ytop = 0; cy < 64 && ytop < 400; cy++, ytop += char_height) {
	for (int cy = 0, ytop = 0; cy < crtc.height && ytop < 400; cy++, ytop += char_height) {
		for (int x = 0, cx = 0; cx < crtc.width; x += 8, cx++) {
			if (Port30_40 && (cx & 1)) {
				continue;
			}
			uint8 attrib = crtc.attrib.expand[cy][cx];
			//			uint8 color = !(Port30_COLOR && (attrib & 8)) ? 7 : (attrib & 0xe0) ? (attrib >> 5) : 8;
			uint8 color = (attrib & 0xe0) ? ((attrib >> 5) | color_mask) : 8;
			bool under_line = ((attrib & 8) != 0);
			bool upper_line = ((attrib & 4) != 0);
			bool secret = ((attrib & 2) != 0);
			bool reverse = ((attrib & 1) != 0);

			uint8 code = secret ? 0 : crtc.text.expand[cy][cx];
#ifdef SUPPORT_PC88_PCG8100
			uint8 *pattern = ((attrib & 0x10) ? sg_pattern : pcg_pattern) + code * 8;
#else
			uint8 *pattern = ((attrib & 0x10) ? sg_pattern : kanji1 + 0x1000) + code * 8;
#endif

			for (int l = 0, y = ytop; l < char_height / 2 && y < 400; l++, y += 2) {
				uint8 pat = (l < 8) ? pattern[l] : 0;
				if ((upper_line && l == 0) || (under_line && l >= 7)) {
					pat = 0xff;
				}
				if (reverse) {
					pat ^= 0xff;
				}

				uint8 *dest = &text[y >> 1][x];
				if (Port30_40) {
					dest[0] = dest[1] = (pat & 0x80) ? color : 0;
					dest[2] = dest[3] = (pat & 0x40) ? color : 0;
					dest[4] = dest[5] = (pat & 0x20) ? color : 0;
					dest[6] = dest[7] = (pat & 0x10) ? color : 0;
					dest[8] = dest[9] = (pat & 0x08) ? color : 0;
					dest[10] = dest[11] = (pat & 0x04) ? color : 0;
					dest[12] = dest[13] = (pat & 0x02) ? color : 0;
					dest[14] = dest[15] = (pat & 0x01) ? color : 0;

					// store text color for monocolor graph screen
					text_color[y >> 1][cx + 0] =
						text_color[y >> 1][cx + 1] = color;
					text_reverse[y >> 1][cx + 0] =
						text_reverse[y >> 1][cx + 1] = reverse;
				}
				else {
					dest[0] = (pat & 0x80) ? color : 0;
					dest[1] = (pat & 0x40) ? color : 0;
					dest[2] = (pat & 0x20) ? color : 0;
					dest[3] = (pat & 0x10) ? color : 0;
					dest[4] = (pat & 0x08) ? color : 0;
					dest[5] = (pat & 0x04) ? color : 0;
					dest[6] = (pat & 0x02) ? color : 0;
					dest[7] = (pat & 0x01) ? color : 0;

					// store text color for monocolor graph screen
					text_color[y >> 1][cx] = color;
					text_reverse[y >> 1][cx] = reverse;
				}
			}
		}
	}
}

#if defined(_PC8001SR)
bool PC88::draw_320x200_color_graph()
{
	if(!Port31_GRAPH || (Port53_G0DS && Port53_G1DS)) {
		memset(graph, 0, sizeof(graph));
		return false;
	}
	uint8 *gvram_b0 = Port53_G0DS ? gvram_null : (gvram + 0x0000);
	uint8 *gvram_r0 = Port53_G0DS ? gvram_null : (gvram + 0x4000);
	uint8 *gvram_g0 = Port53_G0DS ? gvram_null : (gvram + 0x8000);
	uint8 *gvram_b1 = Port53_G1DS ? gvram_null : (gvram + 0x2000);
	uint8 *gvram_r1 = Port53_G1DS ? gvram_null : (gvram + 0x6000);
	uint8 *gvram_g1 = Port53_G1DS ? gvram_null : (gvram + 0xa000);

	if(port[0x33] & 4) {
		// G1>G0
		uint8 *tmp;
		tmp = gvram_b0; gvram_b0 = gvram_b1; gvram_b1 = tmp;
		tmp = gvram_r0; gvram_r0 = gvram_r1; gvram_r1 = tmp;
		tmp = gvram_g0; gvram_g0 = gvram_g1; gvram_g1 = tmp;
	}

	for(int y = 0, addr = 0; y < 200; y++) {
		for(int x = 0; x < 640; x += 16) {
			uint8 b0 = gvram_b0[addr];
			uint8 r0 = gvram_r0[addr];
			uint8 g0 = gvram_g0[addr];
			uint8 b1 = gvram_b1[addr];
			uint8 r1 = gvram_r1[addr];
			uint8 g1 = gvram_g1[addr];
			addr++;
			uint8 *dest = &graph[y][x];
			uint8 brg0, brg1;
			brg0 = ((b0 & 0x80) >> 7) | ((r0 & 0x80) >> 6) | ((g0 & 0x80) >> 5);
			brg1 = ((b1 & 0x80) >> 7) | ((r1 & 0x80) >> 6) | ((g1 & 0x80) >> 5);
			dest[ 0] = dest[ 1] = brg0 ? brg0 : brg1;
			brg0 = ((b0 & 0x40) >> 6) | ((r0 & 0x40) >> 5) | ((g0 & 0x40) >> 4);
			brg1 = ((b1 & 0x40) >> 6) | ((r1 & 0x40) >> 5) | ((g1 & 0x40) >> 4);
			dest[ 2] = dest[ 3] = brg0 ? brg0 : brg1;
			brg0 = ((b0 & 0x20) >> 5) | ((r0 & 0x20) >> 4) | ((g0 & 0x20) >> 3);
			brg1 = ((b1 & 0x20) >> 5) | ((r1 & 0x20) >> 4) | ((g1 & 0x20) >> 3);
			dest[ 4] = dest[ 5] = brg0 ? brg0 : brg1;
			brg0 = ((b0 & 0x10) >> 4) | ((r0 & 0x10) >> 3) | ((g0 & 0x10) >> 2);
			brg1 = ((b1 & 0x10) >> 4) | ((r1 & 0x10) >> 3) | ((g1 & 0x10) >> 2);
			dest[ 6] = dest[ 7] = brg0 ? brg0 : brg1;
			brg0 = ((b0 & 0x08) >> 3) | ((r0 & 0x08) >> 2) | ((g0 & 0x08) >> 1);
			brg1 = ((b1 & 0x08) >> 3) | ((r1 & 0x08) >> 2) | ((g1 & 0x08) >> 1);
			dest[ 8] = dest[ 9] = brg0 ? brg0 : brg1;
			brg0 = ((b0 & 0x04) >> 2) | ((r0 & 0x04) >> 1) | ((g0 & 0x04)     );
			brg1 = ((b1 & 0x04) >> 2) | ((r1 & 0x04) >> 1) | ((g1 & 0x04)     );
			dest[10] = dest[11] = brg0 ? brg0 : brg1;
			brg0 = ((b0 & 0x02) >> 1) | ((r0 & 0x02)     ) | ((g0 & 0x02) << 1);
			brg1 = ((b1 & 0x02) >> 1) | ((r1 & 0x02)     ) | ((g1 & 0x02) << 1);
			dest[12] = dest[13] = brg0 ? brg0 : brg1;
			brg0 = ((b0 & 0x01)     ) | ((r0 & 0x01) << 1) | ((g0 & 0x01) << 2);
			brg1 = ((b1 & 0x01)     ) | ((r1 & 0x01) << 1) | ((g1 & 0x01) << 2);
			dest[14] = dest[15] = brg0 ? brg0 : brg1;
		}
	}
	return true;
}

bool PC88::draw_320x200_4color_graph()
{
	if(!Port31_GRAPH || (Port53_G0DS && Port53_G1DS && Port53_G2DS)) {
		memset(graph, 0, sizeof(graph));
		return false;
	}
	uint8 *gvram_b = Port53_G0DS ? gvram_null : (gvram + 0x0000);
	uint8 *gvram_r = Port53_G1DS ? gvram_null : (gvram + 0x4000);
	uint8 *gvram_g = Port53_G2DS ? gvram_null : (gvram + 0x8000);

	for(int y = 0, addr = 0; y < 200; y++) {
		for(int x = 0; x < 640; x += 8) {
			uint8 brg = gvram_b[addr] | gvram_r[addr] | gvram_g[addr];
			addr++;
			uint8 *dest = &graph[y][x];
			dest[0] = dest[1] = (brg >> 6) & 3;
			dest[2] = dest[3] = (brg >> 4) & 3;
			dest[4] = dest[5] = (brg >> 2) & 3;
			dest[6] = dest[7] = (brg     ) & 3;
		}
	}
	return true;
}

void PC88::draw_320x200_attrib_graph()
{
	if(!Port31_GRAPH || (Port53_G0DS && Port53_G1DS && Port53_G2DS && Port53_G3DS && Port53_G4DS && Port53_G5DS)) {
		memset(graph, 0, sizeof(graph));
		return;
	}
	uint8 *gvram_b0 = Port53_G0DS ? gvram_null : (gvram + 0x0000);
	uint8 *gvram_r0 = Port53_G1DS ? gvram_null : (gvram + 0x4000);
	uint8 *gvram_g0 = Port53_G2DS ? gvram_null : (gvram + 0x8000);
	uint8 *gvram_b1 = Port53_G3DS ? gvram_null : (gvram + 0x2000);
	uint8 *gvram_r1 = Port53_G4DS ? gvram_null : (gvram + 0x6000);
	uint8 *gvram_g1 = Port53_G5DS ? gvram_null : (gvram + 0xa000);

	if(Port30_40) {
		for(int y = 0, addr = 0; y < 200; y++) {
			for(int x = 0, cx = 0; x < 640; x += 16, cx += 2) {
				uint8 color = text_color[y][cx];
				uint8 brg = gvram_b0[addr] | gvram_r0[addr] | gvram_g0[addr] |
					gvram_b1[addr] | gvram_r1[addr] | gvram_g1[addr];
				if(text_reverse[y][cx]) {
					brg ^= 0xff;
				}
				addr++;
				uint8 *dest = &graph[y][x];
				dest[ 0] = dest[ 1] = (brg & 0x80) ? color : 0;
				dest[ 2] = dest[ 3] = (brg & 0x40) ? color : 0;
				dest[ 4] = dest[ 5] = (brg & 0x20) ? color : 0;
				dest[ 6] = dest[ 7] = (brg & 0x10) ? color : 0;
				dest[ 8] = dest[ 9] = (brg & 0x08) ? color : 0;
				dest[10] = dest[11] = (brg & 0x04) ? color : 0;
				dest[12] = dest[13] = (brg & 0x02) ? color : 0;
				dest[14] = dest[15] = (brg & 0x01) ? color : 0;
			}
		}
	} else {
		for(int y = 0, addr = 0; y < 200; y++) {
			for(int x = 0, cx = 0; x < 640; x += 16, cx += 2) {
				uint8 color_l = text_color[y][cx + 0];
				uint8 color_r = text_color[y][cx + 1];
				uint8 brg = gvram_b0[addr] | gvram_r0[addr] | gvram_g0[addr] |
					gvram_b1[addr] | gvram_r1[addr] | gvram_g1[addr];
				if(text_reverse[y][cx + 0]) {
					brg ^= 0xf0;
				}
				if(text_reverse[y][cx + 1]) {
					brg ^= 0x0f;
				}
				addr++;
				uint8 *dest = &graph[y][x];
				dest[ 0] = dest[ 1] = (brg & 0x80) ? color_l : 0;
				dest[ 2] = dest[ 3] = (brg & 0x40) ? color_l : 0;
				dest[ 4] = dest[ 5] = (brg & 0x20) ? color_l : 0;
				dest[ 6] = dest[ 7] = (brg & 0x10) ? color_l : 0;
				dest[ 8] = dest[ 9] = (brg & 0x08) ? color_r : 0;
				dest[10] = dest[11] = (brg & 0x04) ? color_r : 0;
				dest[12] = dest[13] = (brg & 0x02) ? color_r : 0;
				dest[14] = dest[15] = (brg & 0x01) ? color_r : 0;
			}
		}
	}
}
#endif

bool PC88::draw_640x200_color_graph()
{
#if defined(_PC8001SR)
	if(!Port31_GRAPH || Port53_G0DS) {
#else
	if (!Port31_GRAPH/* || (Port53_G0DS && Port53_G1DS && Port53_G2DS)*/) {
#endif
		memset(graph, 0, sizeof(graph));
		return false;
	}
	uint8 *gvram_b = /*Port53_G0DS ? gvram_null : */(gvram + 0x0000);
	uint8 *gvram_r = /*Port53_G1DS ? gvram_null : */(gvram + 0x4000);
	uint8 *gvram_g = /*Port53_G2DS ? gvram_null : */(gvram + 0x8000);

	for (int y = 0, addr = 0; y < 200; y++) {
		for (int x = 0; x < 640; x += 8) {
			uint8 b = gvram_b[addr];
			uint8 r = gvram_r[addr];
			uint8 g = gvram_g[addr];
			addr++;
			uint8 *dest = &graph[y][x];
			dest[0] = ((b & 0x80) >> 7) | ((r & 0x80) >> 6) | ((g & 0x80) >> 5);
			dest[1] = ((b & 0x40) >> 6) | ((r & 0x40) >> 5) | ((g & 0x40) >> 4);
			dest[2] = ((b & 0x20) >> 5) | ((r & 0x20) >> 4) | ((g & 0x20) >> 3);
			dest[3] = ((b & 0x10) >> 4) | ((r & 0x10) >> 3) | ((g & 0x10) >> 2);
			dest[4] = ((b & 0x08) >> 3) | ((r & 0x08) >> 2) | ((g & 0x08) >> 1);
			dest[5] = ((b & 0x04) >> 2) | ((r & 0x04) >> 1) | ((g & 0x04));
			dest[6] = ((b & 0x02) >> 1) | ((r & 0x02)) | ((g & 0x02) << 1);
			dest[7] = ((b & 0x01)) | ((r & 0x01) << 1) | ((g & 0x01) << 2);
		}
	}
	return true;
}

void PC88::draw_640x200_mono_graph()
{
	if (!Port31_GRAPH || (Port53_G0DS && Port53_G1DS && Port53_G2DS)) {
		memset(graph, 0, sizeof(graph));
		return;
	}
	uint8 *gvram_b = Port53_G0DS ? gvram_null : (gvram + 0x0000);
	uint8 *gvram_r = Port53_G1DS ? gvram_null : (gvram + 0x4000);
	uint8 *gvram_g = Port53_G2DS ? gvram_null : (gvram + 0x8000);

	for (int y = 0, addr = 0; y < 200; y++) {
		for (int x = 0; x < 640; x += 8) {
			uint8 brg = gvram_b[addr] | gvram_r[addr] | gvram_g[addr];
			addr++;
			uint8 *dest = &graph[y][x];
			dest[0] = (brg & 0x80) >> 7;
			dest[1] = (brg & 0x40) >> 6;
			dest[2] = (brg & 0x20) >> 5;
			dest[3] = (brg & 0x10) >> 4;
			dest[4] = (brg & 0x08) >> 3;
			dest[5] = (brg & 0x04) >> 2;
			dest[6] = (brg & 0x02) >> 1;
			dest[7] = (brg & 0x01);
		}
	}
}

void PC88::draw_640x200_attrib_graph()
{
	if (!Port31_GRAPH || (Port53_G0DS && Port53_G1DS && Port53_G2DS)) {
		memset(graph, 0, sizeof(graph));
		return;
	}
	uint8 *gvram_b = Port53_G0DS ? gvram_null : (gvram + 0x0000);
	uint8 *gvram_r = Port53_G1DS ? gvram_null : (gvram + 0x4000);
	uint8 *gvram_g = Port53_G2DS ? gvram_null : (gvram + 0x8000);

	for (int y = 0, addr = 0; y < 200; y++) {
		for (int x = 0, cx = 0; x < 640; x += 8, cx++) {
			uint8 color = text_color[y][cx];
			uint8 brg = gvram_b[addr] | gvram_r[addr] | gvram_g[addr];
			if (text_reverse[y][cx]) {
				brg ^= 0xff;
			}
			addr++;
			uint8 *dest = &graph[y][x];
			dest[0] = (brg & 0x80) ? color : 0;
			dest[1] = (brg & 0x40) ? color : 0;
			dest[2] = (brg & 0x20) ? color : 0;
			dest[3] = (brg & 0x10) ? color : 0;
			dest[4] = (brg & 0x08) ? color : 0;
			dest[5] = (brg & 0x04) ? color : 0;
			dest[6] = (brg & 0x02) ? color : 0;
			dest[7] = (brg & 0x01) ? color : 0;
		}
	}
}

void PC88::draw_640x400_mono_graph()
{
	if (!Port31_GRAPH || (Port53_G0DS && Port53_G1DS)) {
		memset(graph, 0, sizeof(graph));
		return;
	}
	uint8 *gvram_b = Port53_G0DS ? gvram_null : (gvram + 0x0000);
	uint8 *gvram_r = Port53_G1DS ? gvram_null : (gvram + 0x4000);

	for (int y = 0, addr = 0; y < 200; y++) {
		for (int x = 0; x < 640; x += 8) {
			uint8 b = gvram_b[addr];
			addr++;
			uint8 *dest = &graph[y][x];
			dest[0] = (b & 0x80) >> 7;
			dest[1] = (b & 0x40) >> 6;
			dest[2] = (b & 0x20) >> 5;
			dest[3] = (b & 0x10) >> 4;
			dest[4] = (b & 0x08) >> 3;
			dest[5] = (b & 0x04) >> 2;
			dest[6] = (b & 0x02) >> 1;
			dest[7] = (b & 0x01);
		}
	}
	for (int y = 200, addr = 0; y < 400; y++) {
		for (int x = 0; x < 640; x += 8) {
			uint8 r = gvram_r[addr];
			addr++;
			uint8 *dest = &graph[y][x];
			dest[0] = (r & 0x80) >> 7;
			dest[1] = (r & 0x40) >> 6;
			dest[2] = (r & 0x20) >> 5;
			dest[3] = (r & 0x10) >> 4;
			dest[4] = (r & 0x08) >> 3;
			dest[5] = (r & 0x04) >> 2;
			dest[6] = (r & 0x02) >> 1;
			dest[7] = (r & 0x01);
		}
	}
}

void PC88::draw_640x400_attrib_graph()
{
	if (!Port31_GRAPH || (Port53_G0DS && Port53_G1DS)) {
		memset(graph, 0, sizeof(graph));
		return;
	}
	uint8 *gvram_b = Port53_G0DS ? gvram_null : (gvram + 0x0000);
	uint8 *gvram_r = Port53_G1DS ? gvram_null : (gvram + 0x4000);

	for (int y = 0, addr = 0; y < 200; y++) {
		for (int x = 0, cx = 0; x < 640; x += 8, cx++) {
			uint8 color = text_color[y >> 1][cx];
			uint8 b = gvram_b[addr];
			if (text_reverse[y >> 1][cx]) {
				b ^= 0xff;
			}
			addr++;
			uint8 *dest = &graph[y][x];
			dest[0] = (b & 0x80) ? color : 0;
			dest[1] = (b & 0x40) ? color : 0;
			dest[2] = (b & 0x20) ? color : 0;
			dest[3] = (b & 0x10) ? color : 0;
			dest[4] = (b & 0x08) ? color : 0;
			dest[5] = (b & 0x04) ? color : 0;
			dest[6] = (b & 0x02) ? color : 0;
			dest[7] = (b & 0x01) ? color : 0;
		}
	}
	for (int y = 200, addr = 0; y < 400; y++) {
		for (int x = 0, cx = 0; x < 640; x += 8, cx++) {
			uint8 color = text_color[y >> 1][cx];
			uint8 r = gvram_r[addr];
			if (text_reverse[y >> 1][cx]) {
				r ^= 0xff;
			}
			addr++;
			uint8 *dest = &graph[y][x];
			dest[0] = (r & 0x80) ? color : 0;
			dest[1] = (r & 0x40) ? color : 0;
			dest[2] = (r & 0x20) ? color : 0;
			dest[3] = (r & 0x10) ? color : 0;
			dest[4] = (r & 0x08) ? color : 0;
			dest[5] = (r & 0x04) ? color : 0;
			dest[6] = (r & 0x02) ? color : 0;
			dest[7] = (r & 0x01) ? color : 0;
		}
	}
}

void PC88::request_intr(int level, bool status)
{
	uint8 bit = 1 << level;
	
	if(status) {
#ifdef SDL
		// for Nobunaga Fuunroku Opening & MID-GARTS Opening
#else
		bit &= intr_mask2;
#endif // SDL
		if(!(intr_req & bit)) {
			intr_req |= bit;
			update_intr();
		}
	} else {
		if(intr_req & bit) {
			intr_req &= ~bit;
			update_intr();
		}
	}
}

void PC88::update_intr()
{
	d_cpu->set_intr_line(((intr_req & intr_mask1 & intr_mask2) != 0), true, 0);
}

uint32 PC88::intr_ack()
{
	uint8 ai = intr_req & intr_mask1 & intr_mask2;
	
	for(int i = 0; i < 8; i++, ai >>= 1) {
		if(ai & 1) {
			intr_req &= ~(1 << i);
			intr_mask1 = 0;
			return i * 2;
		}
	}
	return 0;
}

void PC88::intr_ei()
{
	update_intr();
}

/* ----------------------------------------------------------------------------
	CRTC (uPD3301)
---------------------------------------------------------------------------- */

void pc88_crtc_t::reset(bool hireso)
{
	blink.rate = 24;
	cursor.type = cursor.mode = -1;
	cursor.x = cursor.y = -1;
	attrib.data = 0xe0;
	attrib.mask = 0xff;
	attrib.num = 20;
	width = 80;
	height = 25;
	char_height = hireso ? 16 : 8;
	skip_line = false;
	vretrace = hireso ? 3 : 7;
	timing_changed = false;
	reverse = 0;
	intr_mask = 3;
}

void pc88_crtc_t::write_cmd(uint8 data)
{
	cmd = (data >> 5) & 7;
	cmd_ptr = 0;
	switch(cmd) {
	case 0:	// reset
		status &= ~0x16;
#ifdef SDL
		status |= 0x80;
#endif // SDL
		cursor.x = cursor.y = -1;
		break;
	case 1:	// start display
		reverse = data & 1;
#ifdef SDL
		status |= 0x90;
#else
		status |= 0x10;
#endif // SDL
		status &= ~8;
		break;
	case 2:	// set interrupt mask
		if(!(data & 1)) {
#ifdef SDL
			status = 0x80; // fix
#else
			status = 0; // from M88
#endif // SDL
		}
		intr_mask = data & 3;
		break;
	case 3:	// read light pen
		status &= ~1;
		break;
	case 4:	// load cursor position ON/OFF
		cursor.type = (data & 1) ? cursor.mode : -1;
		break;
	case 5:	// reset interrupt
		status &= ~6;
		break;
	case 6:	// reset counters
		status &= ~6;
		break;
	}
}

void pc88_crtc_t::write_param(uint8 data)
{
	switch(cmd) {
	case 0:
		switch(cmd_ptr) {
		case 0:
			width = min((data & 0x7f) + 2, 80);
			break;
		case 1:
			if(height != (data & 0x3f) + 1) {
				height = (data & 0x3f) + 1;
				timing_changed = true;
			}
			blink.rate = 32 * ((data >> 6) + 1);
			break;
		case 2:
			if(char_height != (data & 0x1f) + 1) {
				char_height = (data & 0x1f) + 1;
				timing_changed = true;
			}
			cursor.mode = (data >> 5) & 3;
			skip_line = ((data & 0x80) != 0);
			break;
		case 3:
			if(vretrace != ((data >> 5) & 7) + 1) {
				vretrace = ((data >> 5) & 7) + 1;
				timing_changed = true;
			}
			break;
		case 4:
			mode = (data >> 5) & 7;
			attrib.num = (mode & 1) ? 0 : min((data & 0x1f) + 1, 20);
			break;
		}
		break;
	case 4:
		switch(cmd_ptr) {
		case 0:
			cursor.x = data;
			break;
		case 1:
			cursor.y = data;
			break;
		}
		break;
#ifdef SDL
	case 6:
		status = 0;
		break;
#endif // SDL
	}
	cmd_ptr++;
}

uint32 pc88_crtc_t::read_param()
{
	uint32 val = 0xff;
	
	switch(cmd) {
	case 3:	// read light pen
		switch(cmd_ptr) {
		case 0:
			val = 0; // fix me
			break;
		case 1:
			val = 0; // fix me
			break;
		}
		break;

#ifdef SDL
	// version 1.10
	default:
		val = read_status();
#endif // SDL
	}
	cmd_ptr++;
	return val;
}

uint32 pc88_crtc_t::read_status()
{
	if(status & 8) {
		return status & ~0x10;
	} else {
		return status;
	}
}

void pc88_crtc_t::start()
{
	memset(buffer, 0, sizeof(buffer));
	buffer_ptr = 0;
	vblank = false;
}

void pc88_crtc_t::finish()
{
	if((status & 0x10) && !(intr_mask & 1)) {
		status |= 2;
	}
	vblank = true;
}

void pc88_crtc_t::write_buffer(uint8 data)
{
	buffer[(buffer_ptr++) & 0x3fff] = data;
}

uint8 pc88_crtc_t::read_buffer(int ofs)
{
	if(ofs < buffer_ptr) {
		return buffer[ofs];
	}
	// dma underrun occurs !!!
	status |= 8;
//	status &= ~0x10;
	return 0;
}

void pc88_crtc_t::update_blink()
{
	// from m88
	if(++blink.counter > blink.rate) {
		blink.counter = 0;
	}
	blink.attrib = (blink.counter < blink.rate / 4) ? 2 : 0;
	blink.cursor = (blink.counter <= blink.rate / 4) || (blink.rate / 2 <= blink.counter && blink.counter <= 3 * blink.rate / 4);
}

void pc88_crtc_t::expand_buffer(bool hireso, bool line400)
{
	int char_height_tmp = char_height;
	int exitline = -1;
	
	if(!hireso) {
		char_height_tmp <<= 1;
	}
	if(line400 || !skip_line) {
		char_height_tmp >>= 1;
	}
	if(!(status & 0x10)) {
		exitline = 0;
		goto underrun;
	}
	for(int cy = 0, ytop = 0, ofs = 0; cy < height && ytop < 200; cy++, ytop += char_height_tmp, ofs += 80 + attrib.num * 2) {
		for(int cx = 0; cx < width; cx++) {
			text.expand[cy][cx] = read_buffer(ofs + cx);
		}
		if((status & 8) && exitline == -1) {
			exitline = cy;
//			goto underrun;
		}
	}
	if(mode & 4) {
		// non transparent
		for(int cy = 0, ytop = 0, ofs = 0; cy < height && ytop < 200; cy++, ytop += char_height_tmp, ofs += 80 + attrib.num * 2) {
			for(int cx = 0; cx < width; cx += 2) {
				set_attrib(read_buffer(ofs + cx + 1));
				attrib.expand[cy][cx] = attrib.expand[cy][cx + 1] = attrib.data & attrib.mask;
			}
			if((status & 8) && exitline == -1) {
				exitline = cy;
//				goto underrun;
			}
		}
	} else {
		// transparent
		if(mode & 1) {
			memset(attrib.expand, 0xe0, sizeof(attrib.expand));
		} else {
			for(int cy = 0, ytop = 0, ofs = 0; cy < height && ytop < 200; cy++, ytop += char_height_tmp, ofs += 80 + attrib.num * 2) {
				uint8 flags[128];
				memset(flags, 0, sizeof(flags));
				for(int i = 2 * (attrib.num - 1); i >= 0; i -= 2) {
					flags[read_buffer(ofs + i + 80) & 0x7f] = 1;
				}
				for(int cx = 0, pos = 0; cx < width; cx++) {
					if(flags[cx]) {
						set_attrib(read_buffer(ofs + pos + 81));
						pos += 2;
					}
					attrib.expand[cy][cx] = attrib.data & attrib.mask;
				}
				if((status & 8) && exitline == -1) {
					exitline = cy;
//					goto underrun;
				}
			}
		}
	}
	if(cursor.x < 80 && cursor.y < 200) {
		if((cursor.type & 1) && blink.cursor) {
			// no cursor
		} else {
			static const uint8 ctype[5] = {0, 8, 8, 1, 1};
			attrib.expand[cursor.y][cursor.x] ^= ctype[cursor.type + 1];
		}
	}
	// only burst mode
underrun:
	if(exitline != -1) {
		for(int cy = exitline; cy < 200; cy++) {
			memset(&text.expand[cy][0], 0, width);
			memset(&attrib.expand[cy][0], 0, width);
			memset(&attrib.expand[cy][0], 0xe0, width); // color=7
		}
	}
}

void pc88_crtc_t::set_attrib(uint8 code)
{
	if(mode & 2) {
		// color
		if(code & 8) {
			attrib.data = (attrib.data & 0x0f) | (code & 0xf0);
			attrib.mask = 0xf3; //for PC-8801mkIIFR •t‘®ƒfƒ‚
		} else {
			attrib.data = (attrib.data & 0xf0) | ((code >> 2) & 0x0d) | ((code << 1) & 2);
			attrib.data ^= reverse;
			attrib.data ^= ((code & 2) && !(code & 1)) ? blink.attrib : 0;
			attrib.mask = 0xff;
		}
	} else {
		attrib.data = 0xe0 | ((code >> 3) & 0x10) | ((code >> 2) & 0x0d) | ((code << 1) & 2);
		attrib.data ^= reverse;
		attrib.data ^= ((code & 2) && !(code & 1)) ? blink.attrib : 0;
		attrib.mask = 0xff;
	}
}

/* ----------------------------------------------------------------------------
	DMAC (uPD8257)
---------------------------------------------------------------------------- */

void pc88_dmac_t::write_io8(uint32 addr, uint32 data)
{
	int c = (addr >> 1) & 3;
	
	switch(addr & 0x0f) {
	case 0:
	case 2:
	case 4:
	case 6:
		if(!high_low) {
			if((mode & 0x80) && c == 2) {
				ch[3].addr.b.l = data;
			}
			ch[c].addr.b.l = data;
		} else {
			if((mode & 0x80) && c == 2) {
				ch[3].addr.b.h = data;
			}
			ch[c].addr.b.h = data;
		}
		high_low = !high_low;
		break;
	case 1:
	case 3:
	case 5:
	case 7:
		if(!high_low) {
			if((mode & 0x80) && c == 2) {
				ch[3].count.b.l = data;
			}
			ch[c].count.b.l = data;
		} else {
			if((mode & 0x80) && c == 2) {
				ch[3].count.b.h = data & 0x3f;
				ch[3].mode = data & 0xc0;
			}
			ch[c].count.b.h = data & 0x3f;
			ch[c].mode = data & 0xc0;
		}
		high_low = !high_low;
		break;
	case 8:
		mode = data;
		high_low = false;
		break;
	}
}

uint32 pc88_dmac_t::read_io8(uint32 addr)
{
	uint32 val = 0xff;
	int c = (addr >> 1) & 3;
	
	switch(addr & 0x0f) {
	case 0:
	case 2:
	case 4:
	case 6:
		if(!high_low) {
			val = ch[c].addr.b.l;
		} else {
			val = ch[c].addr.b.h;
		}
		high_low = !high_low;
		break;
	case 1:
	case 3:
	case 5:
	case 7:
		if(!high_low) {
			val = ch[c].count.b.l;
		} else {
			val = (ch[c].count.b.h & 0x3f) | ch[c].mode;
		}
		high_low = !high_low;
		break;
	case 8:
		val = status;
		status &= 0xf0;
//		high_low = false;
		break;
	}
	return val;
}

void pc88_dmac_t::start(int c)
{
	if(mode & (1 << c)) {
		status &= ~(1 << c);
		ch[c].running = true;
	} else {
		ch[c].running = false;
	}
}

void pc88_dmac_t::run(int c, int nbytes)
{
	if(ch[c].running) {
		while(nbytes > 0 && ch[c].count.sd >= 0) {
//			if(ch[c].mode == 0x80) {
				ch[c].io->write_dma_io8(0, mem->read_dma_data8(ch[c].addr.w.l));
//			} else if(ch[c].mode == 0x40) {
//				mem->write_dma_data8(ch[c].addr.w.l, ch[c].io->read_dma_io8(0));
//			}
			ch[c].addr.sd++;
			ch[c].count.sd--;
			nbytes--;
		}
	}
}

void pc88_dmac_t::finish(int c)
{
	if(ch[c].running) {
		while(ch[c].count.sd >= 0) {
//			if(ch[c].mode == 0x80) {
				ch[c].io->write_dma_io8(0, mem->read_dma_data8(ch[c].addr.w.l));
//			} else if(ch[c].mode == 0x40) {
//				mem->write_dma_data8(ch[c].addr.w.l, ch[c].io->read_dma_io8(0));
//			}
			ch[c].addr.sd++;
			ch[c].count.sd--;
		}
		if((mode & 0x80) && c == 2) {
			ch[2].addr.sd = ch[3].addr.sd;
			ch[2].count.sd = ch[3].count.sd;
			ch[2].mode = ch[3].mode;
		} else if(mode & 0x40) {
			mode &= ~(1 << c);
		}
		status |= (1 << c);
		ch[c].running = false;
	}
}

#define STATE_VERSION_100		3
										// version 1.00 - version 1.10
#define STATE_VERSION_120		4
										// version 1.20 - version 1.61
#define STATE_VERSION_170		5
										// version 1.70 -

void PC88::save_state(FILEIO* state_fio)
{
	state_fio->FputUint32(STATE_VERSION_170);
	state_fio->FputInt32(this_device_id);
	
	state_fio->Fwrite(ram, sizeof(ram), 1);
#if defined(PC88_EXRAM_BANKS)
	state_fio->Fwrite(exram, sizeof(exram), 1);
#endif
	state_fio->Fwrite(gvram, sizeof(gvram), 1);
	state_fio->Fwrite(tvram, sizeof(tvram), 1);
	state_fio->Fwrite(port, sizeof(port), 1);
	state_fio->Fwrite(&crtc, sizeof(crtc), 1);
	state_fio->Fwrite(&dmac, sizeof(dmac), 1);
	state_fio->Fwrite(alu_reg, sizeof(alu_reg), 1);
	state_fio->FputUint8(gvram_plane);
	state_fio->FputUint8(gvram_sel);
	state_fio->FputBool(cpu_clock_low);
	state_fio->FputBool(mem_wait_on);
	state_fio->FputInt32(m1_wait_clocks);
	state_fio->FputInt32(mem_wait_clocks_r);
	state_fio->FputInt32(mem_wait_clocks_w);
	state_fio->FputInt32(tvram_wait_clocks_r);
	state_fio->FputInt32(tvram_wait_clocks_w);
	state_fio->FputInt32(gvram_wait_clocks_r);
	state_fio->FputInt32(gvram_wait_clocks_w);
	state_fio->FputInt32(busreq_clocks);
	state_fio->Fwrite(palette, sizeof(palette), 1);
	state_fio->FputBool(update_palette);
	state_fio->FputBool(hireso);
	state_fio->Fwrite(text, sizeof(text), 1);
	state_fio->Fwrite(graph, sizeof(graph), 1);
	state_fio->Fwrite(palette_text_pc, sizeof(palette_text_pc), 1);
	state_fio->Fwrite(palette_graph_pc, sizeof(palette_graph_pc), 1);
	state_fio->FputBool(usart_dcd);
	state_fio->FputBool(opn_busy);
	state_fio->FputUint8(key_caps);
	state_fio->FputUint8(key_kana);
#ifdef SUPPORT_PC88_JOYSTICK
	state_fio->FputUint32(mouse_strobe_clock);
	state_fio->FputUint32(mouse_strobe_clock_lim);
	state_fio->FputInt32(mouse_phase);
	state_fio->FputInt32(mouse_dx);
	state_fio->FputInt32(mouse_dy);
	state_fio->FputInt32(mouse_lx);
	state_fio->FputInt32(mouse_ly);
#endif
	state_fio->FputUint8(intr_req);
	state_fio->FputBool(intr_req_sound);
	state_fio->FputUint8(intr_mask1);
	state_fio->FputUint8(intr_mask2);
	state_fio->FputBool(cmt_play);
	state_fio->FputBool(cmt_rec);
	state_fio->Fwrite(rec_file_path, sizeof(rec_file_path), 1);
	if(cmt_rec && cmt_fio->IsOpened()) {
		int length_tmp = (int)cmt_fio->Ftell();
		cmt_fio->Fseek(0, FILEIO_SEEK_SET);
		state_fio->FputInt32(length_tmp);
		while(length_tmp != 0) {
			uint8 buffer[1024];
			int length_rw = min(length_tmp, sizeof(buffer));
			cmt_fio->Fread(buffer, length_rw, 1);
			state_fio->Fwrite(buffer, length_rw, 1);
			length_tmp -= length_rw;
		}
	} else {
		state_fio->FputInt32(0);
	}
	state_fio->FputInt32(cmt_bufptr);
	state_fio->FputInt32(cmt_bufcnt);
	state_fio->Fwrite(cmt_buffer, sizeof(cmt_buffer), 1);
	state_fio->Fwrite(cmt_data_carrier, sizeof(cmt_data_carrier), 1);
	state_fio->FputInt32(cmt_data_carrier_cnt);
	state_fio->FputInt32(cmt_register_id);
	state_fio->FputBool(beep_on);
	state_fio->FputBool(beep_signal);
	state_fio->FputBool(sing_signal);
#ifdef SUPPORT_PC88_PCG8100
	state_fio->FputUint16(pcg_addr);
	state_fio->FputUint8(pcg_data);
	state_fio->FputUint8(pcg_ctrl);
	state_fio->Fwrite(pcg_pattern, sizeof(pcg_pattern), 1);
#endif
#ifdef NIPPY_PATCH
	state_fio->FputBool(nippy_patch);
#endif
#ifdef SDL
	// version 1.00
	state_fio->FputInt32(beep_event_id);
	state_fio->FputUint32(xm8_ext_flags);

	// version 1.20
	state_fio->FputInt32(gvram_access_count);
#endif // SDL
}

bool PC88::load_state(FILEIO* state_fio)
{
	uint32 version;

	release_tape();
	
	version = state_fio->FgetUint32();
	if (version > STATE_VERSION_170) {
		return false;
	}
	if(state_fio->FgetInt32() != this_device_id) {
		return false;
	}
	state_fio->Fread(ram, sizeof(ram), 1);
#if defined(PC88_EXRAM_BANKS)
	state_fio->Fread(exram, sizeof(exram), 1);
#endif
	state_fio->Fread(gvram, sizeof(gvram), 1);
	state_fio->Fread(tvram, sizeof(tvram), 1);
	state_fio->Fread(port, sizeof(port), 1);
	state_fio->Fread(&crtc, sizeof(crtc), 1);
	state_fio->Fread(&dmac, sizeof(dmac), 1);
	state_fio->Fread(alu_reg, sizeof(alu_reg), 1);
	gvram_plane = state_fio->FgetUint8();
	gvram_sel = state_fio->FgetUint8();
	cpu_clock_low = state_fio->FgetBool();
	mem_wait_on = state_fio->FgetBool();
	m1_wait_clocks = state_fio->FgetInt32();
	mem_wait_clocks_r = state_fio->FgetInt32();
	mem_wait_clocks_w = state_fio->FgetInt32();
	tvram_wait_clocks_r = state_fio->FgetInt32();
	tvram_wait_clocks_w = state_fio->FgetInt32();
	gvram_wait_clocks_r = state_fio->FgetInt32();
	gvram_wait_clocks_w = state_fio->FgetInt32();
	busreq_clocks = state_fio->FgetInt32();
	state_fio->Fread(palette, sizeof(palette), 1);
	update_palette = state_fio->FgetBool();
	hireso = state_fio->FgetBool();
	state_fio->Fread(text, sizeof(text), 1);
	state_fio->Fread(graph, sizeof(graph), 1);
	state_fio->Fread(palette_text_pc, sizeof(palette_text_pc), 1);
#ifdef SDL
	if (version < STATE_VERSION_170) {
		state_fio->Fread(palette_graph_pc, sizeof(scrntype), 8);
		palette_graph_pc[8] = palette_text_pc[8];
	}
	else {
		state_fio->Fread(palette_graph_pc, sizeof(palette_graph_pc), 1);
	}
#else
	state_fio->Fread(palette_graph_pc, sizeof(palette_graph_pc), 1);
#endif // SDL
	usart_dcd = state_fio->FgetBool();
	opn_busy = state_fio->FgetBool();
	key_caps = state_fio->FgetUint8();
	key_kana = state_fio->FgetUint8();
#ifdef SUPPORT_PC88_JOYSTICK
	mouse_strobe_clock = state_fio->FgetUint32();
	mouse_strobe_clock_lim = state_fio->FgetUint32();
	mouse_phase = state_fio->FgetInt32();
	mouse_dx = state_fio->FgetInt32();
	mouse_dy = state_fio->FgetInt32();
	mouse_lx = state_fio->FgetInt32();
	mouse_ly = state_fio->FgetInt32();
#endif
	intr_req = state_fio->FgetUint8();
	intr_req_sound = state_fio->FgetBool();
	intr_mask1 = state_fio->FgetUint8();
	intr_mask2 = state_fio->FgetUint8();
	cmt_play = state_fio->FgetBool();
	cmt_rec = state_fio->FgetBool();
	state_fio->Fread(rec_file_path, sizeof(rec_file_path), 1);
	int length_tmp = state_fio->FgetInt32();
	if(cmt_rec) {
		cmt_fio->Fopen(rec_file_path, FILEIO_READ_WRITE_NEW_BINARY);
		while(length_tmp != 0) {
			uint8 buffer[1024];
			int length_rw = min(length_tmp, sizeof(buffer));
			state_fio->Fread(buffer, length_rw, 1);
			if(cmt_fio->IsOpened()) {
				cmt_fio->Fwrite(buffer, length_rw, 1);
			}
			length_tmp -= length_rw;
		}
	}
	cmt_bufptr = state_fio->FgetInt32();
	cmt_bufcnt = state_fio->FgetInt32();
	state_fio->Fread(cmt_buffer, sizeof(cmt_buffer), 1);
	state_fio->Fread(cmt_data_carrier, sizeof(cmt_data_carrier), 1);
	cmt_data_carrier_cnt = state_fio->FgetInt32();
	cmt_register_id = state_fio->FgetInt32();
	beep_on = state_fio->FgetBool();
	beep_signal = state_fio->FgetBool();
	sing_signal = state_fio->FgetBool();
#ifdef SUPPORT_PC88_PCG8100
	pcg_addr = state_fio->FgetUint16();
	pcg_data = state_fio->FgetUint8();
	pcg_ctrl = state_fio->FgetUint8();
	state_fio->Fread(pcg_pattern, sizeof(pcg_pattern), 1);
#endif
#ifdef NIPPY_PATCH
	nippy_patch = state_fio->FgetBool();
#endif
#ifdef SDL
	// version 1.00
	beep_event_id = state_fio->FgetInt32();
	xm8_ext_flags = state_fio->FgetUint32();

	if (version >= STATE_VERSION_120) {
		// version 1.20
		gvram_access_count = state_fio->FgetInt32();
	}
	else {
		gvram_access_count = 0;
	}
#endif // SDL
	
	// post process
	dmac.mem = dmac.ch[2].io = this;
	dmac.ch[0].io = dmac.ch[1].io = dmac.ch[3].io = vm->dummy;
#ifdef SDL
	update_memmap(-1, true);
	update_memmap(-1, false);
#else
	update_low_memmap();
	update_tvram_memmap();
#endif // SDL
	return true;
}

