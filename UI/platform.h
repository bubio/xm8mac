//
// eXcellent Multi-platform emulator type 8 - 'XM8'
// based on ePC-8801MA
//
// Author (ePC-8801MA) : Takeda.Toshiya
// Author (XM8) : Tanaka.Yasushi
//
// [ platform dependent ]
//

#ifdef SDL

#ifndef PLATFORM_H
#define PLATFORM_H

//
// platform dependent
//
class Platform
{
public:
	Platform(App *a);
										// constructor
	virtual ~Platform();
										// destructor
	bool Init(SDL_Window *window);
										// initialize
	void Deinit();
										// deinitialize

	// search file
	const char* FindFirst(const char *dir, Uint32 *info);
										// find first file
	const char* FindNext(Uint32 *info);
										// find next file
	bool IsDir(Uint32 info);
										// check directory
	bool MakePath(char *dir, const char *name);
										// make path from dir(UTF-8) and name(SHIFT-JIS)

	// file date and time
	bool GetFileDateTime(const char *name, cur_time_t *cur_time);
										// get file date and time

	// message box
	void MsgBox(SDL_Window *window, const char *string);
										// modal message box

	bool CheckMouseButton();
										// check mouse left button

private:
	App *app;
										// app
#ifdef _WIN32
	void *mutex_handle;
										// HANDLE
	void *find_handle;
										// HANDLE
	void *find_data;
										// WIN32_FIND_DATA
	char find_name[_MAX_PATH * 3];
										// file name (shift-jis)
	bool find_root;
										// root flag
	int find_drive;
										// find drive count
#endif // _WIN32
#ifdef __linux__
	bool FindUp(const char *dir);
										// find ..
	void *dir_handle;
										// DIR *
	char dir_name[_MAX_PATH * 3];
										// file name (shift-jis)
	bool dir_up;
										// FindUp() result
#endif // __linux__
};

#endif // PLATFORM_H

#endif // SDL
