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

#include "os.h"
#include "common.h"
#include "classes.h"
#include "emu_sdl.h"
#include "emu.h"
#include "vm.h"
#include "event.h"
#include "upd1990a.h"
#include "fmsound.h"
#include "pc88.h"
#include "setting.h"
#include "platform.h"
#include "video.h"
#include "audio.h"
#include "font.h"
#include "input.h"
#include "converter.h"
#include "menu.h"
#include "menuid.h"
#include "diskmgr.h"
#include "tapemgr.h"
#ifdef __ANDROID__
#include "xm8jni.h"
#endif // __ANDROID__
#include "app.h"

//
// defines
//
#define APP_NAME				"XM8 (based on ePC-8801MA)";
										// application name
#define APP_VER					0x0170
										// version (BCD)
#define APP_WIDTH				SCREEN_WIDTH
										// window width
#define APP_HEIGHT_TRANSPARENT	SCREEN_HEIGHT
										// window height (transparent)
#define APP_HEIGHT_STATUS		(SCREEN_HEIGHT + 18)
										// window height (status line)
#define MS_SHIFT				16
										// float to uint shift (ex:0x10000=1ms)
#define SLEEP_MENU				50
										// delay on menu mode (ms)
#define SLEEP_POWERDOWN			1000
										// delay on power down (ms)
#define FORCE_SYNC				500
										// force synchronize (ms)
#define SKIP_FRAMES_MAX			15
										// max skip frames without drawing
#define SKIP_FRAMES_FULL		12
										// skip frames with full speed
#define PLATFORM_IOS			"iOS"
										// platform name (iOS)
#define PLATFORM_ANDROID		"Android"
										// platform name (Android)
#define PLATFORM_WINDOWS		"Windows"
										// platform name (Windows)
#define COUNT_PER_POWERINFO		10
										// main loop count per SDL_GetPowerInfo
#define POWERDOWN_LEVEL			10
										// enter power-down state (%)
#define STATE_FILENAME			"state%d.bin"
										// state file name
#define MOUSE_INFINITE_TIME		20000
										// mouse infinite time (ms)

//
// App()
// constructor
//
App::App()
{
	int drive;

	SDL_assert(SDL_arraysize(diskmgr) == MAX_DRIVE);

	// component
	vm_sem = NULL;
	setting = NULL;
	window = NULL;
	platform = NULL;
	video = NULL;
	audio = NULL;
	font = NULL;
	input = NULL;
	converter = NULL;
	menu = NULL;
	for (drive=0; drive<MAX_DRIVE; drive++) {
		diskmgr[drive] = NULL;
	}
	tapemgr = NULL;
	wrapper = NULL;
	emu = NULL;
	vm = NULL;
	evmgr = NULL;
	pc88 = NULL;
	upd1990a = NULL;

	// flags
	app_quit = false;
	app_fullspeed = false;
	app_fullscreen = false;
	app_background = false;
	app_mobile = false;
	app_menu = false;
	app_powerdown = false;
	app_forcesync = false;
	power_counter = 0;
	power_pointer = 0;
	memset(power_level, 0, sizeof(power_level));

	// frame rate
	memset(draw_tick, 0, sizeof(draw_tick));
	draw_tick_count = 0;
	draw_tick_point = 0;

	// mouse cursor
	mouse_tick = 0;

	// system information
	system_info = 0;

	// state path
	state_path[0] = '\0';

	// audio parameter
	audio_param = NULL;
	audio_opened = false;
}

//
// ~App()
// destructor
//
App::~App()
{
	Deinit();
}

//
// Init()
// initialize
//
bool App::Init()
{
	Audio::OpenParam param;
	int width;
	int height;
	int loop;
	const char *name;

#ifdef __ANDROID__
	// check skip flag
	if (Android_ChkSkipMain() != 0) {
		return false;
	}
#endif // __ANDROID__

	// get platform
	name = SDL_GetPlatform();
	if (strcmp(name, PLATFORM_IOS) == 0) {
		// iOS platform
		app_mobile = true;
	}
	if (strcmp(name, PLATFORM_ANDROID) == 0) {
		// Android platform
		app_mobile = true;
	}

	// semaphore
	if (app_mobile == true) {
		vm_sem = SDL_CreateSemaphore(1);
		if (vm_sem == NULL) {
			Deinit();
			return false;
		}
	}

	// setting
	setting = new Setting;
	if (setting->Init() == false) {
		Deinit();
		return false;
	}

	// spcfiy scaling quality (all platforms)
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, setting->GetScaleQuality());

	// platform (1)
	platform = new Platform(this);

	// window
	width = setting->GetWindowWidth();
	if (setting->HasStatusLine() == true) {
		height = (width * APP_HEIGHT_STATUS) / APP_WIDTH;
	}
	else {
		height = (width * APP_HEIGHT_TRANSPARENT) / APP_WIDTH;
	}
	window = SDL_CreateWindow(  GetAppTitle(),
								SDL_WINDOWPOS_UNDEFINED,
								SDL_WINDOWPOS_UNDEFINED,
								width,
								height,
								SDL_WINDOW_SHOWN | SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_MOUSE_FOCUS);
	if (window == NULL) {
		Deinit();
		return false;
	}

	// enable drag and drop
	SDL_EventState(SDL_DROPFILE, SDL_ENABLE);

	// platform (2)
	if (platform->Init(window) == false) {
		Deinit();
		return false;
	}

	// video
	video = new Video(this);
	if (video->Init(window) == false) {
		Deinit();
		return false;
	}

	// audio
	audio = new Audio;
	if (audio->Init() == false) {
		Deinit();
		return false;
	}

	// audio parameter
	audio_param = (Uint8*)SDL_malloc(sizeof(Audio::OpenParam));
	if (audio_param == NULL) {
		Deinit();
		return false;
	}

	// emulator i/f wrapper
	wrapper = new EMU_SDL(video);

	// emulator i/f
	emu = new EMU(wrapper);

	// font
	font = new Font(this);
	if (font->Init(window) == false) {
		Deinit();
		return false;
	}

	// input
	input = new Input(this);
	if (input->Init() == false) {
		Deinit();
		return false;
	}

	// converter
	converter = new Converter;
	if (converter->Init() == false) {
		Deinit();
		return false;
	}

	// menu
	menu = new Menu(this);
	if (menu->Init() == false) {
		Deinit();
		return false;
	}

	// open audio device
	param.device = setting->GetAudioDevice();
	param.freq = setting->GetAudioFreq();
	param.samples = 1 << setting->GetAudioPower();
	param.buffer = setting->GetAudioBuffer();
	param.per = (setting->GetAudioUnit() * param.freq + 500) / 1000;
	if (audio->Open(&param) == false) {
		Deinit();
		return false;
	}

	// save audio parameter
	audio_opened = true;
	memcpy(audio_param, &param, sizeof(param));

	// create virtual machine
	vm = new VM(emu);
	vm->initialize_sound(param.freq, param.per);
	vm->reset();

	// event manager
	evmgr = (EVENT*)vm->get_device(1);

	// PC88 device
	pc88 = (PC88*)vm->get_device(2);

	// rtc device
	upd1990a = (UPD1990A*)vm->get_device(6);

	// disk manager
	for (loop=0; loop<2; loop++) {
		diskmgr[loop] = new DiskManager;
		if (diskmgr[loop]->Init(vm, loop) == false) {
			Deinit();
			return false;
		}
	}

	// tape manager
	tapemgr = new TapeManager;
	if (tapemgr->Init(vm) == false) {
		Deinit();
		return false;
	}

	// set window size
	SDL_GetWindowSize(window, &width, &height);
	video->SetWindowSize(width, height);

	// power management
	for (loop=0; loop<SDL_arraysize(power_level); loop++) {
		power_level[loop] = 100;
	}
	power_counter = 0;
	power_pointer = 0;
	app_powerdown = false;

	// start virtual machine
	app_quit = false;
	app_fullspeed = false;
	app_background = false;
	app_menu = false;
	CtrlAudio();

	// mouse cursor
	mouse_tick = SDL_GetTicks();
	SDL_ShowCursor(SDL_ENABLE);

	// system information
	system_info = setting->GetSystems();

#ifdef __ANDROID__
	// poll joystick for second launch (see SDL_SYS_JoystickDetect())
	Android_PollJoystick();
#endif // __ANDROID__

	return true;
}

//
// Deinit()
// deinitialize
//
void App::Deinit()
{
	int drive;

#ifdef __ANDROID__
	// check skip flag
	if (Android_ChkSkipMain() != 0) {
		return;
	}
#endif // __ANDROID__

	// tape manager
	if (tapemgr != NULL) {
		tapemgr->Deinit();
		delete tapemgr;
		tapemgr = NULL;
	}

	// disk manager
	for (drive=0; drive<MAX_DRIVE; drive++) {
		if (diskmgr[drive] != NULL) {
			diskmgr[drive]->Deinit();
			delete diskmgr[drive];
			diskmgr[drive] = NULL;
		}
	}

	// virtual machine
	if (vm != NULL) {
		delete vm;
		vm = NULL;
	}

	// menu
	if (menu != NULL) {
		menu->Deinit();
		delete menu;
		menu = NULL;
	}

	// converter
	if (converter != NULL) {
		converter->Deinit();
		delete converter;
		converter = NULL;
	}

	// input
	if (input != NULL) {
		input->Deinit();
		delete input;
		input = NULL;
	}

	// font
	if (font != NULL) {
		font->Deinit();
		delete font;
		font = NULL;
	}

	// emulator i/f
	if (emu != NULL) {
		delete emu;
		emu = NULL;
	}

	// emulator i/f wrapper
	if (wrapper != NULL) {
		delete wrapper;
		wrapper = NULL;
	}

	// audio parameter
	if (audio_param != NULL) {
		SDL_free(audio_param);
		audio_param = NULL;
	}

	// audio
	if (audio != NULL) {
		audio->Deinit();
		delete audio;
		audio = NULL;
		audio_opened = false;
	}

	// video
	if (video != NULL) {
		video->Deinit();
		delete video;
		video = NULL;
	}

	// platform (2)
	if (platform != NULL) {
		platform->Deinit();
	}

	// window
	if (window != NULL) {
		SDL_DestroyWindow(window);
		window = NULL;
	}

	// platform (1)
	if (platform != NULL) {
		delete platform;
		platform = NULL;
	}

	// setting
	if (setting != NULL) {
		setting->Deinit();
		delete setting;
		setting = NULL;
	}

	// semaphore
	if (vm_sem != NULL) {
		SDL_DestroySemaphore(vm_sem);
		vm_sem = NULL;
	}
}

//
// GetPlatform()
// get Platform instance
//
Platform* App::GetPlatform()
{
	SDL_assert(platform != NULL);
	return platform;
}

//
// GetSetting()
// get Setting instance
//
Setting* App::GetSetting()
{
	SDL_assert(setting != NULL);
	return setting;
}

//
// GetVideo()
// get Video instance
//
Video* App::GetVideo()
{
	SDL_assert(video != NULL);
	return video;
}

//
// GetFont()
// get Font instance
//
Font* App::GetFont()
{
	SDL_assert(font != NULL);
	return font;
}

//
// GetInput()
// get Input instance
//
Input* App::GetInput()
{
	SDL_assert(input != NULL);
	return input;
}

//
// GetConverter()
// get Converter instance
//
Converter* App::GetConverter()
{
	SDL_assert(converter != NULL);
	return converter;
}

//
// GetMenu()
// get Menu instance
//
Menu* App::GetMenu()
{
	SDL_assert(menu != NULL);
	return menu;
}

//
// GetWrapper()
// get EMU_SDL instance
//
EMU_SDL* App::GetWrapper()
{
	SDL_assert(wrapper != NULL);
	return wrapper;
}

//
// GetEmu()
// get EMU instance
//
EMU* App::GetEmu()
{
	SDL_assert(emu != NULL);
	return emu;
}

//
// GetDiskManager()
// get DiskManager instance array
//
DiskManager** App::GetDiskManager()
{
	SDL_assert(diskmgr[MAX_DRIVE - 1] != NULL);
	return diskmgr;
}

//
// GetTapeManager()
// get TapeManager instance
//
TapeManager* App::GetTapeManager()
{
	SDL_assert(tapemgr != NULL);
	return tapemgr;
}

//
// Run()
// running application
//
void App::Run()
{
	int ret;
	SDL_Event e;
	double rate;
	double ms_per_frame;
	Uint32 begin;
	Uint32 add;
	Uint32 total;
	Uint32 diff;
	int run;
	int extra;
	Uint8 *buffer;
	int buffer_samples;
	int buffer_evmgr;
	int buffer_pct;
	int normskip;
	int fullskip;

	// initialize
	begin = SDL_GetTicks();
	run = 0;
	rate = vm->frame_rate();
	ms_per_frame = (1000.0 * (1 << MS_SHIFT)) / rate;
	add = (Uint32)ms_per_frame;
	total = 0;
	normskip = 0;
	fullskip = 0;

#ifdef __ANDROID__
	// android intent
	if (ProcessIntent() == false) {
		// load state 0 (auto)
		Load(0);

		// enter menu
		EnterMenu(MENU_MAIN);
	}
#else
	// load state 0 (auto)
	Load(0);

	// enter menu
	EnterMenu(MENU_MAIN);
#endif // __ANDROID__

	// main loop
	while (app_quit == false) {
		// stop virtual machine or menu
		if ((app_menu == true) || (app_background == true) || (app_powerdown == true)) {
			// draw
			if ((app_mobile != true) || (app_background != true)) {
				// no draw if app_mobile && app_background
				if (app_menu == true) {
					menu->Draw();
				}
				Draw();
			}

			// wait until event
			if (app_background == true) {
				// background -> wait infinite
				ret = SDL_WaitEvent(&e);
			}
			else {
				if (app_menu == true) {
					// menu
					ret = SDL_WaitEventTimeout(&e, SLEEP_MENU);
				}
				else {
					// power down
					ret = SDL_WaitEventTimeout(&e, SLEEP_POWERDOWN);
				}
			}

			// poll event
			while (ret != 0) {
				Poll(&e);
				if (app_quit == true) {
					break;
				}
				ret = SDL_PollEvent(&e);
			}
			if (app_quit == true) {
				continue;
			}

			// process menu
			if (app_menu == true) {
				menu->ProcessMenu();
			}

			// power management
			PowerMng();

			// clear timing control
			begin = SDL_GetTicks();
			total = 0;
			draw_tick_count = 0;
			draw_tick_point = 0;
			continue;
		}

		// tick diff (1)
		if (app_fullspeed == true) {
			// full speed
			begin = SDL_GetTicks();
			total = 0;
			diff = 0;
		}
		else {
			// normal speed
			diff = (SDL_GetTicks() - begin) << MS_SHIFT;
			diff -= total;
		}

		// force sync
		if (app_forcesync == true) {
			diff = (FORCE_SYNC << MS_SHIFT) + 1;
			app_forcesync = false;
		}
		if (diff < 0x80000000) {
			if (diff > (FORCE_SYNC << MS_SHIFT)) {
				begin = SDL_GetTicks();
				total = 0;
				diff = 0;
			}
		}
		else {
			if (diff < (Uint32)(0 - (FORCE_SYNC << MS_SHIFT))) {
				begin = SDL_GetTicks();
				total = 0;
				diff = 0;
			}
		}

		// run ?
		if (diff < 0x80000000) {
			// prepare to run virtual machine
			extra = 0;
			LockVM();

			// run virtual machine and write audio samples
			diskmgr[0]->ProcessMgr();
			diskmgr[1]->ProcessMgr();
			buffer = (Uint8*)evmgr->create_sound32(&extra);
			buffer_evmgr = evmgr->sound_buffer_ptr();
			buffer_samples = audio->GetFreeSamples();
			if (app_fullspeed == true) {
				// full speed
				if (buffer_samples >= buffer_evmgr) {
					buffer_pct = audio->Write(buffer, buffer_evmgr);
					evmgr->set_sample_multi(0x1000);
				}
				evmgr->create_sound32_after(buffer_evmgr);
			}
			else {
				// normal speed
				if (buffer_samples < buffer_evmgr) {
					diff = SDL_GetTicks();
					while (buffer_samples < buffer_evmgr) {
						// buffer underrun
						UnlockVM();
						SDL_Delay(1);
						LockVM();
						buffer_samples = audio->GetFreeSamples();
					}
					total += (Uint32)(SDL_GetTicks() - diff) << MS_SHIFT;
				}
				buffer_pct = audio->Write(buffer, buffer_evmgr);
				evmgr->set_sample_multi(multi_table[buffer_pct >> 4]);
				evmgr->create_sound32_after(buffer_evmgr);
			}
			UnlockVM();

			// calc next time
			if (rate != vm->frame_rate()) {
				rate = vm->frame_rate();
				ms_per_frame = (1000.0 * (1 << MS_SHIFT)) / rate;
				add = (Uint32)ms_per_frame;
			}

			// add executed ms
			for (ret=0; ret<extra; ret++) {
				total += add;
			}

			// add executed frames
			run += extra;
		}

		// poll event
		for (;;) {
			ret = SDL_PollEvent(&e);
			if (ret == 0) {
				break;
			}
			Poll(&e);
			if (app_quit == true) {
				break;
			}
		}
		if (app_quit == true) {
			continue;
		}

		// power management
		PowerMng();

		// tick diff (2)
		if (app_fullspeed == true) {
			// full speed
			begin = SDL_GetTicks();
			total = 0;
			diff = 0;
		}
		else {
			// normal speed
			diff = (SDL_GetTicks() - begin) << MS_SHIFT;
		}

		// wait?
		if (run > 0) {
			// have rest time or skiped over maximum frames
			if ((diff <= total) || (run >= SKIP_FRAMES_MAX)) {
				// reset frame
				run = 0;

				// rendering
				if (app_fullspeed == true) {
					// full speed
					fullskip++;
					if (fullskip >= SKIP_FRAMES_FULL) {
						fullskip = 0;
						Draw();
					}
				}
				else {
					// normal speed
					if ((run >= SKIP_FRAMES_MAX) || (normskip >= setting->GetSkipFrame())) {
						// calculation frame rate
						draw_tick[draw_tick_point] = SDL_GetTicks();
						draw_tick_point++;
						if (draw_tick_point >= SDL_arraysize(draw_tick)) {
							draw_tick_point = 0;
						}
						if (draw_tick_count < SDL_arraysize(draw_tick)) {
							draw_tick_count++;
						}

						// drawing
						Draw();
						normskip = 0;
					}
					else {
						// skip farme
						normskip++;
					}
				}
			}
		}
		else {
			// do nothing. wait until next frame
			if (diff < total) {
				ret = SDL_WaitEventTimeout(&e, (total - diff) >> MS_SHIFT);
				while (ret != 0) {
					Poll(&e);
					if (app_quit == true) {
						break;
					}
					ret= SDL_PollEvent(&e);
				}
				if (app_quit == true)  {
					continue;
				}
			}
		}

		// revise
		if (total >= 0x40000000) {
			begin += (0x10000000 >> MS_SHIFT);
			total -= 0x10000000;
		}

		// mouse cursor
		if ((Uint32)(SDL_GetTicks() - mouse_tick) >= setting->GetMouseTime()) {
			if (setting->GetMouseTime() < MOUSE_INFINITE_TIME) {
				if (SDL_GetMouseState(NULL, NULL) == 0) {
					if (SDL_ShowCursor(SDL_QUERY) == SDL_ENABLE) {
						SDL_ShowCursor(SDL_DISABLE);
					}
				}
			}
		}

		// softkey
		input->ProcessList();
		input->DelayedBreak();

		// sleep 0 if full speed
		if (app_fullspeed == true) {
			SDL_Delay(0);
		}
	}

	// save state 0 (auto)
	Save(0);
}

//
// Draw()
// rendering
//
void App::Draw()
{
	Uint32 info;
	int point;
	double rate;
	Uint32 urate;

	// system information
	info = setting->GetSystems();
	video->SetSystemInfo(info);

	// rendering
	vm->draw_screen();

	// calculate frame rate
	urate = 0;
	if (draw_tick_point == 0) {
		point = SDL_arraysize(draw_tick) - draw_tick_count;
		rate = (double)(10000.0 * (draw_tick_count - 1));
		if ((draw_tick[SDL_arraysize(draw_tick) - 1] - draw_tick[point]) != 0) {
			rate /= (draw_tick[SDL_arraysize(draw_tick) - 1] - draw_tick[point]);
			urate = (Uint32)rate;
			if (urate > 999) {
				urate = 999;
			}
		}
	}
	// inform frame rate of video driver
	if ((app_menu == true) || (app_background == true) || (app_powerdown == true) || (app_fullspeed == true)) {
		video->SetFrameRate(1000);
	}
	else {
		if (urate != 0) {
			video->SetFrameRate(urate);
		}
	}

	// drawing by video driver
	video->Draw();
}

//
// PowerMng()
// power management
//
void App::PowerMng()
{
	int pct;
	int loop;
	int avg;
	SDL_PowerState state;

	// setting (watch battery)
	if (setting->IsWatchBattery() == false) {
		// reset battery level
		for (loop=0; loop<SDL_arraysize(power_level); loop++) {
			power_level[loop] = 100;
		}
		power_counter = 0;

		if (app_powerdown == true) {
			app_powerdown = false;
			video->SetPowerDown(false);
			CtrlAudio();
		}
		return;
	}

	// power counter
	power_counter++;
	if (power_counter < COUNT_PER_POWERINFO) {
		return;
	}
	power_counter = 0;

	// get power information
	state = SDL_GetPowerInfo(NULL, &pct);

	if (state != SDL_POWERSTATE_ON_BATTERY) {
		// reset battery level
		for (loop=0; loop<SDL_arraysize(power_level); loop++) {
			power_level[loop] = 100;
		}
	}
	else {
		// record power level
		power_level[power_pointer] = pct;
		power_pointer++;
		if (power_pointer == SDL_arraysize(power_level)) {
			power_pointer = 0;
		}
	}

	// get average
	avg = 0;
	for (loop=0; loop<SDL_arraysize(power_level); loop++) {
		avg += power_level[loop];
	}
	avg /= SDL_arraysize(power_level);

	// check average power level
	if (avg <= POWERDOWN_LEVEL) {
		if (app_powerdown == false) {
			// power level is too low
			app_powerdown = true;
			video->SetPowerDown(true);
			CtrlAudio();
		}
	}
	else {
		if (app_powerdown == true) {
			app_powerdown = false;
			video->SetPowerDown(false);
			CtrlAudio();
		}
	}
}

#ifdef __ANDROID__
//
// ProcessIntent()
// process android intent
//
bool App::ProcessIntent()
{
	bool result;
	const char *intent;

	// check intent
	if (Android_HasIntent() == 0) {
		return false;
	}

	// get intent
	intent = Android_GetIntent();

	// drive 1
	diskmgr[0]->Close();
	result = diskmgr[0]->Open(intent, 0);

	// drive 2
	diskmgr[1]->Close();
	if (result == true) {
		if (diskmgr[0]->GetBanks() > 1) {
			diskmgr[1]->Open(intent, 1);
		}
	}

	// clear intent
	Android_ClearIntent();

	return result;
}
#endif // __ANDROID__

//
// Poll()
// poll event
//
void App::Poll(SDL_Event *e)
{
	// handle SDL events
	switch (e->type) {
	case SDL_QUIT:
		app_quit = true;
		break;

	case SDL_WINDOWEVENT:
		OnWindow(e);
		break;

	case SDL_MOUSEMOTION:
		if (e->motion.which != SDL_TOUCH_MOUSEID) {
			SDL_ShowCursor(SDL_ENABLE);
			mouse_tick = SDL_GetTicks();
		}
		if (app_menu == true) {
			// menu
			menu->OnMouseMotion(e);
		}
		else {
			// soft key
			input->OnMouseMotion(e);
		}
		break;

	case SDL_MOUSEBUTTONDOWN:
		if (e->button.which != SDL_TOUCH_MOUSEID) {
			SDL_ShowCursor(SDL_ENABLE);
			mouse_tick = SDL_GetTicks();
		}
		if (app_menu == true) {
			// menu
			menu->OnMouseButtonDown(e);
		}
		else {
			// enter menu ?
			if ((e->button.which != SDL_TOUCH_MOUSEID) && (e->button.button == SDL_BUTTON_RIGHT)) {
				// enter menu
				EnterMenu(MENU_MAIN);
			}
			else {
				// softkey
				input->OnMouseButtonDown(e);
			}
		}
		break;

	case SDL_MOUSEBUTTONUP:
		if (e->button.which != SDL_TOUCH_MOUSEID) {
			SDL_ShowCursor(SDL_ENABLE);
			mouse_tick = SDL_GetTicks();
		}
		if (app_menu == true) {
			// menu
			menu->OnMouseButtonUp(e);
		}
		else {
			// softkey
			input->OnMouseButtonUp(e);
		}
		break;

	case SDL_MOUSEWHEEL:
		if (e->wheel.which != SDL_TOUCH_MOUSEID) {
			SDL_ShowCursor(SDL_ENABLE);
			mouse_tick = SDL_GetTicks();
		}
		if (app_menu == true) {
			menu->OnMouseWheel(e);
		}
		else {
			// softkey
			input->OnMouseWheel(e);
		}
		break;

	case SDL_KEYDOWN:
		if (app_background == false) {
#ifdef __ANDROID__
			if (e->key.keysym.scancode == SDL_SCANCODE_AC_BACK) {
				OnKeyDown(e);
				break;
			}
#endif // __ANDROID__
			if (setting->IsKeyEnable() == true) {
				OnKeyDown(e);
			}
		}
		break;

	case SDL_KEYUP:
		if (app_background == false) {
#ifdef __ANDROID__
			if (e->key.keysym.scancode == SDL_SCANCODE_AC_BACK) {
				OnKeyUp(e);
				break;
			}
#endif // __ANDROID__
			if (setting->IsKeyEnable() == true) {
				OnKeyUp(e);
			}
		}
		break;

	case SDL_TEXTEDITING:
		break;

	case SDL_TEXTINPUT:
		break;

	case SDL_JOYAXISMOTION:
		if (app_menu == true) {
			menu->OnJoystick();
		}
		else {
			if (app_background == false) {
				input->OnJoystick();
			}
		}
		break;

	case SDL_JOYBALLMOTION:
		break;

	case SDL_JOYHATMOTION:
		break;

	case SDL_JOYBUTTONDOWN:
		if (app_menu == true) {
			menu->OnJoystick();
		}
		else {
			if (app_background == false) {
				input->OnJoystick();
			}
		}
		break;

	case SDL_JOYBUTTONUP:
		if (app_menu == true) {
			menu->OnJoystick();
		}
		else {
			if (app_background == false) {
				input->OnJoystick();
			}
		}
		break;

	case SDL_JOYDEVICEADDED:
		input->AddJoystick();
		break;

	case SDL_JOYDEVICEREMOVED:
		input->AddJoystick();
		break;

	case SDL_DROPFILE:
		OnDropFile(e);
		break;

	case SDL_FINGERDOWN:
		if (app_menu == true) {
			menu->OnFingerDown(e);
		}
		else {
			if (app_background == false) {
				input->OnFingerDown(e);
			}
		}
		break;

	case SDL_FINGERUP:
		if (app_menu == true) {
			menu->OnFingerUp(e);
		}
		else {
			if (app_background == false) {
				input->OnFingerUp(e);
			}
		}
		break;

	case SDL_FINGERMOTION:
		if (app_menu == true) {
			menu->OnFingerMotion(e);
		}
		else {
			if (app_background == false) {
				input->OnFingerMotion(e);
			}
		}
		break;

	case SDL_CLIPBOARDUPDATE:
		break;

	case SDL_RENDER_TARGETS_RESET:
		break;

	default:
		break;
	}
}

//
// OnWindow()
// window event
//
void App::OnWindow(SDL_Event *e)
{
	bool activate;
	bool inactivate;
	bool resize;
	bool lostfocus;
	int width;
	int height;
	Audio::OpenParam param;

	// init
	activate = false;
	inactivate = false;
	resize = false;
	lostfocus = false;

	// handle window event
	switch (e->window.event) {
	case SDL_WINDOWEVENT_SHOWN:
		activate = true;
		break;

	case SDL_WINDOWEVENT_HIDDEN:
		inactivate = true;
		lostfocus = true;
		break;

	case SDL_WINDOWEVENT_EXPOSED:
		activate = true;
		break;

	case SDL_WINDOWEVENT_MOVED:
		video->DrawCtrl();
		break;

	case SDL_WINDOWEVENT_RESIZED:
		resize = true;
		break;

	case SDL_WINDOWEVENT_SIZE_CHANGED:
		break;

	case SDL_WINDOWEVENT_MINIMIZED:
		inactivate = true;
		lostfocus = true;
		break;

	case SDL_WINDOWEVENT_MAXIMIZED:
		activate = true;
		resize = true;
		break;

	case SDL_WINDOWEVENT_RESTORED:
		resize = true;
		break;

	case SDL_WINDOWEVENT_ENTER:
		break;

	case SDL_WINDOWEVENT_LEAVE:
		break;

	case SDL_WINDOWEVENT_FOCUS_GAINED:
		activate = true;
		break;

	case SDL_WINDOWEVENT_FOCUS_LOST:
		lostfocus = true;
		break;

	case SDL_WINDOWEVENT_CLOSE:
		break;

	default:
		break;
	}

	// action (activate)
	if (activate == true) {
		// background -> foreground
		if (app_background == true) {
			// audio open
			if (audio_opened == false) {
				memcpy(&param, audio_param, sizeof(param));
				audio_opened = audio->Open(&param);
			}

			app_background = false;
			app_forcesync = true;
			CtrlAudio();

			// force next draw
			video->DrawCtrl();
		}
	}

	// action (inactivate)
	if (inactivate == true) {
		// foreground -> background
		if (app_background == false) {
			app_background = true;
			CtrlAudio();

			// audio close
			if (audio_opened == true) {
				audio->Close();
				audio_opened = false;
			}
		}
	}

	// action (resize)
	if (resize == true) {
		// window resize
		SDL_GetWindowSize(window, &width, &height);
		video->SetWindowSize(width, height);
	}

	// action (lostfocus)
	if (lostfocus == true) {
		// lost keyboard focus
		input->LostFocus();
	}
}

//
// OnKeyDown()
// key down event
//
void App::OnKeyDown(SDL_Event *e)
{
	SDL_Scancode code;

	// scancode
	code = e->key.keysym.scancode;

#ifndef __ANDROID__
	// check ALT (1)
	if ((e->key.keysym.mod & KMOD_ALT) != 0) {
		// ALT + ENTER
		if ((code == SDL_SCANCODE_RETURN) || (code == SDL_SCANCODE_KP_ENTER)) {
			if (e->key.repeat == 0) {
				// toggle screen
				if (app_fullscreen == false) {
					FullScreen();
				}
				else {
					WindowScreen();
				}
			}
			return;
		}
	}
#endif // !__ANDROID__

	// menu
	if (app_menu == true) {
		menu->OnKeyDown(e);
		return;
	}

	// check repeat
	if (e->key.repeat != 0) {
		return;
	}

#ifdef __ANDROID__
	// Android back key
	if (code == SDL_SCANCODE_AC_BACK) {
		EnterMenu(MENU_QUIT);
		return;
	}
#endif // __ANDROID__

	// check ALT (2)
	if ((e->key.keysym.mod & KMOD_ALT) != 0) {
		// ALT + F11
		if (code == SDL_SCANCODE_F11) {
			// toggle speed
			if (app_fullspeed == false) {
				FullSpeed();
			}
			else {
				NormalSpeed();
			}
			return;
		}
	}

	// F11
	if (code == SDL_SCANCODE_F11) {
		EnterMenu(MENU_MAIN);
		return;
	}

	// input
	input->OnKeyDown(false, code);
}

//
// OnKeyUp()
// key up event
//
void App::OnKeyUp(SDL_Event *e)
{
	SDL_Scancode code;

	// scancode
	code = e->key.keysym.scancode;

#ifndef __ANDROID__
	// check ALT (1)
	if ((e->key.keysym.mod & KMOD_ALT) != 0) {
		// ALT + ENTER
		if ((code == SDL_SCANCODE_RETURN) || (code == SDL_SCANCODE_KP_ENTER)) {
			return;
		}
	}
#endif // !__ANDROID__

	// menu
	if (app_menu == true) {
		return;
	}

#ifdef __ANDROID__
	// Android back key
	if (code == SDL_SCANCODE_AC_BACK) {
		return;
	}
#endif // __ANDROID__

	// check ALT (2)
	if ((e->key.keysym.mod & KMOD_ALT) != 0) {
		// ALT + F11
		if (code == SDL_SCANCODE_F11) {
			return;
		}
	}

	// F11
	if (code == SDL_SCANCODE_F11) {
		return;
	}

	// input
	input->OnKeyUp(false, code);
}

//
// OnDropFile()
// drag & drop event
//
void App::OnDropFile(SDL_Event *e)
{
	const char *src;
	char *dest;
	bool result;
	Uint8 high;
	Uint8 low;

	// initialize
	src = e->drop.file;
	dest = state_path;

	while (*src != '\0') {
		// Linux encodes UTF-8 string into '%hex' style
		if (*src == '%') {
			// get high and low
			high = (Uint8)src[1];
			if (high == '\0') {
				break;
			}
			low = (Uint8)src[2];
			if (low == '\0') {
				break;
			}
			src += 3;

			// high
			if ((high >= '0') && (high <= '9')) {
				high -= '0';
			}
			else {
				high |= 0x20;
				high -= 0x57;
			}
			high <<= 4;

			// low
			if ((low >= '0') && (low <= '9')) {
				low -= '0';
			}
			else {
				low |= 0x20;
				low -= 0x57;
			}

			*dest++ = (char)(high | low);
		}
		else {
			*dest++ = (char)*src++;
		}
	}

	// terminate
	*dest = '\0';

	// drive 1
	diskmgr[0]->Close();
	result = diskmgr[0]->Open(state_path, 0);

	// drive 2
	diskmgr[1]->Close();
	if (result == true) {
		if (diskmgr[0]->GetBanks() > 1) {
			diskmgr[1]->Open(state_path, 1);
		}
	}

	// free memory
	SDL_free(e->drop.file);

	if (result == true) {
		// leave menu
		LeaveMenu();

		// reset
		Reset();
	}
}

//
// OnKeyVM()
// caps and kana key to vm
//
void App::OnKeyVM(SDL_Scancode code)
{
	// CAPS(0x14)
	if (code == SDL_SCANCODE_CAPSLOCK) {
		vm->key_down(0x14, false);
	}

	// KANA(0x15)
	if (code == SDL_SCANCODE_SCROLLLOCK) {
		vm->key_down(0x15, false);
	}
}

//
// GetKeyVM()
// get key buffer from vm
//
void App::GetKeyVM(Uint8 *buf)
{
	pc88->get_key_status((uint8*)buf);
}

//
// GetKeyCode()
// get keycode from port and bit
//
Uint32 App::GetKeyCode(Uint32 port, Uint32 bit)
{
	return (Uint32)pc88->get_key_code((uint32)port, (uint32)bit);
}

//
// FullScreen()
// enter full screen
//
void App::FullScreen()
{
	int ret;
	int width;
	int height;

	// fake full screen mode
	ret = SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
	if (ret == 0) {
		app_fullscreen = true;

		// inform video of client rect
		SDL_GetWindowSize(window, &width, &height);
		video->SetWindowSize(width, height);

		// menu
		if (app_menu == true) {
			menu->UpdateMenu();
		}
	}
}

//
// WindowScreen()
// back to window screen mode
//
void App::WindowScreen()
{
	int ret;
	int width;
	int height;

	// restore window mode
	ret = SDL_SetWindowFullscreen(window, 0);
	if (ret == 0) {
		app_fullscreen = false;

		// inform video of client rect
		SDL_GetWindowSize(window, &width, &height);
		video->SetWindowSize(width, height);

		// menu
		if (app_menu == true) {
			menu->UpdateMenu();
		}
	}
}

//
// IsFullScreen()
// get full screen flag
//
bool App::IsFullScreen()
{
	return app_fullscreen;
}

//
// FullSpeed()
// enter full speed
//
void App::FullSpeed()
{
	app_fullspeed = true;
	video->SetFullSpeed(true);

	// menu
	if (app_menu == true) {
		menu->UpdateMenu();
	}
}

//
// NormalSpeed()
// back to normal speed
//
void App::NormalSpeed()
{
	// initialize frame rate
	memset(draw_tick, 0, sizeof(draw_tick));
	draw_tick_count = 0;
	draw_tick_point = 0;

	app_fullspeed = false;
	video->SetFullSpeed(false);

	// menu
	if (app_menu == true) {
		menu->UpdateMenu();
	}
}

//
// IsFullSpeed()
// get full speed flag
//
bool App::IsFullSpeed()
{
	return app_fullspeed;
}

//
// SetWindowWidth()
// set window width
//
void App::SetWindowWidth()
{
	int width;
	int height;

	width = setting->GetWindowWidth();
	if (setting->HasStatusLine() == true) {
		height = (width * APP_HEIGHT_STATUS) / APP_WIDTH;
	}
	else {
		height = (width * APP_HEIGHT_TRANSPARENT) / APP_WIDTH;
	}

	// resize window
	SDL_SetWindowSize(window, width, height);

	// resize video
	SDL_GetWindowSize(window, &width, &height);
	video->SetWindowSize(width, height);
}

//
// EnterMenu()
// enter menu mode
//
void App::EnterMenu(int id)
{
	// flag
	app_menu = true;

	// system information
	system_info = setting->GetSystems();

	// mouse cursor
	SDL_ShowCursor(SDL_ENABLE);

	// input
	input->LostFocus();

	// audio
	CtrlAudio();

	// video
	video->SetMenuMode(true);

	// menu
	menu->EnterMenu(id);
}

//
// LeaveMenu()
// restore run mode
//
void App::LeaveMenu(bool check)
{
	// flag
	app_menu = false;

	// mouse curosor
	mouse_tick = SDL_GetTicks();

	// input
	input->LostFocus();
	input->ResetList();

	// audio
	CtrlAudio();

	// video
	video->SetMenuMode(false);

	// system info
	if (check == true) {
		if (setting->GetSystems() != system_info) {
			// rebuild virtual machine
			system_info = setting->GetSystems();
			ChangeSystem();
		}
	}

	// resync rtc
	upd1990a->resync();
}

//
// CtrlAudio()
// control sound
//
void App::CtrlAudio()
{
	if ((app_background == false) && (app_menu == false) && (app_powerdown == false)) {
		audio->Play();
	}
	if ((app_background == true) || (app_menu == true) || (app_powerdown == true)) {
		audio->Stop();
	}
}

//
// ChangeAudio()
// change audio parameter
//
void App::ChangeAudio()
{
	Audio::OpenParam param;
	FMSound *fmsound;

	// close audio device
	audio->Close();

	// open audio device
	param.device = setting->GetAudioDevice();
	param.freq = setting->GetAudioFreq();
	param.samples = 1 << setting->GetAudioPower();
	param.buffer = setting->GetAudioBuffer();
	param.per = (setting->GetAudioUnit() * param.freq + 500) / 1000;

	// SDL audio
	if (audio->Open(&param) == true) {
		// release event manager
		evmgr->release();

		// set OPN/OPNA rate
		fmsound = (FMSound*)vm->get_device(7);
		fmsound->change_rate(param.freq);
#ifdef SUPPORT_PC88_PCG8100
		fmsound = (FMSound*)vm->get_device(17);
		fmsound->change_rate(param.freq);
#else
		fmsound = (FMSound*)vm->get_device(13);
		fmsound->change_rate(param.freq);
#endif // SUPPORT_PC88_PCG8100

		// initialize event manager
		evmgr->initialize_sound(param.freq, param.per);

		// save audio parameter
		memcpy(audio_param, &param, sizeof(param));
	}
}

//
// ChangeSystem()
// change system
//
void App::ChangeSystem(bool load)
{
	int drive;
	bool open[MAX_DRIVE];
	int bank[MAX_DRIVE];
	bool play;
	bool rec;

	// lock vm
	if (load == false) {
		LockVM();
	}

	// get current disk information
	for (drive=0; drive<MAX_DRIVE; drive++) {
		open[drive] = diskmgr[drive]->IsOpen();
		if (open[drive] == true) {
			bank[drive] = diskmgr[drive]->GetBank();
		}
	}

	// get current tape information
	play = tapemgr->IsPlay();
	rec = tapemgr->IsRec();

	// delete virtual machine
	delete vm;
	vm = NULL;

	// create virtual machine
	vm = new VM(emu);
	vm->initialize_sound(   setting->GetAudioFreq(),
							(setting->GetAudioUnit() * setting->GetAudioFreq()) / 1000);
	vm->reset();

	// get event manager
	evmgr = (EVENT*)vm->get_device(1);

	// get PC88 device
	pc88 = (PC88*)vm->get_device(2);

	// get rtc device
	upd1990a = (UPD1990A*)vm->get_device(6);

	// restore disk manager
	for (drive=0; drive<MAX_DRIVE; drive++) {
		diskmgr[drive]->SetVM(vm);
		if (open[drive] == true) {
			diskmgr[drive]->Open(bank[drive]);
		}
		else {
			diskmgr[drive]->Close();
		}
	}

	// restore tape manager
	tapemgr->SetVM(vm);
	if (play == true) {
		tapemgr->Play();
	}
	if (rec == true) {
		tapemgr->Rec();
	}

	// unlock vm
	if (load == false) {
		UnlockVM();
	}

	// restart audio
	if (audio->IsPlay() == true) {
		audio->Stop();
		audio->Play();
	}
}

//
// GetDiskDir()
// get disk dir
//
const char* App::GetDiskDir(int drive)
{
	// drive 1
	if ((drive == -1) || (drive == 0)) {
		if (diskmgr[0]->IsOpen() == true) {
			return diskmgr[0]->GetDir();
		}
	}

	// drive 2
	if ((drive == -1) || (drive == 1)) {
		if (diskmgr[1]->IsOpen() == true) {
			return diskmgr[1]->GetDir();
		}
	}

	// no open
	if (diskmgr[0]->IsOpen() == true) {
		return diskmgr[0]->GetDir();
	}
	if (diskmgr[1]->IsOpen() == true) {
		return diskmgr[1]->GetDir();
	}

	// application base path
	return (const char*)wrapper->get_app_path();
}

//
// GetTapeDir()
// get tape dir
//
const char* App::GetTapeDir()
{
	// play
	if (tapemgr->IsPlay() == true) {
		return tapemgr->GetDir();
	}

	// rec
	if (tapemgr->IsRec() == true) {
		return tapemgr->GetDir();
	}

	// application base path
	return (const char*)wrapper->get_app_path();
}

//
// Reset()
// reset virtual machine
//
void App::Reset()
{
	// virtual machine
	vm->reset();

	// resync rtc
	upd1990a->resync();

	// restart audio
	if (audio->IsPlay() == true) {
		audio->Stop();
		audio->Play();
	}
}

//
// Load()
// load state
//
bool App::Load(int slot)
{
	char name[64];
	FILEIO fileio;
	int freq;

	// get current freq
	freq = setting->GetAudioFreq();

	// lock vm
	LockVM();

	// state path
	sprintf(name, STATE_FILENAME, slot);
	strcpy(state_path, setting->GetSettingDir());
	strcat(state_path, name);

	// open
	if (fileio.Fopen(state_path, FILEIO_READ_BINARY) == true) {
		// settting
		if (setting->LoadSetting(&fileio) == true) {
			// rebuild vm
			ChangeSystem(true);

			// disk manager
			diskmgr[0]->Load(&fileio);
			diskmgr[1]->Load(&fileio);

			// tape manager
			tapemgr->Load(&fileio);

			// each device
			vm->load_state(&fileio);

			// close
			fileio.Fclose();

			// initialize frame rate
			memset(draw_tick, 0, sizeof(draw_tick));
			draw_tick_count = 0;
			draw_tick_point = 0;

			// audio
			if (setting->GetAudioFreq() != freq) {
				ChangeAudio();
			}
			else {
				if (audio->IsPlay() == true) {
					audio->Stop();
					audio->Play();
				}
			}

			// input
			input->ChangeList(false, false);

			// resync rtc
			upd1990a->resync();

			// success
			UnlockVM();
			return true;
		}
		else {
			// close
			fileio.Fclose();
		}
	}

	// unlock vm
	UnlockVM();

	// fail
	return false;
}

//
// Save()
// save state
//
bool App::Save(int slot)
{
	char name[64];
	FILEIO fileio;

	// lock vm
	LockVM();

	// state path
	sprintf(name, STATE_FILENAME, slot);
	strcpy(state_path, setting->GetSettingDir());
	strcat(state_path, name);

	// open
	if (fileio.Fopen(state_path, FILEIO_WRITE_BINARY) == true) {
		// settting
		setting->SaveSetting(&fileio);

		// disk manager
		diskmgr[0]->Save(&fileio);
		diskmgr[1]->Save(&fileio);

		// tape manager
		tapemgr->Save(&fileio);

		// each device
		vm->save_state(&fileio);

		// close
		fileio.Fclose();

		// success
		UnlockVM();
		return true;
	}

	// unlock vm
	UnlockVM();

	// fail
	return false;
}

//
// GetStateTime()
// get state time
//
bool App::GetStateTime(int slot, cur_time_t *cur_time)
{
	char name[64];

	// state path
	sprintf(name, STATE_FILENAME, slot);
	strcpy(state_path, setting->GetSettingDir());
	strcat(state_path, name);

	// platform
	return platform->GetFileDateTime(state_path, cur_time);
}

//
// Quit()
// quit application
//
void App::Quit()
{
	app_quit = true;
}

//
// LockVM()
// lock vm thread
//
void App::LockVM()
{
	if (vm_sem != NULL) {
		SDL_SemWait(vm_sem);
	}
}

//
// UnlockVM()
// unlock vm thread
//
void App::UnlockVM()
{
	if (vm_sem != NULL) {
		SDL_SemPost(vm_sem);
	}
}

//
// GetAppVersion()
// get application version
//
Uint32 App::GetAppVersion()
{
	return APP_VER;
}

//
// GetAppTitle()
// get application title in UTF-8 format
//
const char* App::GetAppTitle()
{
	return APP_NAME;
}

//
// GetEvMgr()
// get event manager
//
void* App::GetEvMgr()
{
	return (void*)evmgr;
}

//
// sample multiple table
//
const int App::multi_table[16] = {
	0x1078,
	0x1068,
	0x1058,
	0x1048,
// 25%
	0x1038,
	0x1028,
// 37.5%
	0x1018,
	0x1010,
// 50%
	0x1008,
	0x1004,
// 62.5%
	0x1000,
	0x0ffc,
// 75%
	0x0ff8,
	0x0ff0,
	0x0fe0,
	0x0fd0,
};

#endif // SDL
