//
// eXcellent Multi-platform emulator type 8 - 'XM8'
// based on ePC-8801MA
//
// Author (ePC-8801MA) : Takeda.Toshiya
// Author (XM8) : Tanaka.Yasushi
//
// [ SDL emulation i/f ]
//

#ifdef SDL

#ifndef EMU_SDL_H
#define EMU_SDL_H

#include "common.h"

class Video;

class EMU_SDL
{
public:
	EMU_SDL(Video *v);
										// constructor
	virtual ~EMU_SDL();
										// destructor
	_TCHAR* get_app_path(void);
										// retreive exection path for 2608_*.WAV
	_TCHAR* get_bios_path(_TCHAR *file_name);
										// create full path with specified BIOS file name
	void get_host_time(cur_time_t *time);
										// get local calendar on host
	void printer_out(uint8 value);
										// output one character to virtual printer
	void printer_strobe(bool value);
										// control strobe line on virtual printer
	void mute_sound(void);
										// disable mixing sound in this cycle
	scrntype* get_screen_buf(int y);
										// get frame buffer pointer according to y position
	static void current_thread_sleep(uint32 ms);
										// thread sleep (OS specific)
private:
	_TCHAR base_path[_MAX_PATH * 3];
										// application base path provieded by SDL
	_TCHAR bios_path[_MAX_PATH * 3];
										// full path buffer for get_bios_path()
	Video *video;
										// video driver
};

#endif // EMU_SDL_H

#endif // SDL
