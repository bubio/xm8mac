/*
	Skelton for retropc emulator

	Author : Takeda.Toshiya
	Date   : 2006.08.18 -

	[ win32 emulation i/f ]
*/

#ifndef _EMU_H_
#define _EMU_H_

#include "common.h"
#include "config.h"
#include "vm/vm.h"

#ifdef SDL
class EMU_SDL;
#endif // SDL

class EMU
{
private:
#ifdef SDL
	EMU_SDL *emu_sdl;
#endif // SDL

	// ----------------------------------------
	// input
	// ----------------------------------------
	uint8 key_status[256];	// windows key code mapping

	uint32 joy_status[2];	// joystick #1, #2 (b0 = up, b1 = down, b2 = left, b3 = right, b4- = buttons

	int mouse_status[3];	// x, y, button (b0 = left, b1 = right)

public:
#ifdef SDL
	EMU(EMU_SDL *wrapper);
#endif // SDL

	// ----------------------------------------
	// initialize
	// ----------------------------------------
	_TCHAR* application_path();

	_TCHAR* bios_path(_TCHAR* file_name);

	// sound
	void mute_sound();

	// ----------------------------------------
	// for virtual machine
	// ----------------------------------------

	// input device
	uint8* key_buffer()
	{
		return key_status;
	}
	uint32* joy_buffer()
	{
		return joy_status;
	}
	int* mouse_buffer()
	{
		return mouse_status;
	}

	// screen
	scrntype* screen_buffer(int y);
#ifdef USE_CRT_FILTER
	bool screen_skip_line;
#endif

	// timer
	void get_host_time(cur_time_t* time);

	// printer
	void printer_out(uint8 value);
	void printer_strobe(bool value);

	// debug log
	void out_debug_log(const _TCHAR* format, ...);

#ifdef SDL
	// SDL specific
	void set_key_buffer(uint8 *status);
	void set_joy_buffer(uint32 *status);
	static void Sleep(uint32 ms);
#endif // SDL
};

#endif
