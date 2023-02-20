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

#include "os.h"
#include "common.h"
#include "vm.h"
#include "classes.h"
#include "video.h"
#include "emu_sdl.h"

//
// defines
//
#define EXTERNAL_PATH_ANDROID		"Android/data/"
								// SDL_AndroidGetExtrernalStoragePath()
#define EXTERNAL_PATH_ROM			"XM8/"
								// replace from EXTERNAL_PATH_ANDROID

//
// EMU_SDL()
// constructor
//
EMU_SDL::EMU_SDL(Video *v)
{
#ifdef __ANDROID__
	char *replace;
#endif	// __ANDROID__

	// save video driver
	SDL_assert(v != NULL);
	video = v;

	// save base path
#ifdef __ANDROID__
	strcpy(base_path, SDL_AndroidGetExternalStoragePath());
	replace = strstr(base_path, EXTERNAL_PATH_ANDROID);
	if (replace != NULL) {
		strcpy(replace, EXTERNAL_PATH_ROM);
	}
#else
	strcpy(base_path, SDL_GetBasePath());
#endif // __ANDROID__
}

//
// ~EMU_SDL()
// destructor
//
EMU_SDL::~EMU_SDL()
{
}

//
// get_app_path()
// retreive exection path for 2608_*.WAV
//
_TCHAR* EMU_SDL::get_app_path()
{
	SDL_assert(base_path != NULL);
	return base_path;
}

//
// get_bios_path()
// create full path with specified BIOS file name
//
_TCHAR* EMU_SDL::get_bios_path(_TCHAR *file_name)
{
	SDL_assert(file_name != NULL);
	SDL_assert(base_path != NULL);

	// overflow check
	if ((strlen(base_path) + strlen(file_name)) >= sizeof(bios_path)) {
		SDL_assert(false);
		return file_name;
	}

	// create path
	strcpy(bios_path, base_path);
	strcat(bios_path, file_name);

	return bios_path;
}

//
// get_host_time()
// get local calendar on host
// !!! caution: sizeof(time_t) depends on compiler !!! (aka 'year 2038 problem')
//
void EMU_SDL::get_host_time(cur_time_t *emu_time)
{
	struct tm *local_time;
	time_t time_32bit_or_64bit;

	time_32bit_or_64bit = time(NULL);
	local_time = localtime(&time_32bit_or_64bit);

	// all clear
	memset(emu_time, 0, sizeof(cur_time_t));

	// set member
	emu_time->year = local_time->tm_year + 1900;
	emu_time->month = local_time->tm_mon + 1;
	emu_time->day = local_time->tm_mday;
	emu_time->day_of_week = local_time->tm_wday;
	emu_time->hour = local_time->tm_hour;
	emu_time->minute = local_time->tm_min;
	emu_time->second = local_time->tm_sec;
}

//
// printer_out()
// output one character to virtual printer
//
void EMU_SDL::printer_out(uint8 value)
{
}

//
// printer_strobe()
// control strobe line on virtual printer
//
void EMU_SDL::printer_strobe(bool value)
{
}

//
// mute_sound()
// disable mixing sound in this cycle
//
void EMU_SDL::mute_sound(void)
{
}

//
// get_screen_buf()
// get frame buffer pointer according to y position
//
scrntype* EMU_SDL::get_screen_buf(int y)
{
	return (scrntype*)video->GetFrameBuf((uint32)y);
}

//
// current_thread_sleep()
// 
void EMU_SDL::current_thread_sleep(uint32 ms)
{
	SDL_Delay((Uint32)ms);
}

#endif // SDL
