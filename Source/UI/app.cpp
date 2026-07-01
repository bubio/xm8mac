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

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

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
#include "clidisk.h"
#ifdef XM8_ENABLE_RETROACHIEVEMENTS
#include "ra_build_info.h"
#include "ra_http_mac.h"
#include "ra_paths.h"
#endif
#ifdef __ANDROID__
#include "xm8jni.h"
#endif // __ANDROID__
#include "app.h"

//
// defines
//
#define APP_NAME				"XM8 (based on ePC-8801MA)";
										// application name
#define APP_VER					0x0179
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

namespace {

int HexValue(char ch)
{
	if (ch >= '0' && ch <= '9') {
		return ch - '0';
	}
	ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
	if (ch >= 'a' && ch <= 'f') {
		return ch - 'a' + 10;
	}
	return -1;
}

bool DecodeDropPath(const char *source, char *destination, size_t capacity,
	std::string *error)
{
	size_t output = 0;

	if (source == NULL || destination == NULL || capacity == 0) {
		*error = "invalid dropped file path";
		return false;
	}
	while (*source != '\0') {
		char value = *source++;
		if (value == '%') {
			const int high = HexValue(source[0]);
			const int low = source[0] != '\0' ? HexValue(source[1]) : -1;
			if (high < 0 || low < 0) {
				*error = "invalid percent escape in dropped file path";
				return false;
			}
			value = static_cast<char>((high << 4) | low);
			source += 2;
		}
		if (output + 1 >= capacity) {
			*error = "dropped file path is too long";
			return false;
		}
		destination[output++] = value;
	}
	destination[output] = '\0';
	return true;
}

#ifdef XM8_ENABLE_RETROACHIEVEMENTS
uint32_t ReadRaMemoryFromVm(uint32_t address, uint8_t *buffer,
	uint32_t num_bytes, void *userdata)
{
	VM *vm = static_cast<VM *>(userdata);
	if (vm == NULL || buffer == NULL) {
		return 0;
	}
	return static_cast<uint32_t>(
		vm->read_ra_inspection_memory(address, buffer, num_bytes));
}

std::string MakeRaUserAgent()
{
	std::ostringstream stream;
	stream << "XM8/" << GetAppVersionString()
		<< " rcheevos/" << Xm8RaBuildInfo::RcheevosVersionString()
		<< " (macOS)";
	return stream.str();
}

std::string RaEventNotice(const Xm8Ra::RaEvent& event)
{
	switch (event.type) {
	case Xm8Ra::RaEventType::AchievementTriggered:
		return "RA: unlocked " + event.achievement.title;
	case Xm8Ra::RaEventType::LeaderboardStarted:
		return "RA: leaderboard started " + event.leaderboard.title;
	case Xm8Ra::RaEventType::LeaderboardFailed:
		return "RA: leaderboard failed " + event.leaderboard.title;
	case Xm8Ra::RaEventType::LeaderboardSubmitted:
		return "RA: leaderboard submitted " + event.leaderboard.title;
	case Xm8Ra::RaEventType::LeaderboardScoreboard:
		return "RA: leaderboard rank " +
			std::to_string(event.scoreboard.new_rank);
	case Xm8Ra::RaEventType::GameCompleted:
		return "RA: game completed";
	case Xm8Ra::RaEventType::ServerError:
		return "RA: server error " + event.server_error.message;
	case Xm8Ra::RaEventType::Disconnected:
		return "RA: disconnected";
	case Xm8Ra::RaEventType::Reconnected:
		return "RA: reconnected";
	case Xm8Ra::RaEventType::SubsetCompleted:
		return "RA: subset completed " + event.subset.title;
	case Xm8Ra::RaEventType::RichPresenceChanged:
		return event.rich_presence.empty() ? std::string() :
			"RA: " + event.rich_presence;
	default:
		break;
	}
	return std::string();
}
#endif

} // namespace

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
#ifdef XM8_ENABLE_RETROACHIEVEMENTS
	ra_library = NULL;
	ra_media_store = NULL;
	ra_service = NULL;
	ra_overlay = NULL;
	ra_mode_enabled = false;
	ra_saved_login_started = false;
	ra_manual_login_started = false;
	ra_session_disabled = false;
#endif

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
	startup_disk_boot = false;
	cli_system_override = false;
	cli_clock_override = false;
	cli_settings_restored = true;
	cli_original_system = SETTING_V2_MODE;
	cli_original_clock = 4;
	cli_original_8h = false;

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
bool App::Init(const CliOptions& options)
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
	if (ApplyCommandLineSettings(options) == false) {
		Deinit();
		return false;
	}

#ifdef XM8_ENABLE_RETROACHIEVEMENTS
	ra_overlay = new Xm8Ra::RaOverlay;
	ra_library = new Xm8Ra::RaLibrary;
	{
		std::string ra_error;
		const std::string ra_root =
			Xm8Ra::RootFromSettingDir(setting->GetSettingDir());
		if (!ra_root.empty() && ra_library->Open(ra_root, &ra_error)) {
			Xm8Ra::RaSettings ra_settings;
			if (ra_library->LoadSettings(&ra_settings, &ra_error)) {
				ra_mode_enabled = ra_settings.enabled;
			}
			ra_media_store = new Xm8Ra::RaMediaStore(ra_library);
		}
		else {
			delete ra_library;
			ra_library = NULL;
			ra_mode_enabled = false;
		}
	}
#endif

	// spcfiy scaling quality (all platforms)
	if (setting->IsImageInterpolation()) {
		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
	} else {
		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, setting->GetScaleQuality());
	}

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


	Uint32 window_flags = SDL_WINDOW_SHOWN | SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_MOUSE_FOCUS;
#ifdef __ANDROID__
	window_flags |= SDL_WINDOW_FULLSCREEN;
#endif
	window = SDL_CreateWindow(  GetAppTitle(),
								SDL_WINDOWPOS_UNDEFINED,
								SDL_WINDOWPOS_UNDEFINED,
								width,
								height,
								window_flags);
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

#ifdef XM8_ENABLE_RETROACHIEVEMENTS
	if (ra_mode_enabled && ra_library != NULL) {
		std::string ra_error;
		if (!EnsureRaService(&ra_error)) {
			ra_mode_enabled = false;
			AddRaNotice("RA: service unavailable");
		}
	}
#endif

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

#ifndef __ANDROID__
	std::string disk_error;
	if (OpenStartupDisks(options.disks, &disk_error) == false) {
		fprintf(stderr, "XM8: %s\n", disk_error.c_str());
		platform->MsgBox(window, disk_error.c_str());
		Deinit();
		return false;
	}
	startup_disk_boot = !options.disks.empty() ||
		cli_system_override || cli_clock_override;
#endif

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
    // Removed in ver 1.7.4
//	Android_PollJoystick();
#endif // __ANDROID__

	return true;
}

//
// ApplyCommandLineSettings()
// apply session-only system settings
//
bool App::ApplyCommandLineSettings(const CliOptions& options)
{
	SDL_assert(setting != NULL);

	cli_system_override = options.system != CliSystemMode::Unspecified;
	cli_clock_override = options.clock != CliClockMode::Unspecified;
	cli_settings_restored = !(cli_system_override || cli_clock_override);
	cli_original_system = setting->GetSystemMode();
	cli_original_clock = setting->GetCPUClock();
	cli_original_8h = setting->Is8HMode();

	switch (options.system) {
	case CliSystemMode::V1S:
		setting->SetSystemMode(SETTING_V1S_MODE);
		break;
	case CliSystemMode::V1H:
		setting->SetSystemMode(SETTING_V1H_MODE);
		break;
	case CliSystemMode::V2:
		setting->SetSystemMode(SETTING_V2_MODE);
		break;
	case CliSystemMode::N:
		setting->SetSystemMode(SETTING_N_MODE);
		break;
	case CliSystemMode::Unspecified:
		break;
	}

	switch (options.clock) {
	case CliClockMode::Clock4MHz:
		setting->SetCPUClock(4);
		setting->Set8HMode(false);
		break;
	case CliClockMode::Clock8MHz:
		setting->SetCPUClock(8);
		setting->Set8HMode(false);
		break;
	case CliClockMode::Clock8MHzH:
		setting->SetCPUClock(8);
		setting->Set8HMode(true);
		break;
	case CliClockMode::Unspecified:
		break;
	}
	return true;
}

//
// RestoreCommandLineSettings()
// keep CLI overrides out of setting.bin
//
void App::RestoreCommandLineSettings()
{
	if (setting == NULL || cli_settings_restored == true) {
		return;
	}
	if (cli_system_override == true) {
		setting->SetSystemMode(cli_original_system);
	}
	if (cli_clock_override == true) {
		setting->SetCPUClock(cli_original_clock);
		setting->Set8HMode(cli_original_8h);
	}
	cli_settings_restored = true;
}

//
// ProbeDisk()
// validate a disk specification without changing a drive
//
bool App::ProbeDisk(const DiskSpec& spec, int *banks, std::string *error)
{
	if (spec.drive < 0 || spec.drive >= MAX_DRIVE) {
		*error = "invalid target drive";
		return false;
	}
	if (spec.path.size() >= (_MAX_PATH * 3)) {
		std::ostringstream message;
		message << "drive " << spec.drive << ": path is too long: " << spec.path;
		*error = message.str();
		return false;
	}
	if (DiskManager::Probe(spec.path.c_str(), banks) == false) {
		std::ostringstream message;
		message << "drive " << spec.drive << ": cannot open D88: " << spec.path;
		*error = message.str();
		return false;
	}
	if (spec.bank < 0 || spec.bank >= *banks) {
		std::ostringstream message;
		message << "drive " << spec.drive << ": bank " << spec.bank
			<< " is out of range; image has " << *banks
			<< " banks: " << spec.path;
		*error = message.str();
		return false;
	}
	return true;
}

//
// OpenDiskFromUser()
// validate and open one disk
//
bool App::OpenDiskFromUser(const DiskSpec& spec, std::string *error)
{
	DiskSpec open_spec = spec;
#ifdef XM8_ENABLE_RETROACHIEVEMENTS
	if (ResolveDiskForRaMode(spec, &open_spec, error) == false) {
		return false;
	}
#endif
	int banks;
	if (ProbeDisk(open_spec, &banks, error) == false) {
		return false;
	}
	if (diskmgr[open_spec.drive]->Open(open_spec.path.c_str(),
		open_spec.bank) == false) {
		std::ostringstream message;
		message << "drive " << open_spec.drive << ": failed to insert D88: "
			<< open_spec.path;
		*error = message.str();
		return false;
	}
	return true;
}

#ifdef XM8_ENABLE_RETROACHIEVEMENTS
//
// ResolveDiskForRaMode()
// map original D88 to RA working copy when RA mode is enabled
//
bool App::ResolveDiskForRaMode(const DiskSpec& spec, DiskSpec *resolved,
	std::string *error)
{
	if (resolved == NULL) {
		*error = "invalid RA disk target";
		return false;
	}
	*resolved = spec;
	if (!ra_mode_enabled) {
		return true;
	}
	if (ra_media_store == NULL) {
		*error = "RA media store is not available";
		return false;
	}

	Xm8Ra::ImportedMedia imported;
	if (!ra_media_store->ImportDesktopD88(spec.path, &imported, error)) {
		return false;
	}
	resolved->path = imported.working_path;
	if (spec.drive == 0 && ra_pending_game_hash.empty() &&
		ra_loaded_game_hash.empty()) {
		BeginRaSessionForMedia(imported.record.md5);
	}
	return true;
}

//
// EnsureRaService()
// create RA service on demand
//
bool App::EnsureRaService(std::string *error)
{
	if (ra_service != NULL) {
		return true;
	}
	if (ra_library == NULL) {
		if (error != NULL) {
			*error = "RA library is not available";
		}
		return false;
	}
	if (vm == NULL) {
		if (error != NULL) {
			*error = "VM is not available";
		}
		return false;
	}

	Xm8Ra::RaServiceOptions ra_options;
	ra_options.ra_root = ra_library->Root();
	ra_options.user_agent = MakeRaUserAgent();
	ra_options.http_client =
		Xm8Ra::CreateMacRaHttpClient(ra_options.user_agent);
	ra_options.host_read_memory = ReadRaMemoryFromVm;
	ra_options.host_read_memory_userdata = vm;
	ra_service = new Xm8Ra::RaService(std::move(ra_options));
	if (!ra_service->IsReady()) {
		delete ra_service;
		ra_service = NULL;
		if (error != NULL) {
			*error = "RA service is not ready";
		}
		return false;
	}
	return true;
}

//
// SaveRaModeSetting()
// persist RA mode setting without changing other RA settings
//
bool App::SaveRaModeSetting(bool enabled, std::string *error)
{
	if (ra_library == NULL) {
		if (error != NULL) {
			*error = "RA library is not available";
		}
		return false;
	}

	Xm8Ra::RaSettings settings;
	if (!ra_library->LoadSettings(&settings, error)) {
		return false;
	}
	settings.enabled = enabled;
	return ra_library->SaveSettings(settings, error);
}

//
// BeginRaSessionForMedia()
// remember the media hash to identify through RA
//
void App::BeginRaSessionForMedia(const std::string& md5)
{
	if (!ra_mode_enabled || ra_service == NULL || md5.empty()) {
		return;
	}

	ra_session_disabled = false;
	ra_saved_login_started = false;
	ra_pending_game_hash = md5;
	ra_loaded_game_hash.clear();
	AddRaNotice("RA: identifying game");
}

//
// ProcessRaService()
// progress RA HTTP, login, game load, and frame/idle processing
//
void App::ProcessRaService(bool emulation_frame)
{
	if (!ra_mode_enabled || ra_service == NULL || ra_session_disabled) {
		return;
	}

	ra_service->DrainHttp();
	if (ra_manual_login_started) {
		const Xm8Ra::RaLoginSnapshot login = ra_service->LoginSnapshot();
		if (login.state == Xm8Ra::RaLoginState::LoggedIn) {
			ra_manual_login_started = false;
			const std::string name = login.display_name.empty() ?
				login.username : login.display_name;
			AddRaNotice(name.empty() ? "RA: logged in" :
				"RA: logged in " + name);
		}
		else if (login.state == Xm8Ra::RaLoginState::Failed) {
			ra_manual_login_started = false;
			AddRaNotice("RA: login failed");
		}
	}

	if (!ra_pending_game_hash.empty()) {
		const Xm8Ra::RaLoginSnapshot login = ra_service->LoginSnapshot();
		const Xm8Ra::RaGameSessionSnapshot game =
			ra_service->GameSessionSnapshot();

		if (login.state == Xm8Ra::RaLoginState::LoggedOut &&
			!ra_saved_login_started) {
			std::string error;
			ra_saved_login_started =
				ra_service->BeginLoginWithSavedToken(&error);
			if (!ra_saved_login_started) {
				ra_session_disabled = true;
				AddRaNotice("RA: login required");
			}
		}
		else if (login.state == Xm8Ra::RaLoginState::LoggedIn &&
			game.state == Xm8Ra::RaGameSessionState::NoGame) {
			std::string error;
			if (ra_service->BeginLoadGameByHash(ra_pending_game_hash,
				&error)) {
				ra_loaded_game_hash = ra_pending_game_hash;
				AddRaNotice("RA: loading game");
			}
			else {
				ra_session_disabled = true;
				AddRaNotice("RA: game load failed");
			}
		}
		else if (game.state == Xm8Ra::RaGameSessionState::Loaded) {
			ra_pending_game_hash.clear();
			AddRaNotice(game.title.empty() ? "RA: game loaded" :
				"RA: " + game.title);
		}
		else if (login.state == Xm8Ra::RaLoginState::Failed ||
			game.state == Xm8Ra::RaGameSessionState::DisabledForSession) {
			ra_session_disabled = true;
			if (login.state == Xm8Ra::RaLoginState::Failed) {
				AddRaNotice("RA: login failed");
			}
			else {
				AddRaNotice("RA: disabled for this session");
			}
		}
	}

	if (emulation_frame) {
		if (!ra_service->DoFrame()) {
			ra_service->Idle();
		}
	}
	else {
		ra_service->Idle();
	}
	AddRaEventsAsNotices(ra_service->TakeEvents());
}

//
// AddRaNotice()
// add RA overlay notice
//
void App::AddRaNotice(const std::string& text)
{
	if (ra_overlay == NULL) {
		return;
	}
	ra_overlay->AddNotice(text, SDL_GetTicks());
}

//
// AddRaEventsAsNotices()
// translate RA events to transient notices
//
void App::AddRaEventsAsNotices(const std::vector<Xm8Ra::RaEvent>& events)
{
	for (const Xm8Ra::RaEvent& event : events) {
		const std::string notice = RaEventNotice(event);
		if (!notice.empty()) {
			AddRaNotice(notice);
		}
	}
}

//
// HandleRaOverlayKeyDown()
// handle RA overlay key input
//
bool App::HandleRaOverlayKeyDown(SDL_Event *e)
{
	if (ra_overlay == NULL || !ra_overlay->IsBlocking()) {
		return false;
	}

	Xm8Ra::RaOverlayKey key;
	switch (e->key.keysym.scancode) {
	case SDL_SCANCODE_TAB:
		key = Xm8Ra::RaOverlayKey::Tab;
		break;
	case SDL_SCANCODE_BACKSPACE:
		key = Xm8Ra::RaOverlayKey::Backspace;
		break;
	case SDL_SCANCODE_RETURN:
	case SDL_SCANCODE_KP_ENTER:
		key = Xm8Ra::RaOverlayKey::Enter;
		break;
	case SDL_SCANCODE_ESCAPE:
		key = Xm8Ra::RaOverlayKey::Escape;
		break;
	case SDL_SCANCODE_UP:
		key = Xm8Ra::RaOverlayKey::Up;
		break;
	case SDL_SCANCODE_DOWN:
		key = Xm8Ra::RaOverlayKey::Down;
		break;
	default:
		return true;
	}

	const Xm8Ra::RaOverlayAction action = ra_overlay->OnControlKey(key);
	if (action == Xm8Ra::RaOverlayAction::SubmitLogin) {
		SubmitRaOverlayLogin();
	}
	else if (action == Xm8Ra::RaOverlayAction::Close) {
		SDL_StopTextInput();
		AddRaNotice("RA: login canceled");
	}
	return true;
}

//
// HandleRaOverlayTextInput()
// handle RA overlay text input
//
bool App::HandleRaOverlayTextInput(SDL_Event *e)
{
	if (ra_overlay == NULL || !ra_overlay->IsBlocking()) {
		return false;
	}
	ra_overlay->OnTextInput(e->text.text);
	return true;
}

//
// SubmitRaOverlayLogin()
// submit RA overlay login form
//
bool App::SubmitRaOverlayLogin()
{
	if (ra_overlay == NULL) {
		return false;
	}

	std::string username;
	std::string password;
	if (!ra_overlay->ConsumeSubmittedLogin(&username, &password)) {
		return false;
	}

	std::string error;
	if (!EnsureRaService(&error)) {
		ra_overlay->SetLoginStatus("RA service unavailable");
		AddRaNotice("RA: service unavailable");
		std::fill(password.begin(), password.end(), '\0');
		return false;
	}

	const bool started = ra_service->BeginLoginWithPassword(username,
		password, &error);
	std::fill(password.begin(), password.end(), '\0');
	if (!started) {
		ra_overlay->SetLoginStatus(error.empty() ?
			"Login failed to start" : error);
		AddRaNotice("RA: login failed");
		return false;
	}

	ra_session_disabled = false;
	ra_saved_login_started = false;
	ra_manual_login_started = true;
	ra_overlay->CloseScreen();
	SDL_StopTextInput();
	AddRaNotice("RA: login started");
	return true;
}

//
// DrawRaOverlay()
// draw RA notice overlay
//
void App::DrawRaOverlay()
{
	if (!ra_mode_enabled || ra_overlay == NULL) {
		return;
	}
	if (ra_overlay->Screen() == Xm8Ra::RaOverlayScreen::Login) {
		const Xm8Ra::RaOverlayLoginSnapshot login =
			ra_overlay->LoginSnapshot();
		SDL_Rect panel = {104, 78, 432, 218};
		Uint32 *buf = video->GetFrameBuf(0);
		font->DrawFillRect(buf, &panel,
			RGB_COLOR(16, 16, 16) | 0xe0000000);
		font->DrawRect(buf, &panel,
			RGB_COLOR(255, 255, 255) | 0xe0000000,
			RGB_COLOR(16, 16, 16) | 0xe0000000);

		SDL_Rect title = {panel.x, panel.y + 12, panel.w, 24};
		font->DrawSjisCenterOr(buf, &title, "RetroAchievements Login",
			RGB_COLOR(255, 255, 255));

		SDL_Rect user_label = {panel.x + 24, panel.y + 56, 104, 22};
		SDL_Rect user_box = {panel.x + 128, panel.y + 52, 272, 28};
		SDL_Rect pass_label = {panel.x + 24, panel.y + 96, 104, 22};
		SDL_Rect pass_box = {panel.x + 128, panel.y + 92, 272, 28};
		font->DrawSjisLeftOr(buf, &user_label, "Username",
			RGB_COLOR(220, 220, 220));
		font->DrawSjisLeftOr(buf, &pass_label, "Password",
			RGB_COLOR(220, 220, 220));

		const bool user_focus =
			login.field == Xm8Ra::RaOverlayLoginField::Username;
		font->DrawRect(buf, &user_box,
			user_focus ? RGB_COLOR(255, 255, 128) : RGB_COLOR(128, 128, 128),
			RGB_COLOR(0, 0, 0) | 0xe0000000);
		font->DrawRect(buf, &pass_box,
			!user_focus ? RGB_COLOR(255, 255, 128) : RGB_COLOR(128, 128, 128),
			RGB_COLOR(0, 0, 0) | 0xe0000000);

		char user[64];
		char pass[64];
		std::snprintf(user, sizeof(user), "%s", login.username.c_str());
		std::snprintf(pass, sizeof(pass), "%s",
			login.masked_password.c_str());
		user_box.x += 8;
		user_box.w -= 16;
		pass_box.x += 8;
		pass_box.w -= 16;
		font->DrawSjisLeftOr(buf, &user_box, user,
			RGB_COLOR(255, 255, 255));
		font->DrawSjisLeftOr(buf, &pass_box, pass,
			RGB_COLOR(255, 255, 255));

		SDL_Rect hint = {panel.x + 24, panel.y + 138, panel.w - 48, 22};
		font->DrawSjisLeftOr(buf, &hint,
			login.can_submit ? "Enter: Login  Esc: Cancel" :
				"Tab: Switch field  Esc: Cancel",
			RGB_COLOR(200, 200, 200));
		if (!login.status_message.empty()) {
			char status[72];
			std::snprintf(status, sizeof(status), "%s",
				login.status_message.c_str());
			SDL_Rect status_rect = {panel.x + 24, panel.y + 170,
				panel.w - 48, 22};
			font->DrawSjisLeftOr(buf, &status_rect, status,
				RGB_COLOR(255, 192, 96));
		}
		video->DrawCtrl();
	}
	const std::string notice = ra_overlay->VisibleNotice(SDL_GetTicks());
	if (notice.empty()) {
		return;
	}

	char text[72];
	std::snprintf(text, sizeof(text), "%s", notice.c_str());
	SDL_Rect rect = {8, 8, 624, 24};
	Uint32 *buf = video->GetFrameBuf(0);
	font->DrawFillRect(buf, &rect, RGB_COLOR(0, 0, 0) | 0xc0000000);
	font->DrawRect(buf, &rect, RGB_COLOR(255, 255, 255) | 0xc0000000,
		RGB_COLOR(0, 0, 0) | 0xc0000000);
	rect.x += 8;
	rect.w -= 16;
	font->DrawSjisLeftOr(buf, &rect, text, RGB_COLOR(255, 255, 255));
	video->DrawCtrl();
}
#endif

//
// OpenStartupDisks()
// validate all CLI disks before opening any drive
//
bool App::OpenStartupDisks(const std::vector<DiskSpec>& disks,
	std::string *error)
{
	int banks;
	for (const DiskSpec& spec : disks) {
		if (ProbeDisk(spec, &banks, error) == false) {
			return false;
		}
	}
	for (const DiskSpec& spec : disks) {
		if (OpenDiskFromUser(spec, error) == false) {
			return false;
		}
	}
	return true;
}

//
// OpenDroppedDisk()
// preserve the legacy bank 0/1 D&D behavior
//
bool App::OpenDroppedDisk(const char *path, std::string *error)
{
	struct Snapshot {
		bool open;
		std::string path;
		int bank;
	};
	Snapshot snapshots[MAX_DRIVE];
	DiskSpec first = {path, 0, 0};
	int banks;

	if (ProbeDisk(first, &banks, error) == false) {
		return false;
	}
	for (int drive=0; drive<MAX_DRIVE; drive++) {
		snapshots[drive].open = diskmgr[drive]->IsOpen();
		if (snapshots[drive].open) {
			snapshots[drive].path = diskmgr[drive]->GetPath();
			snapshots[drive].bank = diskmgr[drive]->GetBank();
		}
	}

	auto restore = [this, &snapshots]() {
		for (int drive=0; drive<MAX_DRIVE; drive++) {
			if (snapshots[drive].open) {
				diskmgr[drive]->Open(snapshots[drive].path.c_str(),
					snapshots[drive].bank);
			} else {
				diskmgr[drive]->Close();
			}
		}
	};

	if (OpenDiskFromUser(first, error) == false) {
		restore();
		return false;
	}
	if (banks > 1) {
		DiskSpec second = {path, 1, 1};
		if (OpenDiskFromUser(second, error) == false) {
			restore();
			return false;
		}
	} else {
		diskmgr[1]->Close();
	}
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

#ifdef XM8_ENABLE_RETROACHIEVEMENTS
	if (ra_service != NULL) {
		delete ra_service;
		ra_service = NULL;
	}
	if (ra_overlay != NULL) {
		delete ra_overlay;
		ra_overlay = NULL;
	}
#endif

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

#ifdef XM8_ENABLE_RETROACHIEVEMENTS
	if (ra_media_store != NULL) {
		delete ra_media_store;
		ra_media_store = NULL;
	}
	if (ra_library != NULL) {
		delete ra_library;
		ra_library = NULL;
	}
	ra_mode_enabled = false;
	ra_saved_login_started = false;
	ra_manual_login_started = false;
	ra_session_disabled = false;
	ra_pending_game_hash.clear();
	ra_loaded_game_hash.clear();
#endif

	// setting
	if (setting != NULL) {
		RestoreCommandLineSettings();
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
// GetAudio()
// get Audio instance
//
Audio* App::GetAudio()
{
	SDL_assert(audio != NULL);
	return audio;
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
	if (startup_disk_boot == false) {
		// load state 0 (auto)
		Load(0);

		// enter menu
		EnterMenu(MENU_MAIN);
	}
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
#ifdef XM8_ENABLE_RETROACHIEVEMENTS
			ProcessRaService(false);
#endif

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
#ifdef XM8_ENABLE_RETROACHIEVEMENTS
			for (ret=0; ret<extra; ret++) {
				ProcessRaService(true);
			}
#endif
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

	// Do not leak a command-line session into the automatic state.
	if (startup_disk_boot == false) {
		Save(0);
	}
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

#ifdef XM8_ENABLE_RETROACHIEVEMENTS
	DrawRaOverlay();
#endif

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
#ifdef XM8_ENABLE_RETROACHIEVEMENTS
		if (HandleRaOverlayTextInput(e)) {
			break;
		}
#endif
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

#ifdef XM8_ENABLE_RETROACHIEVEMENTS
	if (HandleRaOverlayKeyDown(e)) {
		return;
	}
#endif

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
	std::string error;
	bool result = DecodeDropPath(e->drop.file, state_path,
		sizeof(state_path), &error);
	if (result == true) {
		result = OpenDroppedDisk(state_path, &error);
	}
	SDL_free(e->drop.file);

	if (result == true) {
		// leave menu
		LeaveMenu();

		// reset
		Reset();
	} else {
		platform->MsgBox(window, error.c_str());
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
			input->ChangeCursorToNumPad(setting->IsCursorToNumPad());
			input->ChangeNumToNumPad(setting->IsNumToNumPad());

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

#ifdef XM8_ENABLE_RETROACHIEVEMENTS
//
// IsRaModeEnabled()
// get RA mode setting
//
bool App::IsRaModeEnabled() const
{
	return ra_mode_enabled;
}

//
// ToggleRaMode()
// toggle RA mode setting
//
bool App::ToggleRaMode()
{
	const bool enable = !ra_mode_enabled;
	std::string error;

	if (enable && !EnsureRaService(&error)) {
		AddRaNotice("RA: service unavailable");
		return false;
	}
	if (!SaveRaModeSetting(enable, &error)) {
		AddRaNotice("RA: setting save failed");
		return false;
	}

	ra_mode_enabled = enable;
	ra_session_disabled = false;
	ra_saved_login_started = false;
	ra_manual_login_started = false;
	ra_pending_game_hash.clear();
	ra_loaded_game_hash.clear();
	if (!enable && ra_service != NULL) {
		ra_service->UnloadGame();
	}
	AddRaNotice(enable ? "RA: mode enabled" : "RA: mode disabled");
	return true;
}

//
// RetryRaSavedLogin()
// retry RA saved token login
//
bool App::RetryRaSavedLogin()
{
	std::string error;
	if (!ra_mode_enabled) {
		if (!SaveRaModeSetting(true, &error)) {
			AddRaNotice("RA: setting save failed");
			return false;
		}
		ra_mode_enabled = true;
	}
	if (!EnsureRaService(&error)) {
		AddRaNotice("RA: service unavailable");
		return false;
	}
	if (IsRaLoggedIn()) {
		AddRaNotice("RA: already logged in");
		return true;
	}
	if (!ra_service->BeginLoginWithSavedToken(&error)) {
		AddRaNotice("RA: login required");
		return false;
	}

	ra_session_disabled = false;
	ra_saved_login_started = true;
	ra_manual_login_started = false;
	AddRaNotice("RA: login started");
	return true;
}

//
// IsRaLoggedIn()
// get RA login state
//
bool App::IsRaLoggedIn() const
{
	if (ra_service == NULL) {
		return false;
	}
	return ra_service->LoginSnapshot().state == Xm8Ra::RaLoginState::LoggedIn;
}

//
// OpenRaLoginOverlay()
// open RA password login overlay
//
bool App::OpenRaLoginOverlay()
{
	if (ra_overlay == NULL) {
		AddRaNotice("RA: overlay unavailable");
		return false;
	}

	std::string error;
	if (!ra_mode_enabled) {
		if (!SaveRaModeSetting(true, &error)) {
			AddRaNotice("RA: setting save failed");
			return false;
		}
		ra_mode_enabled = true;
	}
	if (!EnsureRaService(&error)) {
		AddRaNotice("RA: service unavailable");
		return false;
	}

	std::string username;
	const Xm8Ra::RaLoginSnapshot login = ra_service->LoginSnapshot();
	if (!login.username.empty()) {
		username = login.username;
	}
	else if (!login.display_name.empty()) {
		username = login.display_name;
	}
	ra_overlay->OpenLogin(username);
	SDL_StartTextInput();
	if (app_menu == true) {
		LeaveMenu(false);
	}
	AddRaNotice("RA: login");
	return true;
}

//
// LogoutRa()
// logout RA
//
void App::LogoutRa()
{
	if (ra_service != NULL) {
		ra_service->Logout();
	}
	ra_session_disabled = false;
	ra_saved_login_started = false;
	ra_manual_login_started = false;
	ra_pending_game_hash.clear();
	ra_loaded_game_hash.clear();
	AddRaNotice("RA: logged out");
}

//
// GetRaMenuStatus()
// get RA status text for menu
//
void App::GetRaMenuStatus(char *buffer, size_t capacity) const
{
	if (buffer == NULL || capacity == 0) {
		return;
	}
	const char *text = "RA: unavailable";
	if (ra_mode_enabled) {
		text = "RA: enabled";
	}
	else if (ra_library != NULL) {
		text = "RA: disabled";
	}
	if (ra_service != NULL) {
		const Xm8Ra::RaLoginSnapshot login = ra_service->LoginSnapshot();
		const Xm8Ra::RaGameSessionSnapshot game =
			ra_service->GameSessionSnapshot();
		if (login.state == Xm8Ra::RaLoginState::LoginPending) {
			text = "RA: login pending";
		}
		else if (login.state == Xm8Ra::RaLoginState::LoggedIn &&
			game.state == Xm8Ra::RaGameSessionState::Loaded) {
			std::snprintf(buffer, capacity, "RA: %s",
				game.title.empty() ? "game loaded" : game.title.c_str());
			return;
		}
		else if (game.state == Xm8Ra::RaGameSessionState::DisabledForSession) {
			text = "RA: session disabled";
		}
		else if (login.state == Xm8Ra::RaLoginState::LoggedIn) {
			const std::string name = login.display_name.empty() ?
				login.username : login.display_name;
			if (!name.empty()) {
				std::snprintf(buffer, capacity, "RA: logged in %s",
					name.c_str());
				return;
			}
			text = "RA: logged in";
		}
		else if (login.state == Xm8Ra::RaLoginState::Failed) {
			text = "RA: login failed";
		}
	}
	std::snprintf(buffer, capacity, "%s", text);
}
#endif

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
// GetAppVersionString()
// get printable application version
//
const char* GetAppVersionString()
{
	static char version[8];
	const unsigned int major = (APP_VER >> 8) & 0xff;
	const unsigned int minor = (APP_VER >> 4) & 0x0f;
	const unsigned int patch = APP_VER & 0x0f;

	snprintf(version, sizeof(version), "%u.%u.%u", major, minor, patch);
	return version;
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
