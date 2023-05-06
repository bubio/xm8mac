/*
	Skelton for retropc emulator

	Author : Takeda.Toshiya
	Date   : 2006.08.18 -

	[ win32 emulation i/f ]
*/

#include "common.h"
#include "emu.h"
#include "emu_sdl.h"

EMU::EMU(EMU_SDL *wrapper)
{
	emu_sdl = wrapper;

	// initialize input data
	memset(key_status, 0, sizeof(key_status));
	memset(joy_status, 0, sizeof(joy_status));
	memset(mouse_status, 0, sizeof(mouse_status));
}

_TCHAR* EMU::application_path(void)
{
	return emu_sdl->get_app_path();
}

_TCHAR* EMU::bios_path(_TCHAR* file_name)
{
	return emu_sdl->get_bios_path(file_name);
}

void EMU::get_host_time(cur_time_t* time)
{
	emu_sdl->get_host_time(time);
}

void EMU::printer_out(uint8 value)
{
	emu_sdl->printer_out(value);
}

void EMU::printer_strobe(bool value)
{
	emu_sdl->printer_strobe(value);
}

void EMU::mute_sound()
{
	emu_sdl->mute_sound();
}

scrntype* EMU::screen_buffer(int y)
{
	return emu_sdl->get_screen_buf(y);
}

void EMU::out_debug_log(const _TCHAR* format, ...)
{
}

void EMU::Sleep(uint32 ms)
{
	EMU_SDL::current_thread_sleep(ms);
}

void EMU::set_key_buffer(uint8 *status)
{
	memcpy(key_status, status, sizeof(key_status));
}

void EMU::set_joy_buffer(uint32 *status)
{
	memcpy(joy_status, status, sizeof(joy_status));
}
