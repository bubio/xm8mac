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
#include "clidisk.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#ifdef XM8_ENABLE_RETROACHIEVEMENTS
#include "ra_library.h"
#include "ra_media_store.h"
#include "ra_overlay.h"
#include "ra_service.h"
#endif

const char* GetAppVersionString();

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
	bool Init(const CliOptions& options);
										// initialize
	void Deinit();
										// deinitialize

	// get component
	Setting* GetSetting();
										// get Setting instance
	Platform* GetPlatform();
										// get platform instance
	Audio* GetAudio();
										// get Audio instance
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
	void RememberDiskOpenDir(const char *path);
										// remember user-selected disk directory
	bool OpenDiskFromMenu(const DiskSpec& spec, std::string *error);
										// open disk from menu
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
#ifdef XM8_ENABLE_RETROACHIEVEMENTS
	bool IsRaModeEnabled() const;
										// get RA mode setting
	bool ToggleRaMode();
										// toggle RA mode setting
	bool OpenRaLoginOverlay();
										// open RA password login overlay
	void OpenRaLibraryOverlay();
										// open RA library overlay
	void OpenRaAchievementsOverlay();
										// open RA achievements overlay
	void OpenRaLeaderboardsOverlay();
										// open RA leaderboards overlay
	void CloseRaOverlayToMenu();
										// close RA overlay and return to RA menu
	bool IsRaLoggedIn() const;
										// get RA login state
	void LogoutRa();
										// logout RA
	void GetRaMenuStatus(char *buffer, size_t capacity) const;
										// get RA status text for menu
	uint32_t ReadRaInspectionMemory(uint32_t address, uint8_t *buffer,
		uint32_t num_bytes) const;
										// read current VM memory for RA
#endif

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
	bool ApplyCommandLineSettings(const CliOptions& options);
										// apply temporary CLI settings
	void RestoreCommandLineSettings();
										// restore persistent settings
	bool ProbeDisk(const DiskSpec& spec, int *banks, std::string *error);
										// validate disk specification
	bool OpenDiskFromUser(const DiskSpec& spec, std::string *error);
										// open one disk
#ifdef XM8_ENABLE_RETROACHIEVEMENTS
	bool ResolveDiskForRaMode(const DiskSpec& spec, DiskSpec *resolved,
		std::string *ra_hash_to_identify, int64_t *ra_game_to_identify,
		std::string *error);
										// resolve disk to RA working copy
	bool EnsureRaService(std::string *error);
										// create RA service if needed
	bool SaveRaModeSetting(bool enabled, std::string *error);
										// persist RA mode setting
	bool BeginRaSavedTokenLogin(bool notify_missing_token);
										// begin saved token login if possible
	void StartRaAfterBoot();
										// start RA login and mounted media after boot restore
	void BeginRaSessionForMedia(const std::string& md5, int64_t game_id);
										// begin RA session for media hash
	void BeginRaSessionForMountedDrive1();
										// begin RA session for mounted drive 1
	void ProcessRaService(bool emulation_frame);
										// progress RA service
	void ProcessRaImages();
										// progress RA badge image HTTP
	void RequestRaBadgeImage(const std::string& url);
										// request RA badge image if needed
	void DrawRaBadgeImage(Uint32 *buf, SDL_Rect *rect,
		const std::string& url);
										// draw RA badge image if cached
	Xm8Ra::RaOverlayLibraryListSnapshot MakeRaLibraryOverlaySnapshot() const;
										// build library overlay snapshot
	bool LaunchRaLibraryGame(int64_t game_id, std::string *error);
										// launch registered RA library game
	Xm8Ra::RaOverlayAchievementListSnapshot MakeRaAchievementsOverlaySnapshot() const;
										// build achievements overlay snapshot
	Xm8Ra::RaOverlayLeaderboardListSnapshot MakeRaLeaderboardsOverlaySnapshot() const;
										// build leaderboards overlay snapshot
	void RefreshRaAchievementsOverlay();
										// refresh achievements overlay if visible
	void AddRaNotice(const std::string& text);
										// add RA overlay notice
	void AddRaEventsAsNotices(const std::vector<Xm8Ra::RaEvent>& events);
										// translate RA events to notices
	bool HandleRaOverlayKeyDown(SDL_Event *e);
										// handle RA overlay key input
	bool HandleRaOverlayTextInput(SDL_Event *e);
										// handle RA overlay text input
	bool HandleRaOverlayMouse(SDL_Event *e);
										// handle RA overlay mouse input
	bool HandleRaOverlayFinger(SDL_Event *e);
										// handle RA overlay touch input
	bool HandleRaOverlayJoystick();
										// handle RA overlay joystick input
	bool HandleRaOverlayAction(Xm8Ra::RaOverlayAction action);
										// handle RA overlay action
	void UpdateRaOverlayTextInput();
										// update SDL text input for RA overlay
	void ClearRaOverlayPointerState();
										// clear RA overlay pointer state
	bool SubmitRaOverlayLogin();
										// submit RA overlay login form
	void DrawRaOverlay();
										// draw RA notice overlay
#endif
	bool IsRaOverlayBlocking() const;
										// check blocking RA overlay
	bool OpenStartupDisks(const std::vector<DiskSpec>& disks, std::string *error);
										// open CLI disks
	bool OpenDroppedDisk(const char *path, std::string *error);
										// open D&D disk

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
	std::string disk_open_dir;
										// last user-selected disk directory
#ifdef XM8_ENABLE_RETROACHIEVEMENTS
	Xm8Ra::RaLibrary *ra_library;
										// RA library database
	Xm8Ra::RaMediaStore *ra_media_store;
										// RA media store
	Xm8Ra::RaService *ra_service;
										// RA client service
	Xm8Ra::RaOverlay *ra_overlay;
										// RA overlay state
	std::unique_ptr<Xm8Ra::RaHttpClient> ra_image_http_client;
										// RA image HTTP client
	struct RaBadgeImage {
		enum State {
			NotRequested,
			Pending,
			Ready,
			Failed
		};
		State state = NotRequested;
		uint64_t request_id = 0;
		int width = 0;
		int height = 0;
		std::vector<uint32_t> pixels;
	};
	std::map<std::string, RaBadgeImage> ra_badge_images;
										// RA badge image memory cache
	uint64_t ra_next_image_request_id;
										// RA image request id allocator
	bool ra_mode_enabled;
										// RA mode setting
	bool ra_saved_login_started;
										// saved token login started
	bool ra_manual_login_started;
										// manual password login started
	bool ra_session_disabled;
										// RA disabled for current launch session
	Uint32 ra_overlay_joystick_prev;
										// RA overlay joystick previous state
	bool ra_overlay_mouse_target_valid;
										// RA mouse pressed target valid
	Xm8Ra::RaOverlayLoginTarget ra_overlay_mouse_target;
										// RA mouse pressed target
	bool ra_overlay_finger_target_valid;
										// RA touch pressed target valid
	Xm8Ra::RaOverlayLoginTarget ra_overlay_finger_target;
										// RA touch pressed target
	bool ra_overlay_finger_scroll_valid;
										// RA touch scroll tracking valid
	bool ra_overlay_finger_scrolled;
										// RA touch moved enough to scroll
	int ra_overlay_finger_scroll_y;
										// RA touch scroll anchor y
	uint32_t ra_overlay_auto_scroll_revision;
										// RA auto-scroll selected text revision
	Uint32 ra_overlay_auto_scroll_started;
										// RA auto-scroll selected text start tick
	std::string ra_pending_game_hash;
										// media hash pending RA load
	int64_t ra_pending_library_game_id;
										// library game pending RA identification
	std::string ra_loaded_game_hash;
										// media hash passed to RA load
#endif

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
	bool startup_disk_boot;
										// boot from CLI disk
	bool cli_system_override;
										// CLI system override active
	bool cli_clock_override;
										// CLI clock override active
	bool cli_settings_restored;
										// CLI settings already restored
	int cli_original_system;
										// persistent system mode
	int cli_original_clock;
										// persistent CPU clock
	bool cli_original_8h;
										// persistent 8MHzH mode

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
