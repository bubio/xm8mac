//
// eXcellent Multi-Platform emulator type 8 - 'XM8'
// based on ePC-8801MA
//
// Author (ePC-8801MA) : Takeda.Toshiya
// Author (XM8) : Tanaka.Yasushi
//
// [ platform dependent ]
//

#ifdef SDL

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif // _WIN32

#include "os.h"
#include "common.h"
#include "classes.h"
#include "app.h"
#include "converter.h"
#include "platform.h"

#if defined(__linux__) && !defined(__ANDROID__)
#include <locale.h>
#endif // __linux__ && !__ANDROID__

#ifdef __linux__
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif // __linux__

//
// defines
//
#define LOCALE_JP_UTF8			"ja_JP.UTF-8"
										// Japanese and UTF-8
#define LOCALE_C_UTF8			"C.UTF-8"
										// Standard C and UTF-8
#define LOCALE_UTF8				"UTF-8"
										// UTF-8

//
// Platform()
// constructor
//
Platform::Platform(App *a)
{
	// save application
	app = a;

#ifdef _WIN32
	// initialize
	mutex_handle = NULL;
	find_data = NULL;
	find_handle = (void*)((HANDLE)INVALID_HANDLE_VALUE);
	find_root = false;
	find_drive = 0;

	// disable input method (it needs to be called before creating window)
	ImmDisableIME((DWORD)-1);
#endif // _WIN32

#ifdef __linux__
	dir_handle = NULL;
	dir_name[0] = '\0';
	dir_up = false;
#endif // __linux__
}

//
// ~Platform()
// destructor
//
Platform::~Platform()
{
	Deinit();
}

//
// Init()
// initialize
//
bool Platform::Init(SDL_Window *window)
{
#if defined(_WIN32) && defined(UNICODE)
	HANDLE hMutex;
	HINSTANCE hInstance;
	HWND hWnd;
	HANDLE hImage;
	SDL_SysWMinfo info;

	// create mutex
	hMutex = CreateMutexW(NULL, TRUE, L"XM8");
	if (hMutex == NULL) {
		return false;
	}
	if (GetLastError() == ERROR_ALREADY_EXISTS) {
		return false;
	}
	mutex_handle = (void*)hMutex;

	// get current module instance
	hInstance = GetModuleHandle(NULL);

	// get window handle
	SDL_VERSION(&info.version);
	if (SDL_GetWindowWMInfo(window, &info) == SDL_TRUE) {
		hWnd = info.info.win.window;

		// load icon
		hImage = LoadImage( hInstance,
							MAKEINTRESOURCE(100),
							IMAGE_ICON,
							0,
							0,
							LR_DEFAULTCOLOR);

		// set icon
		if ((hWnd != NULL) && (hImage != NULL)) {
			SendMessage(hWnd, WM_SETICON, ICON_SMALL, (LPARAM)hImage);
		}

		// load icon (big)
		hImage = LoadImage( hInstance,
							MAKEINTRESOURCE(101),
							IMAGE_ICON,
							0,
							0,
							LR_DEFAULTCOLOR);

		// set icon
		if ((hWnd != NULL) && (hImage != NULL)) {
			SendMessage(hWnd, WM_SETICON, ICON_BIG, (LPARAM)hImage);
		}
	}

	// allocate WIN32_FIND_DATA
	find_data = (void*)SDL_malloc(sizeof(WIN32_FIND_DATAW));
	if (find_data == NULL) {
		return false;
	}
#endif // _WIN32 && UNICODE

#if defined(__linux__) && !defined(__ANDROID__)
	char *opaque;

	// set locale to UTF-8
	opaque = setlocale(LC_CTYPE, LOCALE_JP_UTF8);
	if (opaque == NULL) {
		opaque = setlocale(LC_CTYPE, LOCALE_C_UTF8);
		if (opaque == NULL) {
			opaque = setlocale(LC_CTYPE, LOCALE_UTF8);
		}
	}
#endif // __linux__ && !__ANDROID__

	return true;
}

//
// Deinit()
// deinitialize
//
void Platform::Deinit()
{
#ifdef _WIN32
	// find handle
	if ((HANDLE)(find_handle) != INVALID_HANDLE_VALUE) {
		FindClose((HANDLE)find_handle);
		find_handle = (void*)((HANDLE)INVALID_HANDLE_VALUE);
	}

	// find data
	if (find_data != NULL) {
		SDL_free(find_data);
		find_data = NULL;
	}

	// mutex handle
	if (mutex_handle != NULL) {
		CloseHandle((HANDLE)mutex_handle);
		mutex_handle = NULL;
	}
#endif // _WIN32

#ifdef __linux__
	if (dir_handle != NULL) {
		closedir((DIR*)dir_handle);
		dir_handle = NULL;
	}
#endif // __linux__
}

//
// FindFirst()
// find first file
//
const char* Platform::FindFirst(const char *dir, Uint32 *info)
{
#if defined(_WIN32) && defined(UNICODE)
	wchar_t wide_dir[MAX_PATH];
	LPWIN32_FIND_DATAW win32_find_data;
	HANDLE handle;
	DWORD drives;
	int loop;

	// check root
	if ((dir[1] == ':') && (dir[2] == '\\') && (dir[3] == '\0')) {
		find_root = true;
		find_drive = 0;
	}
	else {
		find_root = false;
	}

	// UTF-8 to UNICODE
	MultiByteToWideChar(CP_UTF8,
						0,
						dir,
						-1,
						wide_dir,
						SDL_arraysize(wide_dir));
	wcscat_s(wide_dir, SDL_arraysize(wide_dir), L"*.*");

	// FindFirst
	win32_find_data = (LPWIN32_FIND_DATAW)find_data;
	handle = FindFirstFileW(wide_dir, win32_find_data);

	// result
	if (handle != INVALID_HANDLE_VALUE) {
		// save handle
		find_handle = (void*)handle;

		// UNICODE to SHIFT-JIS
		WideCharToMultiByte(932,
							0,
							win32_find_data->cFileName,
							-1,
							find_name,
							SDL_arraysize(find_name),
							NULL,
							NULL);

		// save info
		*info = (Uint32)win32_find_data->dwFileAttributes;

		// directory ?
		if ((win32_find_data->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
			strcat_s(find_name, SDL_arraysize(find_name), "\\");
		}

		if (strcmp(find_name, ".\\") == 0) {
			return FindNext(info);
		}
		else {
			return find_name;
		}
	}

	if (find_root == true) {
		// get valid drive bit map
		drives = GetLogicalDrives();

		// search valid drive
		for (loop=0; loop<32; loop++) {
			if ((drives & 1) != 0) {
				find_name[0] = (char)('A' + loop);
				find_name[1] = ':';
				find_name[2] = '\\';
				find_name[3] = '\0';
				*info = FILE_ATTRIBUTE_DIRECTORY;
				find_drive = loop + 1;
				return find_name;
			}
			drives >>= 1;
		}
	}

	return NULL;
#endif // _WIN32 && UNICODE

#ifdef __linux__
	DIR *dir_ret;

	// Find ..
	dir_up = FindUp(dir);

	// open directory
	dir_ret = opendir(dir);
	if (dir_ret == NULL) {
		return NULL;
	}

	// save
	dir_handle = (void*)dir_ret;

	// ..
	if (dir_up == true) {
		strcpy(dir_name, "../");
		*info = DT_DIR;
		return dir_name;
	}

	// find next
	return FindNext(info);
#endif // __liunx__
}

//
// FindNext()
// find next file
//
const char* Platform::FindNext(Uint32 *info)
{
#if defined(_WIN32) && defined(UNICODE)
	HANDLE handle;
	LPWIN32_FIND_DATAW win32_find_data;
	BOOL result;
	DWORD drives;
	int loop;

	// FindNext
	win32_find_data = (LPWIN32_FIND_DATAW)find_data;
	handle = (HANDLE)find_handle;
	if (handle != INVALID_HANDLE_VALUE) {
		result = FindNextFileW((HANDLE)find_handle, win32_find_data);

		if (result == FALSE) {
			// fail
			FindClose(handle);
			handle = INVALID_HANDLE_VALUE;
			find_handle = (void*)handle;
		}
		else {
			// success
			WideCharToMultiByte(932,
								0,
								win32_find_data->cFileName,
								-1,
								find_name,
								SDL_arraysize(find_name),
								NULL,
								NULL);

			// save info
			*info = (Uint32)win32_find_data->dwFileAttributes;

			// directory ?
			if ((win32_find_data->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
				strcat_s(find_name, SDL_arraysize(find_name), "\\");
			}

			return find_name;
		}
	}

	if (find_root == true) {
		// get valid drive bit map
		drives = GetLogicalDrives();

		// skip drives
		for (loop=0; loop<find_drive; loop++) {
			drives >>= 1;
		}

		// search valid drive
		for (loop=0; loop<32; loop++) {
			if ((drives & 1) != 0) {
				find_name[0] = (char)('A' + find_drive + loop);
				find_name[1] = ':';
				find_name[2] = '\\';
				find_name[3] = '\0';
				*info = FILE_ATTRIBUTE_DIRECTORY;
				find_drive += (loop + 1);
				return find_name;
			}
			drives >>= 1;
		}
	}

	return NULL;
#endif // _WIN32 && UNICODE

#ifdef __linux__
	struct dirent *entry;
	Converter *converter;

	// get converter
	converter = app->GetConverter();

	// find
	for (;;) {
		entry = readdir((DIR*)dir_handle);
		if (entry == NULL) {
			closedir((DIR*)dir_handle);
			dir_handle = NULL;
			return NULL;
		}

		// check .
		if (entry->d_name[0] != '.') {
			break;
		}
	}

	// name
	converter->UtfToSjis(entry->d_name, dir_name);

	// directory ?
	if (entry->d_type == DT_DIR) {
		if (dir_name[strlen(dir_name) - 1] != '/') {
			strcat(dir_name, "/");
		}
	}

	// type
	*info = (Uint32)entry->d_type;

	return dir_name;
#endif // __liunx__
}

#ifdef __linux__
//
// FindUp()
// find ..
//
bool Platform::FindUp(const char *dir)
{
	struct dirent *entry;
	DIR *dir_ret;

	// root ?
	if (strcmp(dir, "/") == 0) {
		return false;
	}

	// open directory
	dir_ret = opendir(dir);
	if (dir_ret == NULL) {
		return false;
	}

	// find loop
	for (;;) {
		entry = readdir(dir_ret);
		if (entry == NULL) {
			break;
		}

		// check '..'
		if (strcmp(entry->d_name, "..") == 0) {
			if (entry->d_type == DT_DIR) {
				closedir(dir_ret);
				return true;
			}
		}
	}

	// close and false
	closedir(dir_ret);
	return false;
}
#endif // __liunx__

//
// IsDir()
// check directory
//
bool Platform::IsDir(Uint32 info)
{
#ifdef _WIN32
	DWORD dwFileAttributes;

	// get file attributes
	dwFileAttributes = (DWORD)info;

	// directory ?
	if ((dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
		return true;
	}
	else {
		return false;
	}
#endif // _WIN32

#ifdef __linux__
	if (info == DT_DIR) {
		return true;
	}
	else {
		return false;
	}
#endif // __linux__
}

//
// MakePath()
// make path name from dir(UTF-8) and name(SHIFT-JIS)
//
bool Platform::MakePath(char *dir, const char *name)
{
#if defined(_WIN32) && defined(UNICODE)
	wchar_t wide_dir[MAX_PATH];
	wchar_t wide_name[MAX_PATH];
	size_t len_dir;
	size_t len_name;
	LPWSTR part;

	// UTF-8 to UNICODE
	MultiByteToWideChar(CP_UTF8,
						0,
						dir,
						-1,
						wide_dir,
						SDL_arraysize(wide_dir));

	// SHIFT-JIS to UNICODE
	MultiByteToWideChar(932,
						0,
						name,
						-1,
						wide_name,
						SDL_arraysize(wide_name));

	// check length
	len_dir = wcslen(wide_dir);
	len_name = wcslen(wide_name);
	if ((len_dir + len_name) >= MAX_PATH) {
		return false;
	}

	// cat
	if ((wide_name[1] == L':') && (wide_name[2] == L'\\') && (wide_name[3] == L'\0')) {
		wcscpy_s(wide_dir, SDL_arraysize(wide_dir), wide_name);
	}
	else {
		wcscat_s(wide_dir, SDL_arraysize(wide_dir), wide_name);
	}

	// GetFullPathName
	GetFullPathName(wide_dir, SDL_arraysize(wide_name), wide_name, &part);

	// UNICODE to UTF-8
	WideCharToMultiByte(CP_UTF8,
						0,
						wide_name,
						-1,
						dir,
						_MAX_PATH * 3,
						NULL,
						NULL);

	return true;
#endif // _WIN32 && UNICODE

#ifdef __linux__
	Converter *converter;
	struct stat filestat;

	// get converter
	converter = app->GetConverter();

	// SHIFT-JIS to UTF-8
	converter->SjisToUtf(name, dir_name);

	// cat
	strcat(dir, dir_name);

	// realpath
	if (realpath(dir, dir_name) == NULL) {
		return false;
	}

	// directory ?
	if (stat(dir_name, &filestat) == 0) {
		if (S_ISDIR(filestat.st_mode)) {
			if (dir_name[strlen(dir_name) - 1] != '/') {
				strcat(dir_name, "/");
			}
		}
	}

	// realpath to dir
	strcpy(dir, dir_name);

	return true;
#endif // __linux__
}

//
// GetFileDateTime()
// get file date and time
//
bool Platform::GetFileDateTime(const char *name, cur_time_t *cur_time)
{
#ifdef _WIN32
	wchar_t wide_name[MAX_PATH];
	LPWIN32_FIND_DATAW win32_find_data;
	HANDLE handle;
	SYSTEMTIME st;
	SYSTEMTIME lt;

	// UTF-8 to UNICODE
	MultiByteToWideChar(CP_UTF8,
						0,
						name,
						-1,
						wide_name,
						SDL_arraysize(wide_name));
	
	// FindFirst
	win32_find_data = (LPWIN32_FIND_DATAW)find_data;
	handle = FindFirstFileW(wide_name, win32_find_data);
	if (handle != INVALID_HANDLE_VALUE) {
		// success
		FindClose(handle);

		// convert FILETIME to localtime
		FileTimeToSystemTime(&(win32_find_data->ftLastWriteTime), &st);
		SystemTimeToTzSpecificLocalTime(NULL, &st, &lt);

		cur_time->year = (int)lt.wYear;
		cur_time->month = (int)lt.wMonth;
		cur_time->day = (int)lt.wDay;
		cur_time->day_of_week = (int)lt.wDayOfWeek;
		cur_time->hour = (int)lt.wHour;
		cur_time->minute = (int)lt.wMinute;
		cur_time->second = (int)lt.wSecond;

		return true;
	}

	return false;
#endif // _WIN32

#ifdef __linux__
	struct stat filestat;
	time_t timep;
	struct tm local_time;

	if (stat(name, &filestat) == 0) {
		// get last modified date at filestat.st_mtime
		timep = (time_t)filestat.st_mtime;
		tzset();
		localtime_r(&timep, &local_time);

		cur_time->year = local_time.tm_year + 1900;
		cur_time->month = local_time.tm_mon + 1;
		cur_time->day = local_time.tm_mday;
		cur_time->day_of_week = local_time.tm_wday;
		cur_time->hour = local_time.tm_hour;
		cur_time->minute = local_time.tm_min;
		cur_time->second = local_time.tm_sec;

		return true;
	}

	return false;
#endif // __linux__
}

//
// MsgBox()
// modal message box
//
void Platform::MsgBox(SDL_Window *window, const char *string)
{
#ifdef _WIN32
	SDL_SysWMinfo info;
	wchar_t message[_MAX_PATH * 2];
	wchar_t title[0x80];

	// initialize SDL_SysWMinfo structure
	SDL_VERSION(&info.version);

	if (SDL_GetWindowWMInfo(window, &info) == SDL_TRUE) {
		// UTF-8 to UNICODE
		MultiByteToWideChar(CP_UTF8,
							0,
							app->GetAppTitle(),
							-1,
							title,
							SDL_arraysize(title));
		MultiByteToWideChar(CP_UTF8,
							0,
							string,
							-1,
							message,
							SDL_arraysize(message));

		MessageBoxW(info.info.win.window, message, title, MB_ICONERROR | MB_OK);
	}
#endif // _WIN32

#if defined(__linux__) && !defined(__ANDROID__)
	SDL_MessageBoxData data;
	SDL_MessageBoxButtonData button;
	int retid;

	// button data
	button.flags = SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT;
	button.buttonid = 1;
	button.text = "OK";

	// message box data
	SDL_zero(data);
	data.flags = SDL_MESSAGEBOX_ERROR;
	data.window = window;
	data.title = app->GetAppTitle();
	data.message = string;
	data.numbuttons = 1;
	data.buttons = &button;
	data.colorScheme = NULL;

	// show modal message box
	SDL_ShowMessageBox(&data, &retid);
#endif // __linux__ && !__ANDROID__
}

//
// CheckMouseButton
// check mouse left button
//
bool Platform::CheckMouseButton()
{
#ifdef _WIN32
	int  swap;
	SHORT button;

	// GetSystemMetrics
	swap = GetSystemMetrics(SM_SWAPBUTTON);
	if (swap == 0) {
		// left
		button = GetAsyncKeyState(VK_LBUTTON);
	}
	else {
		// right
		button = GetAsyncKeyState(VK_RBUTTON);
	}

	if ((button & 0x8000) != 0) {
		return true;
	}
	else {
		return false;
	}
#else
	return true;
#endif // _WIN32
}

#endif // SDL
