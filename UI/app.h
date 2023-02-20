//
// eXcellent Multi-platform emulator type 8 - 'XM8'
// based on ePC-8801MA
//
// Author (ePC-8801MA) : Takeda.Toshiya
// Author (XM8) : Tanaka.Yasushi
//
// [ application ]
//

#ifdef SDL

#ifndef APP_H
#define APP_H

#include "classes.h"

//
// XM8 application
//
class App
{
public:
	App();
										// constructor
	~App();
										// destructor
	bool Init();
										// initialize
	void Deinit();
										// deinitialize

	// get component
	Setting* GetSetting();
										// get Setting instance
	Platform* GetPlatform();
										// get platform instance
	Video* GetVideo();
										// get Video instance
	Font* GetFont();
										// get Font instance
	Input* GetInput();
										// get Input instance
	Converter* GetConverter();
										// get Converter instance
	Menu* GetMenu();
										// get Menu instance
	EMU_SDL* GetWrapper();
										// get EMU_SDL instance
	EMU* GetEmu();
										// get EMU instance
	DiskManager** GetDiskManager();
										// get DiskManager instance array
	TapeManager* GetTapeManager();
										// get TapeManager instance

	// run
	void Run();
										// run

	// mode
	void FullScreen();
										// full screen
	void WindowScreen();
										// window screen
	bool IsFullScreen();
										// get full screen flag
	void FullSpeed();
										// full speed
	void NormalSpeed();
										// normal speed
	bool IsFullSpeed();
										// get full speed flag
	void SetWindowWidth();
										// set window width

	// action
	void OnKeyVM(SDL_Scancode code);
										// key down to vm
	void GetKeyVM(Uint8 *buf);
										// get key buffer from vm
	Uint32 GetKeyCode(Uint32 port, Uint32 bit);
										// get keycode from vm
	void EnterMenu(int id);
										// menu mode
	void LeaveMenu(bool check = true);
										// run mode
	void ChangeAudio();
										// change audio parameter
	void ChangeSystem(bool load = false);
										// change system
	const char* GetDiskDir(int drive = -1);
										// get disk dir
	const char* GetTapeDir();
										// get tape dir
	void Reset();
										// reset
	bool Load(int slot);
										// load state
	bool Save(int slot);
										// save state
	bool GetStateTime(int slot, cur_time_t *cur_time);
										// get state time
	void Quit();
										// quit

	// misc
	Uint32 GetAppVersion();
										// get version
	const char* GetAppTitle();
										// get application title
	void* GetEvMgr();
										// get event manager

private:
	// drawing
	void Draw();
										// rendering
	double GetFrameRate();
										// calculate frame rate

	// power management
	void PowerMng();
										// power management

#ifdef __ANDROID__
	bool ProcessIntent();
										// process intent
#endif // __ANDROID__

	// event
	void Poll(SDL_Event *e);
										// poll event
	void OnWindow(SDL_Event *e);
										// window event
	void OnKeyDown(SDL_Event *e);
										// key down event
	void OnKeyUp(SDL_Event *e);
										// key up event
	void OnDropFile(SDL_Event *e);
										// drag & drop event

	// mode
	void CtrlAudio();
										// control audio

	// sync
	void LockVM();
										// lock vm
	void UnlockVM();
										// unlock vm

	// component
	SDL_sem *vm_sem;
										// semaphore object
	Setting *setting;
										// setting driver
	SDL_Window *window;
										// window
	Platform *platform;
										// platform driver
	Video *video;
										// video driver
	Audio *audio;
										// audio driver
	Font *font;
										// font manager
	Input *input;
										// input driver
	Converter *converter;
										// unicode converter
	Menu *menu;
										// menu driver
	EMU_SDL *wrapper;
										// emulator i/f wrapper
	EMU *emu;
										// emulator i/f
	VM *vm;
										// virtual machine
	EVENT *evmgr;
										// event manager
	PC88 *pc88;
										// PC88 device
	UPD1990A *upd1990a;
										// rtc device
	DiskManager *diskmgr[2];
										// disk manager
	TapeManager *tapemgr;
										// tape manager

	// flags
	bool app_quit;
										// application quit flag
	bool app_fullspeed;
										// application full speed flag
	bool app_fullscreen;
										// application full screen flag
	bool app_background;
										// application back ground mode
	bool app_mobile;
										// mobile platform flag
	bool app_menu;
										// application menu flag
	bool app_powerdown;
										// application power down flag
	bool app_forcesync;
										// application force synchronize flag

	// power management
	int power_counter;
										// power counter
	int power_pointer;
										// power pointer
	int power_level[4];
										// power level

	// frame rate
	Uint32 draw_tick[0x40];
										// calculate frame rate
	Uint32 draw_tick_count;
										// calculate frame rate
	int draw_tick_point;
										// calculate frame rate

	// mouse
	Uint32 mouse_tick;
										// mouse timeout (ms)

	// system
	Uint32 system_info;
										// system information

	// state path
	char state_path[_MAX_PATH * 3];
										// state path

	// sound sample multiple table
	static const int multi_table[16];
										// multiple table

	// audio parameter
	Uint8 *audio_param;
										// Audio::OpenParam
	bool audio_opened;
										// audio open flag
};

#endif // APP_H

#endif // SDL
