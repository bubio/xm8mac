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
#include <cstring>
#include <ctime>
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
#include "menuitem.h"
#include "menuid.h"
#include "diskmgr.h"
#include "tapemgr.h"
#include "clidisk.h"
#ifdef XM8_ENABLE_RETROACHIEVEMENTS
#include "ra_build_info.h"
#include "ra_media_probe.h"
#include "ra_media_change_policy.h"
#include "ra_platform.h"
#include "ra_paths.h"
#include "ra_session_policy.h"
#include "ra_state_store.h"
#include "ra_text_converter.h"
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
#define SLEEP_RA_BACKGROUND		1000
										// maximum RA idle interval in background (ms)
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

#ifdef XM8_ENABLE_RETROACHIEVEMENTS
SDL_Rect RaGameDetailStartButtonRect();
bool PointInRect(int x, int y, const SDL_Rect& rect);
#endif

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
uint32_t ReadRaMemoryFromApp(uint32_t address, uint8_t *buffer,
	uint32_t num_bytes, void *userdata)
{
	App *app = static_cast<App *>(userdata);
	if (app == NULL || buffer == NULL) {
		return 0;
	}
	return app->ReadRaInspectionMemory(address, buffer, num_bytes);
}

std::string MakeRaUserAgent()
{
	std::ostringstream stream;
	stream << "XM8/" << GetAppVersionString()
		<< " rcheevos/" << Xm8RaBuildInfo::RcheevosVersionString()
		<< " (macOS)";
	return stream.str();
}

std::string ParentDirectoryName(const std::string& path)
{
	const size_t end = path.find_last_not_of("/\\");
	if (end == std::string::npos) return std::string();
	const size_t parent_separator = path.find_last_of("/\\", end);
	if (parent_separator == std::string::npos) return std::string();
	const size_t grandparent_separator = path.find_last_of("/\\",
		parent_separator == 0 ? 0 : parent_separator - 1);
	const size_t begin = grandparent_separator == std::string::npos ? 0 :
		grandparent_separator + 1;
	return path.substr(begin, parent_separator - begin);
}

bool DecodeRaBadgePixels(const std::vector<uint8_t>& encoded, int *width,
	int *height, std::vector<uint32_t> *pixels)
{
	std::vector<uint8_t> rgba;
	if (!Xm8RaBuildInfo::DecodeImageRgba(encoded.data(), encoded.size(),
		width, height, &rgba) || *width > 2048 || *height > 2048 ||
		static_cast<int64_t>(*width) * *height > 4194304) {
		return false;
	}
	pixels->clear();
	pixels->reserve(static_cast<size_t>(*width) *
		static_cast<size_t>(*height));
	for (size_t index = 0; index + 3 < rgba.size(); index += 4) {
		pixels->push_back((static_cast<uint32_t>(rgba[index + 3]) << 24) |
			RGB_COLOR(rgba[index], rgba[index + 1], rgba[index + 2]));
	}
	return !pixels->empty();
}

std::string RaEventNotice(const Xm8Ra::RaEvent& event)
{
	switch (event.type) {
	case Xm8Ra::RaEventType::AchievementTriggered:
		return "RA: unlocked " + event.achievement.title + " (" +
			std::to_string(event.achievement.points) + " points)";
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
		// Rich Presence is frequently updated gameplay state. Display it in
		// the RA menu detail line instead of replacing transient notices.
		return std::string();
	default:
		break;
	}
	return std::string();
}

std::string RaSubmissionErrorText(const Xm8Ra::RaServerErrorEvent& error)
{
	std::ostringstream stream;
	stream << "RA send failed";
	if (error.api == "award_achievement") {
		stream << ": achievement";
	}
	else if (error.api == "submit_lboard_entry") {
		stream << ": leaderboard";
	}
	else if (!error.api.empty()) {
		stream << ": " << error.api;
	}
	if (error.related_id != 0) {
		stream << " #" << error.related_id;
	}
	if (!error.message.empty()) {
		stream << ": " << error.message;
	}
	else if (error.result != 0) {
		stream << ": error " << error.result;
	}
	return stream.str();
}

Xm8Ra::RaNoticePriority RaEventNoticePriority(const Xm8Ra::RaEvent& event)
{
	switch (event.type) {
	case Xm8Ra::RaEventType::AchievementTriggered:
	case Xm8Ra::RaEventType::GameCompleted:
	case Xm8Ra::RaEventType::SubsetCompleted:
		return Xm8Ra::RaNoticePriority::Critical;
	case Xm8Ra::RaEventType::LeaderboardFailed:
	case Xm8Ra::RaEventType::LeaderboardSubmitted:
	case Xm8Ra::RaEventType::LeaderboardScoreboard:
	case Xm8Ra::RaEventType::ServerError:
	case Xm8Ra::RaEventType::Disconnected:
	case Xm8Ra::RaEventType::Reconnected:
		return Xm8Ra::RaNoticePriority::Important;
	case Xm8Ra::RaEventType::LeaderboardStarted:
	default:
		return Xm8Ra::RaNoticePriority::Normal;
	}
}

std::string RaGameLoadFailureNotice(const Xm8Ra::RaGameSessionSnapshot& game)
{
	if (!game.message.empty()) {
		return "RA: " + game.message;
	}
	if (!game.hash.empty()) {
		return "RA: unsupported " + game.hash.substr(0, 8);
	}
	return "RA: unsupported game";
}

std::string RaLeaderboardScoreboardDetail(
	const Xm8Ra::RaOverlayLeaderboardItem& leaderboard)
{
	std::ostringstream stream;
	stream << "Rank " << leaderboard.new_rank;
	if (leaderboard.num_entries != 0) {
		stream << "/" << leaderboard.num_entries;
	}
	if (!leaderboard.submitted_score.empty()) {
		stream << "  You " << leaderboard.submitted_score;
	}
	if (!leaderboard.best_score.empty()) {
		stream << "  Best " << leaderboard.best_score;
	}
	if (!leaderboard.top_entries.empty()) {
		const Xm8Ra::RaOverlayLeaderboardItem::ScoreboardEntry& top =
			leaderboard.top_entries[0];
		stream << "  Top #" << top.rank << " " << top.username;
		if (!top.score.empty()) {
			stream << " " << top.score;
		}
	}
	return stream.str();
}

std::string RaLeaderboardEntriesDetail(
	const Xm8Ra::RaOverlayLeaderboardItem& leaderboard)
{
	if (leaderboard.entries_pending) {
		return "Loading leaderboard entries";
	}
	if (leaderboard.entries_failed) {
		return leaderboard.entries_message.empty() ?
			"Leaderboard entries unavailable" : leaderboard.entries_message;
	}
	if (!leaderboard.has_entries) {
		return std::string();
	}

	std::ostringstream stream;
	stream << "Entries " << leaderboard.entries.size();
	if (leaderboard.entry_total != 0) {
		stream << "/" << leaderboard.entry_total;
	}
	const size_t count =
		std::min<size_t>(leaderboard.entries.size(), 3);
	for (size_t i = 0; i < count; i++) {
		const Xm8Ra::RaOverlayLeaderboardItem::ScoreboardEntry& entry =
			leaderboard.entries[i];
		stream << "  #" << entry.rank << " " << entry.username;
		if (!entry.score.empty()) {
			stream << " " << entry.score;
		}
	}
	return stream.str();
}
#endif

std::string DirectoryOfPath(const char *path)
{
	if (path == NULL || path[0] == '\0') {
		return std::string();
	}
	const char *last = NULL;
	for (const char *p = path; *p != '\0'; ++p) {
		if (*p == '/' || *p == '\\') {
			last = p;
		}
	}
	if (last == NULL) {
		return std::string();
	}
	return std::string(path, static_cast<size_t>(last - path + 1));
}

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
	ra_next_image_request_id = 1000000;
	ra_image_cache_limit_bytes = 128LL * 1024 * 1024;
	ra_notification_duration_ms = 5000;
	ra_mode_enabled = false;
	ra_play_mode = Xm8Ra::RaPlayMode::Hardcore;
	ra_fast_disk_override_active = false;
	ra_saved_fast_disk = false;
	ra_reset_requested = false;
	ra_saved_login_started = false;
	ra_manual_login_started = false;
	ra_library_sync_started_for_login = false;
	ra_session_state = Xm8Ra::RaSessionState::Ready;
	ra_overlay_joystick_prev = 0;
	ra_overlay_mouse_target_valid = false;
	ra_overlay_mouse_target = Xm8Ra::RaOverlayLoginTarget::Username;
	ra_overlay_mouse_detail_target = 0;
	ra_overlay_mouse_list_target_valid = false;
	ra_overlay_mouse_list_target = 0;
	ra_overlay_finger_target_valid = false;
	ra_overlay_finger_target = Xm8Ra::RaOverlayLoginTarget::Username;
	ra_overlay_finger_detail_target = 0;
	ra_overlay_finger_list_target_valid = false;
	ra_overlay_finger_list_target = 0;
	ra_overlay_finger_scroll_valid = false;
	ra_overlay_finger_scrolled = false;
	ra_overlay_finger_scroll_y = 0;
	ra_status_mouse_pressed = false;
	ra_status_mouse_dragged = false;
	ra_status_finger_pressed = false;
	ra_status_finger_dragged = false;
	ra_status_finger_id = -1;
	ra_status_finger_start_x = 0;
	ra_status_finger_start_y = 0;
	ra_overlay_auto_scroll_revision = 0;
	ra_overlay_auto_scroll_started = 0;
	ra_menu_presence_scroll_active = false;
	ra_menu_error_scroll_active = false;
	ra_menu_detail_active = false;
	ra_menu_presence_scroll_started = 0;
	ra_pending_library_game_id = 0;
	ra_loaded_library_game_id = 0;
	ra_media_change_pending = false;
	ra_media_change_rollback = false;
	ra_media_change_restore_failed = false;
	ra_media_change_target = {"", 0, 0};
	ra_media_change_old_bank = 0;
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
				ra_play_mode = ra_settings.last_mode == Xm8Ra::kRaModeHardcore ?
					Xm8Ra::RaPlayMode::Hardcore : Xm8Ra::RaPlayMode::Casual;
			}
			ra_media_store = new Xm8Ra::RaMediaStore(ra_library);
		}
		else {
			delete ra_library;
			ra_library = NULL;
			ra_mode_enabled = false;
		}
	}
	ra_menu_status.Set(ra_library == NULL ?
		Xm8Ra::RaMenuStatusState::Unavailable : (ra_mode_enabled ?
		Xm8Ra::RaMenuStatusState::Enabled :
		Xm8Ra::RaMenuStatusState::Disabled));
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

#ifdef XM8_ENABLE_RETROACHIEVEMENTS
	AttachRaHostFrameCallback();
#endif

	// PC88 device
	pc88 = (PC88*)vm->get_device(2);

	// rtc device
	upd1990a = (UPD1990A*)vm->get_device(6);

#ifdef XM8_ENABLE_RETROACHIEVEMENTS
	if (ra_mode_enabled && ra_library != NULL) {
		std::string ra_error;
		if (!EnsureRaService(&ra_error)) {
			ra_mode_enabled = false;
			ra_menu_status.Set(Xm8Ra::RaMenuStatusState::Disabled);
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
	std::string ra_hash_to_identify;
	int64_t ra_game_to_identify = 0;
	bool ra_media_change = false;
	if (ResolveDiskForRaMode(spec, &open_spec, &ra_hash_to_identify,
		&ra_game_to_identify, &ra_media_change, error) == false) {
		return false;
	}
#endif
	int banks;
	if (ProbeDisk(open_spec, &banks, error) == false) {
		return false;
	}
#ifdef XM8_ENABLE_RETROACHIEVEMENTS
	if (ra_media_change) {
		return BeginRaMediaChange(open_spec, ra_hash_to_identify, error);
	}
#endif
	if (diskmgr[open_spec.drive]->Open(open_spec.path.c_str(),
		open_spec.bank) == false) {
		std::ostringstream message;
		message << "drive " << open_spec.drive << ": failed to insert D88: "
			<< open_spec.path;
		*error = message.str();
		return false;
	}
#ifdef XM8_ENABLE_RETROACHIEVEMENTS
	if (ra_mode_enabled &&
		!RememberRaLaunchDriveForMountedDisk(open_spec.drive, error)) {
		return false;
	}
	if (!ra_hash_to_identify.empty()) {
		// A different RA title starts from a cold VM, never from the previous
		// game's live memory.
		LockVM();
		vm->reset();
		upd1990a->resync();
		UnlockVM();
		BeginRaSessionForMedia(ra_hash_to_identify, ra_game_to_identify);
	}
#endif
	return true;
}

//
// OpenDiskFromMenu()
// open disk from menu
//
bool App::OpenDiskFromMenu(const DiskSpec& spec, std::string *error)
{
	if (!OpenDiskFromUser(spec, error)) {
		return false;
	}
	RememberDiskOpenDir(spec.path.c_str());
	return true;
}

#ifdef XM8_ENABLE_RETROACHIEVEMENTS
//
// ResolveDiskForRaMode()
// map original D88 to RA working copy when RA mode is enabled
//
bool App::ResolveDiskForRaMode(const DiskSpec& spec, DiskSpec *resolved,
	std::string *ra_hash_to_identify, int64_t *ra_game_to_identify,
	bool *ra_media_change, std::string *error)
{
	if (resolved == NULL) {
		*error = "invalid RA disk target";
		return false;
	}
	*resolved = spec;
	if (ra_hash_to_identify != NULL) {
		ra_hash_to_identify->clear();
	}
	if (ra_game_to_identify != NULL) {
		*ra_game_to_identify = 0;
	}
	if (ra_media_change != NULL) {
		*ra_media_change = false;
	}
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
	if (spec.drive == 0 && ra_pending_game_hash.empty()) {
		const int bank = spec.bank < 0 ? 0 : spec.bank;
		if (bank >= static_cast<int>(imported.media_info.bank_md5s.size())) {
			*error = "RA D88 bank hash is not available";
			return false;
		}
		const std::string& ra_hash = imported.media_info.bank_md5s[bank];
		const bool game_loaded = ra_service != NULL &&
			Xm8Ra::IsRaSessionEvaluating(ra_session_state) &&
			ra_service->GameSessionSnapshot().state ==
				Xm8Ra::RaGameSessionState::Loaded;
		const bool same_working_media = game_loaded && diskmgr[0] != NULL &&
			diskmgr[0]->IsOpen() &&
			resolved->path == diskmgr[0]->GetPath();
		const Xm8Ra::RaMediaChangeAction action =
			Xm8Ra::ClassifyMediaChange(spec.drive, game_loaded,
				ra_media_change_pending, same_working_media,
				ra_loaded_library_game_id,
				ra_loaded_game_hash, imported.record.game_id, ra_hash);
		if (action == Xm8Ra::RaMediaChangeAction::RejectPending) {
			*error = "RA media change is already pending";
			return false;
		}
		if (action == Xm8Ra::RaMediaChangeAction::RejectDifferentGame) {
			*error = "RA media belongs to another game; restart required";
			return false;
		}
		if (action == Xm8Ra::RaMediaChangeAction::BeginSameGameChange) {
			if (ra_media_change != NULL) {
				*ra_media_change = true;
			}
			if (ra_hash_to_identify != NULL) {
				*ra_hash_to_identify = ra_hash;
			}
		}
		else if (!game_loaded ||
			Xm8Ra::IsRaSessionOffline(ra_session_state)) {
			if (ra_hash_to_identify != NULL) {
				*ra_hash_to_identify = ra_hash;
			}
			if (ra_game_to_identify != NULL) {
				*ra_game_to_identify = imported.record.game_id;
			}
		}
	}
	return true;
}

//
// BeginRaMediaChange()
// ask RA to accept a same-game Drive 1 media hash before changing the VM
//
bool App::BeginRaMediaChange(const DiskSpec& target,
	const std::string& hash, std::string *error)
{
	if (ra_service == NULL || diskmgr[0] == NULL ||
		!diskmgr[0]->IsOpen() || target.drive != 0 || hash.empty()) {
		if (error != NULL) {
			*error = "RA media change is not available";
		}
		return false;
	}
	if (ra_media_change_pending) {
		if (error != NULL) {
			*error = "RA media change is already pending";
		}
		return false;
	}

	ClearRaMediaChangeState();
	ra_media_change_pending = true;
	ra_media_change_target = target;
	ra_media_change_new_hash = hash;
	ra_media_change_old_hash = ra_loaded_game_hash;
	ra_media_change_old_path = diskmgr[0]->GetPath();
	ra_media_change_old_bank = diskmgr[0]->GetBank();
	if (!ra_service->BeginChangeMediaByHash(hash, error)) {
		ra_service->ClearMediaChangeResult();
		ClearRaMediaChangeState();
		return false;
	}

	return true;
}

//
// ProcessRaMediaChange()
// commit the VM swap after RA success, or restore RA after VM failure
//
void App::ProcessRaMediaChange()
{
	if (!ra_media_change_pending || ra_service == NULL) {
		return;
	}
	const Xm8Ra::RaMediaChangeSnapshot change =
		ra_service->MediaChangeSnapshot();
	if (change.state == Xm8Ra::RaMediaChangeState::None ||
		change.state == Xm8Ra::RaMediaChangeState::Pending) {
		return;
	}

	if (change.state == Xm8Ra::RaMediaChangeState::Failed) {
		const std::string message = change.message.empty() ?
			"media change failed" : change.message;
		ra_service->ClearMediaChangeResult();
		if (ra_media_change_rollback) {
			EnterRaOfflineSession("RA rollback failed: " + message);
		}
		else {
			ClearRaMediaChangeState();
			AddRaNotice("RA: " + message);
		}
		return;
	}

	if (ra_media_change_rollback) {
		ra_service->ClearMediaChangeResult();
		if (ra_media_change_restore_failed) {
			EnterRaOfflineSession("previous disk could not be restored");
			return;
		}
		ra_loaded_game_hash = ra_media_change_old_hash;
		ClearRaMediaChangeState();
		AddRaNotice("RA: media change rolled back");
		return;
	}

	std::string vm_error;
	bool vm_changed = diskmgr[0]->Open(ra_media_change_target.path.c_str(),
		ra_media_change_target.bank);
	if (!vm_changed) {
		vm_error = "VM rejected changed media";
	}
	else if (!RememberRaLaunchDriveForMountedDisk(0, &vm_error)) {
		vm_changed = false;
	}

	if (vm_changed) {
		ra_loaded_game_hash = ra_media_change_new_hash;
		ra_service->ClearMediaChangeResult();
		ClearRaMediaChangeState();
		return;
	}

	const bool restored = !ra_media_change_old_path.empty() &&
		diskmgr[0]->Open(ra_media_change_old_path.c_str(),
			ra_media_change_old_bank);
	ra_media_change_restore_failed = !restored;
	ra_media_change_rollback = true;
	ra_service->ClearMediaChangeResult();
	std::string rollback_error;
	if (ra_media_change_old_hash.empty() ||
		!ra_service->BeginChangeMediaByHash(ra_media_change_old_hash,
			&rollback_error)) {
		EnterRaOfflineSession(rollback_error.empty() ? vm_error :
			"RA rollback failed: " + rollback_error);
		return;
	}
	ProcessRaMediaChange();
}

void App::ClearRaMediaChangeState()
{
	ra_media_change_pending = false;
	ra_media_change_rollback = false;
	ra_media_change_restore_failed = false;
	ra_media_change_target = {"", 0, 0};
	ra_media_change_new_hash.clear();
	ra_media_change_old_hash.clear();
	ra_media_change_old_path.clear();
	ra_media_change_old_bank = 0;
}

void App::SetRaMenuStatusAfterSessionStop()
{
	if (ra_menu_status.State() == Xm8Ra::RaMenuStatusState::UnknownGame) {
		return;
	}
	if (!ra_mode_enabled) {
		ra_menu_status.Set(Xm8Ra::RaMenuStatusState::Disabled);
		return;
	}
	if (ra_service == NULL) {
		ra_menu_status.Set(Xm8Ra::RaMenuStatusState::Enabled);
		return;
	}
	const Xm8Ra::RaLoginSnapshot login = ra_service->LoginSnapshot();
	if (login.state == Xm8Ra::RaLoginState::LoginPending) {
		ra_menu_status.Set(Xm8Ra::RaMenuStatusState::LoginPending);
	}
	else if (login.state == Xm8Ra::RaLoginState::LoggedIn) {
		const std::string name = login.display_name.empty() ?
			login.username : login.display_name;
		ra_menu_status.Set(Xm8Ra::RaMenuStatusState::LoggedIn, name);
	}
	else if (login.state == Xm8Ra::RaLoginState::Failed) {
		ra_menu_status.Set(Xm8Ra::RaMenuStatusState::LoginFailed);
	}
	else {
		ra_menu_status.Set(Xm8Ra::RaMenuStatusState::Enabled);
	}
}

void App::SetRaMenuStatusForConnectivity(bool disconnected)
{
	ra_menu_status.SetConnectivity(disconnected);
}

void App::EnterRaOfflineSession(const std::string& message)
{
	ra_menu_status.EnterOfflineSession();
	if (ra_service != NULL) {
		ra_service->UnloadGame();
	}
	ra_session_state = Xm8Ra::TransitionRaSession(ra_session_state,
		Xm8Ra::RaSessionSignal::SessionInvalidated);
	RestoreRaSessionOverrides();
	if (audio != NULL) CtrlAudio();
	ra_pending_game_hash.clear();
	ra_pending_library_game_id = 0;
	ra_loaded_library_game_id = 0;
	ra_loaded_game_hash.clear();
	ra_leaderboard_scoreboards.clear();
	ClearRaMediaChangeState();
	if (ra_overlay != NULL) {
		ra_overlay->ClearGameplayStatus();
	}
	AddRaNotice(message.empty() ? "RA: offline" : "RA: offline - " + message);
	RefreshRaAchievementsOverlay();
	RefreshRaLeaderboardsOverlay();
	menu->UpdateRaStatus();
}

void App::StopRaSession()
{
	ra_session_state = Xm8Ra::TransitionRaSession(ra_session_state,
		Xm8Ra::RaSessionSignal::StopGame);
	SetRaMenuStatusAfterSessionStop();
	RestoreRaSessionOverrides();
	if (audio != NULL) CtrlAudio();
	if (ra_overlay != NULL) {
		ra_overlay->ClearGameplayStatus();
	}
}

void App::ProcessRaLibrarySync()
{
	if (ra_service == NULL || ra_library == NULL) {
		return;
	}
	const Xm8Ra::RaLibrarySyncSnapshot snapshot =
		ra_service->LibrarySyncSnapshot();
	if (snapshot.state == Xm8Ra::RaLibrarySyncState::Succeeded) {
		Xm8Ra::RaLibrarySyncPayload payload;
		payload.username = snapshot.username;
		for (const Xm8Ra::RaLibrarySyncHash& source : snapshot.hashes) {
			Xm8Ra::RaLibraryHashMatch item;
			item.hash = source.hash;
			item.ra_game_id = source.game_id;
			payload.hashes.push_back(item);
		}
		for (const Xm8Ra::RaLibrarySyncTitle& source : snapshot.titles) {
			Xm8Ra::RaLibraryGameTitle item;
			item.ra_game_id = source.game_id;
			item.title = source.title;
			item.badge_url = source.badge_url;
			payload.titles.push_back(item);
		}
		for (const Xm8Ra::RaLibrarySyncProgress& source : snapshot.progress) {
			Xm8Ra::RaLibraryProgress item;
			item.ra_game_id = source.game_id;
			item.core_total = source.total;
			item.core_unlocked = source.unlocked;
			item.hardcore_unlocked = source.hardcore_unlocked;
			payload.progress.push_back(item);
		}
		std::string error;
		if (ra_library->ApplyLibrarySync(payload, &error)) {
			if (ra_overlay != NULL &&
				ra_overlay->Screen() == Xm8Ra::RaOverlayScreen::Library) {
				ra_overlay->OpenLibrary(MakeRaLibraryOverlaySnapshot());
			}
		}
		else {
			AddRaNotice("RA: library sync apply failed");
		}
		ra_service->ClearLibrarySyncResult();
		return;
	}
	if (snapshot.state == Xm8Ra::RaLibrarySyncState::Failed) {
		AddRaNotice("RA: library sync failed");
		ra_service->ClearLibrarySyncResult();
		return;
	}
	if (snapshot.state != Xm8Ra::RaLibrarySyncState::None ||
		ra_library_sync_started_for_login ||
		ra_session_state != Xm8Ra::RaSessionState::Ready ||
		!ra_pending_game_hash.empty() ||
		ra_service->LoginSnapshot().state != Xm8Ra::RaLoginState::LoggedIn ||
		ra_service->GameSessionSnapshot().state !=
			Xm8Ra::RaGameSessionState::NoGame) {
		return;
	}

	ra_library_sync_started_for_login = true;
	std::vector<Xm8Ra::RaMediaBankHash> media_hashes;
	std::string error;
	if (!ra_library->ListMediaBankHashes(&media_hashes, &error)) {
		AddRaNotice("RA: library sync preparation failed");
		return;
	}
	std::map<std::string, bool> unique_hashes;
	for (const Xm8Ra::RaMediaBankHash& media : media_hashes) {
		unique_hashes[media.ra_hash] = true;
	}
	std::vector<std::string> hashes;
	for (const auto& hash : unique_hashes) {
		hashes.push_back(hash.first);
	}
	if (!ra_service->BeginLibrarySync(hashes, &error)) {
		AddRaNotice("RA: library sync did not start");
		return;
	}
}

//
// RememberRaSourceDirForMountedDisk()
// remember original source dir for mounted RA working copy
//
bool App::RememberRaSourceDirForMountedDisk(int drive)
{
	if (drive < 0 || drive >= MAX_DRIVE || ra_library == NULL ||
		diskmgr[drive] == NULL || !diskmgr[drive]->IsOpen()) {
		return false;
	}

	Xm8Ra::D88MediaInfo media;
	if (!Xm8Ra::ProbeD88File(diskmgr[drive]->GetPath(), &media, nullptr)) {
		return false;
	}

	Xm8Ra::MediaHealthRecord record;
	if (!ra_library->LoadMediaHealthRecord(media.md5, &record, nullptr)) {
		return false;
	}
	const std::string dir = DirectoryOfPath(record.source_locator.c_str());
	if (dir.empty()) {
		return false;
	}
	disk_open_dir = dir;
	return true;
}

//
// RememberRaLaunchDriveForMountedDisk()
// persist an explicitly mounted disk/bank in the owning game's launch profile
//
bool App::RememberRaLaunchDriveForMountedDisk(int drive, std::string *error)
{
	if (drive < 0 || drive >= MAX_DRIVE || ra_library == NULL ||
		diskmgr[drive] == NULL || !diskmgr[drive]->IsOpen()) {
		return true;
	}

	Xm8Ra::D88MediaInfo media;
	if (!Xm8Ra::ProbeD88File(diskmgr[drive]->GetPath(), &media, error)) {
		return false;
	}
	Xm8Ra::MediaRecord record;
	std::string find_error;
	if (!ra_library->FindMedia(media.md5, &record, &find_error)) {
		if (!find_error.empty()) {
			if (error != NULL) {
				*error = find_error;
			}
			return false;
		}
		return true;
	}

	// Drive 2 is only part of this launch profile when Drive 1 belongs to
	// the same local game. This also covers two banks from one D88.
	if (drive == 1) {
		if (diskmgr[0] == NULL || !diskmgr[0]->IsOpen()) {
			return true;
		}
		Xm8Ra::D88MediaInfo anchor_media;
		Xm8Ra::MediaRecord anchor_record;
		if (!Xm8Ra::ProbeD88File(diskmgr[0]->GetPath(), &anchor_media, error)) {
			return false;
		}
		find_error.clear();
		if (!ra_library->FindMedia(anchor_media.md5, &anchor_record,
			&find_error)) {
			if (!find_error.empty()) {
				if (error != NULL) {
					*error = find_error;
				}
				return false;
			}
			return true;
		}
		if (anchor_record.game_id != record.game_id) {
			return true;
		}
	}

	Xm8Ra::LaunchProfile profile;
	if (!ra_library->LoadLaunchProfile(record.game_id, &profile, error)) {
		return false;
	}
	Xm8Ra::LaunchDrive& slot = profile.drives[drive];
	const int bank = diskmgr[drive]->GetBank();
	const bool is_anchor = drive == 0;
	if (slot.assigned && slot.media_md5 == media.md5 &&
		slot.bank_index == bank && slot.is_ra_anchor == is_anchor) {
		return true;
	}

	if (is_anchor) {
		profile.drives[1].is_ra_anchor = false;
	}
	slot.assigned = true;
	slot.media_md5 = media.md5;
	slot.bank_index = bank;
	slot.is_ra_anchor = is_anchor;
	return ra_library->SaveLaunchProfile(profile, error);
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
	ra_options.credentials_store =
		Xm8Ra::CreatePlatformRaCredentialsStore(ra_options.ra_root);
	ra_options.http_client =
		Xm8Ra::CreatePlatformRaHttpClient(ra_options.user_agent);
	if (ra_options.http_client == nullptr) {
		if (error != NULL) {
			*error = "RA HTTP backend is not available";
		}
		return false;
	}
	ra_options.host_read_memory = ReadRaMemoryFromApp;
	ra_options.host_read_memory_userdata = this;
	ra_service = new Xm8Ra::RaService(std::move(ra_options));
	if (!ra_service->IsReady()) {
		delete ra_service;
		ra_service = NULL;
		if (error != NULL) {
			*error = "RA service is not ready";
		}
		return false;
	}
	ra_connectivity_monitor = Xm8Ra::CreatePlatformRaConnectivityMonitor();
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

bool App::SaveRaPlayModeSetting(Xm8Ra::RaPlayMode mode, std::string *error)
{
	if (ra_library == NULL) {
		if (error != NULL) *error = "RA library is not available";
		return false;
	}
	Xm8Ra::RaSettings settings;
	if (!ra_library->LoadSettings(&settings, error)) return false;
	settings.last_mode = mode == Xm8Ra::RaPlayMode::Hardcore ?
		Xm8Ra::kRaModeHardcore : Xm8Ra::kRaModeSoftcore;
	return ra_library->SaveSettings(settings, error);
}

Xm8Ra::RaSessionPolicyContext App::GetRaPolicyContext() const
{
	Xm8Ra::RaSessionPolicyContext context;
	context.ra_enabled = ra_mode_enabled;
	context.selected_mode = ra_play_mode;
	context.session_state = ra_session_state;
	return context;
}

bool App::CheckRaOperation(Xm8Ra::RaRestrictedOperation operation,
	const char *notice)
{
	if (Xm8Ra::IsRaOperationAllowed(GetRaPolicyContext(), operation)) {
		return true;
	}
	AddRaNotice(notice);
	return false;
}

void App::ApplyRaOnlineRestrictions()
{
	if (!Xm8Ra::IsRaOnlineSession(GetRaPolicyContext())) return;
	if (!ra_fast_disk_override_active) {
		ra_saved_fast_disk = setting->IsFastDisk();
		ra_fast_disk_override_active = true;
	}
	setting->SetFastDisk(false);
	if (IsRaHardcoreActive() && app_fullspeed) NormalSpeed();
}

void App::RestoreRaSessionOverrides()
{
	if (!ra_fast_disk_override_active || setting == NULL) return;
	setting->SetFastDisk(ra_saved_fast_disk);
	ra_fast_disk_override_active = false;
}

//
// BeginRaSavedTokenLogin()
// begin saved token login if possible
//
bool App::BeginRaSavedTokenLogin(bool notify_missing_token)
{
	if (ra_service == NULL) {
		if (notify_missing_token) {
			AddRaNotice("RA: service unavailable");
		}
		return false;
	}
	if (IsRaLoggedIn()) {
		if (notify_missing_token) {
			AddRaNotice("RA: already logged in");
		}
		return true;
	}

	std::string error;
	if (!ra_service->BeginLoginWithSavedToken(&error)) {
		if (notify_missing_token) {
			AddRaNotice("RA: login required");
		}
		return false;
	}

	ra_saved_login_started = true;
	ra_manual_login_started = false;
	ra_library_sync_started_for_login = false;
	return true;
}

//
// StartRaAfterBoot()
// start RA after startup disks and auto-resume are settled
//
void App::StartRaAfterBoot()
{
	if (!ra_mode_enabled || ra_library == NULL) {
		return;
	}

	std::string error;
	if (!EnsureRaService(&error)) {
		ra_mode_enabled = false;
		ra_menu_status.Set(Xm8Ra::RaMenuStatusState::Disabled);
		AddRaNotice("RA: service unavailable");
		menu->UpdateRaStatus();
		return;
	}

	BeginRaSavedTokenLogin(false);
	BeginRaSessionForMountedDrive1();
	menu->UpdateRaStatus();
}

//
// BeginRaSessionForMedia()
// remember the media hash to identify through RA
//
void App::BeginRaSessionForMedia(const std::string& md5, int64_t game_id)
{
	if (!ra_mode_enabled || ra_service == NULL || md5.empty()) {
		return;
	}

	StopRaSession();
	if (ra_overlay != NULL) {
		ra_overlay->ClearLastSubmissionError();
	}
	ra_session_state = Xm8Ra::TransitionRaSession(ra_session_state,
		Xm8Ra::RaSessionSignal::BeginLaunch);
	ApplyRaOnlineRestrictions();
	if (audio != NULL) CtrlAudio();
	if (ra_service != NULL) {
		const Xm8Ra::RaLibrarySyncState sync_state =
			ra_service->LibrarySyncSnapshot().state;
		if (sync_state == Xm8Ra::RaLibrarySyncState::PendingHashes ||
			sync_state == Xm8Ra::RaLibrarySyncState::PendingTitles ||
			sync_state == Xm8Ra::RaLibrarySyncState::PendingProgress) {
			ra_library_sync_started_for_login = false;
		}
		ra_service->UnloadGame();
		ra_service->SetHardcoreEnabled(
			ra_play_mode == Xm8Ra::RaPlayMode::Hardcore);
	}
	const Xm8Ra::RaLoginState login_state =
		ra_service->LoginSnapshot().state;
	const char *login_text = login_state == Xm8Ra::RaLoginState::LoggedOut ?
		"out" : login_state == Xm8Ra::RaLoginState::LoginPending ?
		"pending" : login_state == Xm8Ra::RaLoginState::LoggedIn ?
		"in" : "failed";
	ra_menu_status.Set(Xm8Ra::RaMenuStatusState::PendingGame,
		md5.substr(0, 8) + " " + login_text);
	ra_pending_game_hash = md5;
	ra_pending_library_game_id = game_id;
	ra_loaded_library_game_id = 0;
	ra_loaded_game_hash.clear();
	ClearRaMediaChangeState();
	ra_leaderboard_scoreboards.clear();
	RefreshRaAchievementsOverlay();
	RefreshRaLeaderboardsOverlay();
}

//
// BeginRaSessionForMountedDrive1()
// begin RA session for already-mounted drive 1
//
void App::BeginRaSessionForMountedDrive1()
{
	if (!ra_mode_enabled || ra_service == NULL || diskmgr[0] == NULL ||
		!diskmgr[0]->IsOpen() || !ra_pending_game_hash.empty()) {
		return;
	}

	Xm8Ra::D88MediaInfo media;
	std::string error;
	if (!Xm8Ra::ProbeD88File(diskmgr[0]->GetPath(), &media, &error)) {
		AddRaNotice("RA: mounted disk probe failed");
		return;
	}

	const int bank = diskmgr[0]->GetBank();
	if (bank < 0 || bank >= static_cast<int>(media.bank_md5s.size())) {
		AddRaNotice("RA: mounted bank hash missing");
		return;
	}

	const std::string& hash = media.bank_md5s[bank];
	if (ra_loaded_game_hash == hash &&
		ra_service->GameSessionSnapshot().state ==
			Xm8Ra::RaGameSessionState::Loaded) {
		return;
	}
	int64_t game_id = 0;
	if (ra_library != NULL) {
		Xm8Ra::MediaRecord record;
		if (ra_library->FindMedia(media.md5, &record, nullptr)) {
			game_id = record.game_id;
		}
	}
	BeginRaSessionForMedia(hash, game_id);
}

//
// RaHostFrameComplete()
// receive the generic EVENT frame completion callback
//
void App::RaHostFrameComplete(void *userdata)
{
	App *app = static_cast<App *>(userdata);
	if (app != NULL) {
		app->ProcessRaEmulationFrame();
	}
}

//
// AttachRaHostFrameCallback()
// connect the current EVENT instance to the RA frame evaluator
//
void App::AttachRaHostFrameCallback()
{
	if (evmgr != NULL) {
		evmgr->set_host_frame_callback(RaHostFrameComplete, this);
	}
}

//
// ProcessRaEmulationFrame()
// evaluate exactly one completed VM frame without touching SDL or UI state
//
void App::ProcessRaEmulationFrame()
{
	if (!ra_mode_enabled ||
		!Xm8Ra::IsRaSessionEvaluating(ra_session_state) ||
		ra_service == NULL ||
		ra_media_change_pending) {
		return;
	}
	ra_service->DoFrame();
}

//
// ProcessRaService()
// progress RA HTTP, login, game load, events, and optional idle processing
//
void App::ProcessRaService(bool emulation_idle)
{
	if (!ra_mode_enabled || ra_service == NULL) {
		return;
	}

	ProcessRaImages();
	ProcessRaConnectivity();

	const Xm8Ra::RaLoginState login_state_before =
		ra_service->LoginSnapshot().state;
	const Xm8Ra::RaGameSessionState game_state_before =
		ra_service->GameSessionSnapshot().state;
	const Xm8Ra::RaLeaderboardEntriesSnapshot entries_before =
		ra_service->LeaderboardEntriesSnapshot();
	ra_service->DrainHttp();
	ProcessRaMediaChange();
	const Xm8Ra::RaLoginSnapshot login_after_drain =
		ra_service->LoginSnapshot();
	const Xm8Ra::RaGameSessionSnapshot game_after_drain =
		ra_service->GameSessionSnapshot();
	const Xm8Ra::RaLeaderboardEntriesSnapshot entries_after_drain =
		ra_service->LeaderboardEntriesSnapshot();
	if (entries_before.leaderboard_id != entries_after_drain.leaderboard_id ||
		entries_before.state != entries_after_drain.state ||
		entries_before.entries.size() != entries_after_drain.entries.size()) {
		RefreshRaLeaderboardsOverlay();
	}
	if (ra_saved_login_started &&
		login_after_drain.state != Xm8Ra::RaLoginState::LoginPending) {
		ra_saved_login_started = false;
		if (login_after_drain.state == Xm8Ra::RaLoginState::LoggedIn) {
			const std::string name = login_after_drain.display_name.empty() ?
				login_after_drain.username : login_after_drain.display_name;
			ReplaceRaNotice(name.empty() ? "RA: logged in" :
				"RA: logged in " + name);
		}
		else if (login_after_drain.state == Xm8Ra::RaLoginState::Failed) {
			ReplaceRaNotice("RA: login failed");
		}
		RefreshRaAchievementsOverlay();
	}
	if (login_after_drain.state != login_state_before ||
		game_after_drain.state != game_state_before) {
		if (login_after_drain.state == Xm8Ra::RaLoginState::LoginPending) {
			ra_menu_status.Set(Xm8Ra::RaMenuStatusState::LoginPending);
		}
		else if (login_after_drain.state == Xm8Ra::RaLoginState::LoggedIn &&
			game_after_drain.state != Xm8Ra::RaGameSessionState::Loaded) {
			if (ra_session_state == Xm8Ra::RaSessionState::Starting &&
				!ra_pending_game_hash.empty()) {
				ra_menu_status.Set(Xm8Ra::RaMenuStatusState::PendingGame,
					ra_pending_game_hash.substr(0, 8) + " in");
			}
			else {
				const std::string name = login_after_drain.display_name.empty() ?
					login_after_drain.username : login_after_drain.display_name;
				ra_menu_status.Set(Xm8Ra::RaMenuStatusState::LoggedIn, name);
			}
		}
		else if (login_after_drain.state == Xm8Ra::RaLoginState::Failed) {
			ra_menu_status.Set(Xm8Ra::RaMenuStatusState::LoginFailed);
		}
		menu->UpdateRaStatus();
	}
	if (ra_manual_login_started) {
		const Xm8Ra::RaLoginSnapshot login = ra_service->LoginSnapshot();
		if (login.state == Xm8Ra::RaLoginState::LoggedIn) {
			ra_manual_login_started = false;
			if (ra_overlay != NULL &&
				ra_overlay->Screen() == Xm8Ra::RaOverlayScreen::Login) {
				CloseRaOverlayToMenu();
			}
			const std::string name = login.display_name.empty() ?
				login.username : login.display_name;
			ReplaceRaNotice(name.empty() ? "RA: logged in" :
				"RA: logged in " + name);
			RefreshRaAchievementsOverlay();
			menu->UpdateRaStatus();
		}
		else if (login.state == Xm8Ra::RaLoginState::Failed) {
			ra_manual_login_started = false;
			if (ra_overlay != NULL &&
				ra_overlay->Screen() == Xm8Ra::RaOverlayScreen::Login) {
				ra_overlay->SetLoginStatus(login.message.empty() ?
					"Login failed" : login.message);
				UpdateRaOverlayTextInput();
			}
			ReplaceRaNotice("RA: login failed");
			RefreshRaAchievementsOverlay();
			menu->UpdateRaStatus();
		}
	}

	if (ra_session_state == Xm8Ra::RaSessionState::Starting &&
		!ra_pending_game_hash.empty()) {
		const Xm8Ra::RaLoginSnapshot login = ra_service->LoginSnapshot();
		const Xm8Ra::RaGameSessionSnapshot game =
			ra_service->GameSessionSnapshot();

		if (login.state == Xm8Ra::RaLoginState::LoggedOut &&
			!ra_saved_login_started) {
			std::string error;
			ra_saved_login_started =
				ra_service->BeginLoginWithSavedToken(&error);
			if (!ra_saved_login_started) {
				EnterRaOfflineSession("login required");
			}
		}
		else if (login.state == Xm8Ra::RaLoginState::LoggedIn &&
			game.state == Xm8Ra::RaGameSessionState::NoGame) {
			std::string error;
			if (ra_service->BeginLoadGameByHash(ra_pending_game_hash,
				&error)) {
				ra_loaded_game_hash = ra_pending_game_hash;
				menu->UpdateRaStatus();
			}
			else {
				EnterRaOfflineSession("game load failed");
			}
		}
		else if (game.state == Xm8Ra::RaGameSessionState::Loaded) {
			// The VM has already been cold-reset for this launch. Establish the
			// matching rcheevos frame-zero state exactly once.
			ra_service->ResetProgress();
			ra_session_state = Xm8Ra::TransitionRaSession(ra_session_state,
				Xm8Ra::RaSessionSignal::LaunchSucceeded);
			ra_loaded_library_game_id = ra_pending_library_game_id;
			if (ra_pending_library_game_id > 0 && ra_library != NULL) {
				std::string error;
				if (!ra_library->MarkGameIdentified(
					ra_pending_library_game_id, game.game_id, game.title,
					game.badge_url, &error)) {
					AddRaNotice("RA: library update failed");
				}
			}
			ra_menu_status.Set(Xm8Ra::RaMenuStatusState::ActiveGame, game.title);
			ra_pending_game_hash.clear();
			ra_pending_library_game_id = 0;
			AddRaNotice(game.title.empty() ? "RA: identified" :
				"RA: identified " + game.title);
			RefreshRaAchievementsOverlay();
			menu->UpdateRaStatus();
		}
		else if (login.state == Xm8Ra::RaLoginState::Failed ||
			game.state == Xm8Ra::RaGameSessionState::DisabledForSession) {
			if (game.state == Xm8Ra::RaGameSessionState::DisabledForSession) {
				ra_menu_status.Set(Xm8Ra::RaMenuStatusState::UnknownGame);
			}
			const std::string failure =
				login.state == Xm8Ra::RaLoginState::Failed ?
				"login failed" : RaGameLoadFailureNotice(game).substr(4);
			EnterRaOfflineSession(failure);
		}
	}

	if (!Xm8Ra::IsRaSessionOffline(ra_session_state) &&
		(emulation_idle ||
		ra_service->GameSessionSnapshot().state !=
			Xm8Ra::RaGameSessionState::Loaded)) {
		ra_service->Idle();
	}
	ProcessRaLibrarySync();
	AddRaEventsAsNotices(ra_service->TakeEvents());
	HandleRaResetRequest();
}

//
// ProcessRaConnectivity()
// map platform reachability changes into the active RA session
//
void App::ProcessRaConnectivity()
{
	if (ra_connectivity_monitor == nullptr) {
		return;
	}
	const Xm8Ra::RaReachabilityTransition transition =
		ra_connectivity_tracker.Observe(ra_connectivity_monitor->Poll());
	if (!transition.has_signal) {
		return;
	}
	const Xm8Ra::RaSessionState previous = ra_session_state;
	ra_session_state = Xm8Ra::TransitionRaSession(ra_session_state,
		transition.signal);
	if (ra_session_state == previous) {
		return;
	}
	SetRaMenuStatusForConnectivity(
		transition.signal == Xm8Ra::RaSessionSignal::Disconnected);
	AddRaNotice(transition.signal == Xm8Ra::RaSessionSignal::Disconnected ?
		"RA: disconnected" : "RA: reconnected");
	menu->UpdateRaStatus();
}

//
// ProcessRaImages()
// progress RA badge image HTTP
//
void App::ProcessRaImages()
{
	if (ra_image_http_client == nullptr) {
		return;
	}

	std::vector<Xm8Ra::RaHttpResponse> completed;
	ra_image_http_client->DrainCompleted(&completed);
	for (const Xm8Ra::RaHttpResponse& response : completed) {
		for (auto& entry : ra_badge_images) {
			RaBadgeImage& image = entry.second;
			if (image.request_id != response.request_id ||
				image.state != RaBadgeImage::Pending) {
				continue;
			}

			image.request_id = 0;
			if (response.transport_result !=
				Xm8Ra::RaHttpTransportResult::Success ||
				response.http_status < 200 || response.http_status >= 300 ||
				response.body.empty()) {
				image.state = RaBadgeImage::Failed;
				break;
			}

			int width = 0;
			int height = 0;
			std::vector<uint32_t> pixels;
			if (!DecodeRaBadgePixels(response.body, &width, &height, &pixels)) {
				image.state = RaBadgeImage::Failed;
				break;
			}

			image.width = width;
			image.height = height;
			image.pixels = std::move(pixels);
			image.state = image.pixels.empty() ? RaBadgeImage::Failed :
				RaBadgeImage::Ready;
			if (image.state == RaBadgeImage::Ready && ra_library != nullptr) {
				std::vector<std::string> protected_urls;
				const uint32_t ticks = SDL_GetTicks();
				for (const auto& candidate : ra_badge_images) {
					if (candidate.second.last_draw_ticks != 0 &&
						static_cast<int32_t>(ticks -
							candidate.second.last_draw_ticks) < 1000) {
						protected_urls.push_back(candidate.first);
					}
				}
				std::string cache_error;
				ra_library->StoreCachedImage(entry.first, image.image_kind,
					response.content_type, response.body,
					static_cast<int64_t>(std::time(nullptr)),
					ra_image_cache_limit_bytes, protected_urls, &cache_error);
			}
			break;
		}
	}
}

//
// RequestRaBadgeImage()
// request RA badge image if needed
//
void App::RequestRaBadgeImage(const std::string& url,
	Xm8Ra::RaImageKind image_kind)
{
	if (url.empty()) {
		return;
	}

	RaBadgeImage& image = ra_badge_images[url];
	if (image.state != RaBadgeImage::NotRequested) {
		return;
	}
	image.image_kind = image_kind;
	if (ra_library != nullptr) {
		std::vector<uint8_t> cached;
		std::string content_type;
		std::string cache_error;
		const Xm8Ra::RaImageCacheLoadResult result =
			ra_library->LoadCachedImage(url,
				static_cast<int64_t>(std::time(nullptr)), &cached,
				&content_type, &cache_error);
		if (result == Xm8Ra::RaImageCacheLoadResult::Hit &&
			DecodeRaBadgePixels(cached, &image.width, &image.height,
				&image.pixels)) {
			image.state = RaBadgeImage::Ready;
			return;
		}
	}

	if (ra_image_http_client == nullptr) {
		ra_image_http_client =
			Xm8Ra::CreatePlatformRaHttpClient(MakeRaUserAgent());
		if (ra_image_http_client == nullptr) {
			image.state = RaBadgeImage::Failed;
			return;
		}
	}

	Xm8Ra::RaHttpRequest request;
	request.request_id = ra_next_image_request_id++;
	request.purpose = Xm8Ra::RaHttpPurpose::Image;
	request.url = url;
	request.max_response_bytes = 1024U * 1024U;
	request.connect_timeout_ms = 10000;
	request.total_timeout_ms = 30000;
	image.request_id = request.request_id;
	image.state = RaBadgeImage::Pending;
	ra_image_http_client->Send(request);
}

//
// DrawRaBadgeImage()
// draw RA badge image if cached
//
void App::DrawRaBadgeImage(Uint32 *buf, SDL_Rect *rect,
	const std::string& url, Xm8Ra::RaImageKind image_kind,
	bool show_placeholder_text)
{
	if (buf == NULL || rect == NULL) {
		return;
	}
	const char *placeholder = image_kind == Xm8Ra::RaImageKind::GameBadge ?
		"Game" : image_kind == Xm8Ra::RaImageKind::AchievementBadgeLocked ?
		"Locked" : "Badge";
	if (url.empty()) {
		if (show_placeholder_text) {
			font->DrawSjisCenterOr(buf, rect, placeholder,
				RGB_COLOR(255, 255, 255));
		}
		else {
			font->DrawRect(buf, rect, RGB_COLOR(127, 127, 127),
				RGB_COLOR(0, 0, 0));
		}
		return;
	}

	RequestRaBadgeImage(url, image_kind);
	const auto found = ra_badge_images.find(url);
	if (found != ra_badge_images.end()) {
		found->second.last_draw_ticks = SDL_GetTicks();
	}
	if (found == ra_badge_images.end() ||
		found->second.state != RaBadgeImage::Ready ||
		found->second.width <= 0 || found->second.height <= 0 ||
		found->second.pixels.empty()) {
		if (show_placeholder_text) {
			font->DrawSjisCenterOr(buf, rect, placeholder,
				RGB_COLOR(255, 255, 255));
		}
		else {
			font->DrawRect(buf, rect, RGB_COLOR(127, 127, 127),
				RGB_COLOR(0, 0, 0));
		}
		return;
	}

	const RaBadgeImage& image = found->second;
	for (int y = 0; y < rect->h; ++y) {
		const int sy = y * image.height / rect->h;
		for (int x = 0; x < rect->w; ++x) {
			const int sx = x * image.width / rect->w;
			const uint32_t source =
				image.pixels[static_cast<size_t>(sy) *
					static_cast<size_t>(image.width) +
					static_cast<size_t>(sx)];
			const uint32_t alpha = source >> 24;
			if (alpha == 0) {
				continue;
			}
			Uint32 *dest = buf + (rect->y + y) * SCREEN_WIDTH +
				rect->x + x;
			if (alpha >= 255) {
				*dest = source;
			}
			else {
				const uint32_t inv = 255 - alpha;
				const uint32_t sr = (source >> 16) & 0xff;
				const uint32_t sg = (source >> 8) & 0xff;
				const uint32_t sb = source & 0xff;
				const uint32_t dr = (*dest >> 16) & 0xff;
				const uint32_t dg = (*dest >> 8) & 0xff;
				const uint32_t db = *dest & 0xff;
				*dest = 0xff000000 |
					RGB_COLOR((sr * alpha + dr * inv) / 255,
						(sg * alpha + dg * inv) / 255,
						(sb * alpha + db * inv) / 255);
			}
		}
	}
}

//
// AddRaNotice()
// add RA overlay notice
//
void App::AddRaNotice(const std::string& text,
	Xm8Ra::RaNoticePriority priority, const std::string& badge_url)
{
	if (ra_overlay == NULL) {
		return;
	}
	ra_overlay->AddNotice(text, SDL_GetTicks(),
		ra_notification_duration_ms, priority, badge_url);
}

void App::ReplaceRaNotice(const std::string& text,
	Xm8Ra::RaNoticePriority priority)
{
	if (ra_overlay == NULL) {
		return;
	}
	ra_overlay->ReplaceNotice(text, SDL_GetTicks(),
		ra_notification_duration_ms, priority);
}

//
// AddRaEventsAsNotices()
// translate RA events to transient notices
//
void App::AddRaEventsAsNotices(const std::vector<Xm8Ra::RaEvent>& events)
{
	bool leaderboards_changed = false;
	bool session_state_changed = false;
	for (const Xm8Ra::RaEvent& event : events) {
		const uint32_t event_now = SDL_GetTicks();
		if (event.type == Xm8Ra::RaEventType::Disconnected ||
			event.type == Xm8Ra::RaEventType::Reconnected) {
			const Xm8Ra::RaSessionState previous = ra_session_state;
			ra_session_state = Xm8Ra::TransitionRaSession(ra_session_state,
				event.type == Xm8Ra::RaEventType::Disconnected ?
				Xm8Ra::RaSessionSignal::Disconnected :
				Xm8Ra::RaSessionSignal::Reconnected);
			if (ra_session_state == previous) {
				continue;
			}
			SetRaMenuStatusForConnectivity(
				event.type == Xm8Ra::RaEventType::Disconnected);
			session_state_changed = true;
		}
		if (event.type == Xm8Ra::RaEventType::LeaderboardScoreboard &&
			event.scoreboard.leaderboard_id != 0) {
			ra_leaderboard_scoreboards[event.scoreboard.leaderboard_id] =
				event.scoreboard;
			leaderboards_changed = true;
		}
		if (event.type == Xm8Ra::RaEventType::ServerError) {
			ra_overlay->SetLastSubmissionError(
				RaSubmissionErrorText(event.server_error));
			if (ra_session_state == Xm8Ra::RaSessionState::Ready) {
				ra_menu_status.Set(Xm8Ra::RaMenuStatusState::SubmissionError,
					RaSubmissionErrorText(event.server_error));
			}
			session_state_changed = true;
		}
		switch (event.type) {
		case Xm8Ra::RaEventType::AchievementChallengeIndicatorShow:
			ra_overlay->ShowChallenge(event.achievement.id,
				event.achievement.title, event.achievement.badge_url,
				event_now);
			break;
		case Xm8Ra::RaEventType::AchievementChallengeIndicatorHide:
			ra_overlay->HideChallenge(event.achievement.id, event_now);
			break;
		case Xm8Ra::RaEventType::AchievementProgressIndicatorShow:
		case Xm8Ra::RaEventType::AchievementProgressIndicatorUpdate: {
			std::string progress = event.achievement.measured_progress;
			if (progress.empty()) {
				char percent[32];
				std::snprintf(percent, sizeof(percent), "%.0f%%",
					event.achievement.measured_percent);
				progress = percent;
			}
			if (event.type ==
				Xm8Ra::RaEventType::AchievementProgressIndicatorShow) {
				ra_overlay->ShowProgress(event.achievement.id,
					event.achievement.title, progress,
					event.achievement.badge_url, event_now);
			}
			else {
				ra_overlay->UpdateProgress(event.achievement.id,
					event.achievement.title, progress,
					event.achievement.badge_url, event_now);
			}
			break;
		}
		case Xm8Ra::RaEventType::AchievementProgressIndicatorHide:
			ra_overlay->HideProgress(event_now);
			break;
		case Xm8Ra::RaEventType::LeaderboardTrackerShow:
			ra_overlay->ShowLeaderboardTracker(event.leaderboard.id,
				event.leaderboard.display, event_now);
			break;
		case Xm8Ra::RaEventType::LeaderboardTrackerUpdate:
			ra_overlay->UpdateLeaderboardTracker(event.leaderboard.id,
				event.leaderboard.display, event_now);
			break;
		case Xm8Ra::RaEventType::LeaderboardTrackerHide:
			ra_overlay->HideLeaderboardTracker(event.leaderboard.id,
				event_now);
			break;
		case Xm8Ra::RaEventType::ResetRequested:
			ra_reset_requested = true;
			break;
		default:
			break;
		}
		std::string notice = RaEventNotice(event);
		if (event.type == Xm8Ra::RaEventType::AchievementTriggered &&
			IsRaHardcoreActive() && !notice.empty()) {
			notice += " [Hardcore]";
		}
		if (!notice.empty()) {
			AddRaNotice(notice, RaEventNoticePriority(event),
				event.type == Xm8Ra::RaEventType::AchievementTriggered ?
					event.achievement.badge_url : std::string());
		}
	}
	if (leaderboards_changed) {
		RefreshRaLeaderboardsOverlay();
	}
	if (session_state_changed) {
		menu->UpdateRaStatus();
	}
}

void App::HandleRaResetRequest()
{
	if (!ra_reset_requested) return;
	ra_reset_requested = false;
	Reset();
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
#ifdef __ANDROID__
	case SDL_SCANCODE_AC_BACK:
#endif
		key = Xm8Ra::RaOverlayKey::Escape;
		break;
	case SDL_SCANCODE_UP:
		key = Xm8Ra::RaOverlayKey::Up;
		break;
	case SDL_SCANCODE_DOWN:
		key = Xm8Ra::RaOverlayKey::Down;
		break;
	case SDL_SCANCODE_LEFT:
		key = Xm8Ra::RaOverlayKey::Left;
		break;
	case SDL_SCANCODE_RIGHT:
		key = Xm8Ra::RaOverlayKey::Right;
		break;
	case SDL_SCANCODE_PAGEUP:
		key = Xm8Ra::RaOverlayKey::PageUp;
		break;
	case SDL_SCANCODE_PAGEDOWN:
		key = Xm8Ra::RaOverlayKey::PageDown;
		break;
	default:
		return true;
	}

	return HandleRaOverlayAction(ra_overlay->OnControlKey(key));
}

//
// HandleRaOverlayAction()
// handle RA overlay action
//
bool App::HandleRaOverlayAction(Xm8Ra::RaOverlayAction action)
{
	if (action == Xm8Ra::RaOverlayAction::SubmitLogin) {
		SubmitRaOverlayLogin();
	}
	else if (action == Xm8Ra::RaOverlayAction::OpenLibraryGame) {
		int64_t game_id = 0;
		std::string error;
		if (ra_overlay == NULL ||
			!ra_overlay->SelectedLibraryGameId(&game_id) ||
			!LaunchRaLibraryGame(game_id, &error)) {
			AddRaNotice(error.empty() ? "RA: launch failed" :
				("RA: " + error));
		}
		else {
			ra_overlay->CloseScreen();
			SDL_StopTextInput();
			ClearRaOverlayPointerState();
			input->LostFocus();
			input->ResetList();
			CtrlAudio();
			video->SetMenuMode(false);
			video->DrawCtrl();
		}
	}
	else if (action == Xm8Ra::RaOverlayAction::ResolveLibraryConflict) {
		int64_t game_id = 0;
		std::string error;
		Xm8Ra::RaGameConflictInfo resolved;
		if (ra_loaded_library_game_id != 0) {
			error = "stop the current library game before resolving media";
		}
		else if (ra_overlay == NULL || ra_library == NULL ||
			!ra_overlay->SelectedLibraryGameId(&game_id) ||
			!ra_library->ResolveGameConflict(game_id, &resolved, &error)) {
			if (error.empty()) error = "media conflict resolution failed";
		}
		if (!error.empty()) {
			AddRaNotice("RA: " + error);
		}
		else {
			ra_overlay->OpenLibrary(MakeRaLibraryOverlaySnapshot());
			AddRaNotice(resolved.kind == Xm8Ra::RaGameConflictKind::Merge ?
				"RA: games merged" : "RA: media split");
		}
	}
	else if (action == Xm8Ra::RaOverlayAction::Close) {
		CloseRaOverlayToMenu();
	}
	else {
		UpdateRaOverlayTextInput();
	}
	EnsureRaLeaderboardEntriesForSelection();
	return true;
}

//
// UpdateRaOverlayTextInput()
// update SDL text input for RA overlay
//
void App::UpdateRaOverlayTextInput()
{
	if (ra_overlay == NULL ||
		ra_overlay->Screen() != Xm8Ra::RaOverlayScreen::Login) {
		SDL_StopTextInput();
		return;
	}

	const Xm8Ra::RaOverlayLoginSnapshot login =
		ra_overlay->LoginSnapshot();
	if (login.focus == Xm8Ra::RaOverlayLoginTarget::Username ||
		login.focus == Xm8Ra::RaOverlayLoginTarget::Password) {
		SDL_StartTextInput();
	}
	else {
		SDL_StopTextInput();
	}
}

//
// ClearRaOverlayPointerState()
// clear RA overlay pointer state
//
void App::ClearRaOverlayPointerState()
{
	ra_overlay_mouse_target_valid = false;
	ra_overlay_mouse_detail_target = 0;
	ra_overlay_mouse_list_target_valid = false;
	ra_overlay_finger_target_valid = false;
	ra_overlay_finger_detail_target = 0;
	ra_overlay_finger_list_target_valid = false;
	ra_overlay_finger_scroll_valid = false;
	ra_overlay_finger_scrolled = false;
	ra_overlay_finger_scroll_y = 0;
	ra_status_mouse_pressed = false;
	ra_status_mouse_dragged = false;
	ra_status_finger_pressed = false;
	ra_status_finger_dragged = false;
	ra_status_finger_id = -1;
	ra_status_finger_start_x = 0;
	ra_status_finger_start_y = 0;
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
// HandleRaStatusMouse()
// cycle RA status pages without consuming game-area input
//
bool App::HandleRaStatusMouse(SDL_Event *e)
{
	if (!ra_mode_enabled || !setting->HasStatusLine()) {
		ra_status_mouse_pressed = false;
		ra_status_mouse_dragged = false;
		return false;
	}

	if (e->type == SDL_MOUSEMOTION) {
		if (!ra_status_mouse_pressed) {
			return false;
		}
		int x = e->motion.x;
		int y = e->motion.y;
		if (!video->ConvertPoint(&x, &y) ||
			!video->IsDedicatedStatusPoint(x, y)) {
			ra_status_mouse_dragged = true;
		}
		return true;
	}
	if (e->type != SDL_MOUSEBUTTONDOWN && e->type != SDL_MOUSEBUTTONUP) {
		return false;
	}
	if (e->button.which == SDL_TOUCH_MOUSEID ||
		e->button.button != SDL_BUTTON_LEFT) {
		return false;
	}

	int x = e->button.x;
	int y = e->button.y;
	const bool in_status = video->ConvertPoint(&x, &y) &&
		video->IsDedicatedStatusPoint(x, y);
	const uint32_t now = SDL_GetTicks();
	const bool notice_visible = ra_overlay->HasVisibleNotice(now);
	const bool status_visible = notice_visible ||
		ra_overlay->StatusPageCount() != 0;
	const bool can_cycle = !notice_visible &&
		ra_overlay->StatusPageCount() > 1;

	if (e->type == SDL_MOUSEBUTTONDOWN) {
		ra_status_mouse_pressed = in_status && status_visible;
		ra_status_mouse_dragged = !can_cycle;
		return in_status && status_visible;
	}

	const bool activate = ra_status_mouse_pressed &&
		!ra_status_mouse_dragged && in_status && can_cycle;
	const bool consume = ra_status_mouse_pressed ||
		(in_status && status_visible);
	ra_status_mouse_pressed = false;
	ra_status_mouse_dragged = false;
	if (activate && ra_overlay->NextStatusPage(now)) {
		video->DrawCtrl();
	}
	return consume;
}

//
// HandleRaStatusFinger()
// cycle RA status pages only in the dedicated status area
//
bool App::HandleRaStatusFinger(SDL_Event *e)
{
	if (!ra_mode_enabled || !setting->HasStatusLine()) {
		ra_status_finger_pressed = false;
		ra_status_finger_dragged = false;
		ra_status_finger_id = -1;
		return false;
	}
	if (e->type != SDL_FINGERDOWN && e->type != SDL_FINGERUP &&
		e->type != SDL_FINGERMOTION) {
		return false;
	}

	int x = 0;
	int y = 0;
	const bool converted = video->ConvertFinger(e->tfinger.x,
		e->tfinger.y, &x, &y);
	const bool in_status = converted && video->IsDedicatedStatusPoint(x, y);
	const uint32_t now = SDL_GetTicks();
	const bool notice_visible = ra_overlay->HasVisibleNotice(now);
	const bool status_visible = notice_visible ||
		ra_overlay->StatusPageCount() != 0;
	const bool can_cycle = !notice_visible &&
		ra_overlay->StatusPageCount() > 1;

	if (e->type == SDL_FINGERDOWN) {
		ra_status_finger_pressed = in_status && status_visible;
		ra_status_finger_dragged = !can_cycle;
		ra_status_finger_id = ra_status_finger_pressed ?
			e->tfinger.fingerId : -1;
		ra_status_finger_start_x = x;
		ra_status_finger_start_y = y;
		return in_status && status_visible;
	}

	if (!ra_status_finger_pressed ||
		ra_status_finger_id != e->tfinger.fingerId) {
		return in_status && status_visible;
	}
	if (e->type == SDL_FINGERMOTION) {
		const int dx = x - ra_status_finger_start_x;
		const int dy = y - ra_status_finger_start_y;
		if (!in_status || dx > 4 || dx < -4 || dy > 4 || dy < -4) {
			ra_status_finger_dragged = true;
		}
		return true;
	}

	const bool activate = !ra_status_finger_dragged && in_status &&
		can_cycle;
	ra_status_finger_pressed = false;
	ra_status_finger_dragged = false;
	ra_status_finger_id = -1;
	if (activate && ra_overlay->NextStatusPage(now)) {
		video->DrawCtrl();
	}
	return true;
}

//
// HandleRaOverlayMouse()
// handle RA overlay mouse input
//
bool App::HandleRaOverlayMouse(SDL_Event *e)
{
	if (ra_overlay == NULL) {
		return false;
	}
	if (!ra_overlay->IsBlocking()) {
		return HandleRaStatusMouse(e);
	}
	if (e->type == SDL_MOUSEMOTION) {
		if (e->motion.which == SDL_TOUCH_MOUSEID) {
			return true;
		}
		if (ra_overlay->Screen() == Xm8Ra::RaOverlayScreen::Library ||
			ra_overlay->Screen() == Xm8Ra::RaOverlayScreen::Achievements ||
			ra_overlay->Screen() == Xm8Ra::RaOverlayScreen::Leaderboards) {
			int x = e->motion.x;
			int y = e->motion.y;
			if (video->ConvertPoint(&x, &y)) {
				return HandleRaOverlayAction(
					ra_overlay->OnListPointer(x, y, false));
			}
		}
		return true;
	}
	if (e->type == SDL_MOUSEWHEEL) {
		if (ra_overlay->Screen() == Xm8Ra::RaOverlayScreen::Library ||
			ra_overlay->Screen() == Xm8Ra::RaOverlayScreen::GameDetail ||
			ra_overlay->Screen() == Xm8Ra::RaOverlayScreen::Achievements ||
			ra_overlay->Screen() ==
				Xm8Ra::RaOverlayScreen::AchievementDetail ||
			ra_overlay->Screen() == Xm8Ra::RaOverlayScreen::Leaderboards) {
			return HandleRaOverlayAction(
				ra_overlay->OnListScroll(-e->wheel.y));
		}
		return true;
	}
	if (e->type != SDL_MOUSEBUTTONDOWN && e->type != SDL_MOUSEBUTTONUP) {
		return true;
	}
	if (e->button.button != SDL_BUTTON_LEFT) {
		if ((e->button.button == SDL_BUTTON_RIGHT ||
			e->button.button == SDL_BUTTON_X1) &&
			e->type == SDL_MOUSEBUTTONUP) {
			return HandleRaOverlayAction(
				ra_overlay->OnControlKey(Xm8Ra::RaOverlayKey::Escape));
		}
		return true;
	}

	int x = e->button.x;
	int y = e->button.y;
	if (!video->ConvertPoint(&x, &y)) {
		if (e->type == SDL_MOUSEBUTTONUP) {
			ra_overlay_mouse_target_valid = false;
			ra_overlay_mouse_detail_target = 0;
			ra_overlay_mouse_list_target_valid = false;
		}
		return true;
	}

	if (ra_overlay->Screen() == Xm8Ra::RaOverlayScreen::Library ||
		ra_overlay->Screen() == Xm8Ra::RaOverlayScreen::Achievements ||
		ra_overlay->Screen() == Xm8Ra::RaOverlayScreen::Leaderboards) {
		size_t target = 0;
		const bool target_valid = ra_overlay->ListTargetAt(x, y, &target);
		if (e->type == SDL_MOUSEBUTTONDOWN) {
			ra_overlay_mouse_list_target_valid = target_valid;
			ra_overlay_mouse_list_target = target;
			return HandleRaOverlayAction(
				ra_overlay->OnListPointer(x, y, false));
		}
		const bool activate = target_valid &&
			ra_overlay_mouse_list_target_valid &&
			ra_overlay_mouse_list_target == target;
		ra_overlay_mouse_list_target_valid = false;
		return HandleRaOverlayAction(
			ra_overlay->OnListPointer(x, y, activate));
	}
	if (ra_overlay->Screen() == Xm8Ra::RaOverlayScreen::GameDetail) {
		if (e->type == SDL_MOUSEBUTTONDOWN) {
			ra_overlay_mouse_detail_target =
				PointInRect(x, y, RaGameDetailStartButtonRect()) ? 2 : 0;
			return true;
		}
		const int pressed_target = ra_overlay_mouse_detail_target;
		ra_overlay_mouse_detail_target = 0;
		if (pressed_target == 2 &&
			PointInRect(x, y, RaGameDetailStartButtonRect()) &&
			(ra_overlay->CanLaunchSelectedLibraryGame() ||
				ra_overlay->CanResolveSelectedLibraryConflict())) {
			return HandleRaOverlayAction(
				ra_overlay->CanLaunchSelectedLibraryGame() ?
				Xm8Ra::RaOverlayAction::OpenLibraryGame :
				Xm8Ra::RaOverlayAction::ResolveLibraryConflict);
		}
		return true;
	}
	if (ra_overlay->Screen() ==
		Xm8Ra::RaOverlayScreen::AchievementDetail) {
		ra_overlay_mouse_detail_target = 0;
		return true;
	}

	if (ra_overlay->Screen() == Xm8Ra::RaOverlayScreen::Login) {
		Xm8Ra::RaOverlayLoginTarget target;
		if (!ra_overlay->LoginTargetAt(x, y, &target)) {
			if (e->type == SDL_MOUSEBUTTONUP) {
				ra_overlay_mouse_target_valid = false;
			}
			return true;
		}
		if (e->type == SDL_MOUSEBUTTONDOWN) {
			ra_overlay_mouse_target = target;
			ra_overlay_mouse_target_valid = true;
			return HandleRaOverlayAction(ra_overlay->OnLoginTarget(target,
				false));
		}

		const bool activate = ra_overlay_mouse_target_valid &&
			ra_overlay_mouse_target == target;
		ra_overlay_mouse_target_valid = false;
		return HandleRaOverlayAction(ra_overlay->OnLoginTarget(target,
			activate));
	}
	return true;
}

//
// HandleRaOverlayFinger()
// handle RA overlay touch input
//
bool App::HandleRaOverlayFinger(SDL_Event *e)
{
	if (ra_overlay == NULL) {
		return false;
	}
	if (!ra_overlay->IsBlocking()) {
		return HandleRaStatusFinger(e);
	}
	if (e->type != SDL_FINGERDOWN && e->type != SDL_FINGERUP &&
		e->type != SDL_FINGERMOTION) {
		return true;
	}

	int x = 0;
	int y = 0;
	if (!video->ConvertFinger(e->tfinger.x, e->tfinger.y, &x, &y)) {
		if (e->type == SDL_FINGERUP) {
			ra_overlay_finger_target_valid = false;
			ra_overlay_finger_detail_target = 0;
			ra_overlay_finger_list_target_valid = false;
			ra_overlay_finger_scroll_valid = false;
			ra_overlay_finger_scrolled = false;
		}
		return true;
	}

	if (ra_overlay->Screen() == Xm8Ra::RaOverlayScreen::Library ||
		ra_overlay->Screen() == Xm8Ra::RaOverlayScreen::Achievements ||
		ra_overlay->Screen() == Xm8Ra::RaOverlayScreen::GameDetail ||
		ra_overlay->Screen() ==
			Xm8Ra::RaOverlayScreen::AchievementDetail ||
		ra_overlay->Screen() == Xm8Ra::RaOverlayScreen::Leaderboards) {
		if (e->type == SDL_FINGERDOWN) {
			ra_overlay_finger_scroll_valid = true;
			ra_overlay_finger_scrolled = false;
			ra_overlay_finger_scroll_y = y;
			ra_overlay_finger_detail_target =
				(ra_overlay->Screen() == Xm8Ra::RaOverlayScreen::GameDetail &&
				 PointInRect(x, y, RaGameDetailStartButtonRect())) ? 2 : 0;
			size_t list_target = 0;
			ra_overlay_finger_list_target_valid =
				ra_overlay->ListTargetAt(x, y, &list_target);
			ra_overlay_finger_list_target = list_target;
			return HandleRaOverlayAction(ra_overlay->OnListPointer(x, y,
				false));
		}
		if (e->type == SDL_FINGERMOTION) {
			if (!ra_overlay_finger_scroll_valid) {
				ra_overlay_finger_scroll_valid = true;
				ra_overlay_finger_scroll_y = y;
			}
			const int delta_y = ra_overlay_finger_scroll_y - y;
			const int rows = delta_y / MENUITEM_HEIGHT;
			if (rows != 0) {
				ra_overlay_finger_scroll_y += rows * MENUITEM_HEIGHT;
				ra_overlay_finger_scrolled = true;
				return HandleRaOverlayAction(
					ra_overlay->OnListScroll(rows));
			}
			return true;
		}
		const bool activate = !ra_overlay_finger_scrolled;
		const int pressed_target = ra_overlay_finger_detail_target;
		ra_overlay_finger_detail_target = 0;
		ra_overlay_finger_scroll_valid = false;
		ra_overlay_finger_scrolled = false;
		if (ra_overlay->Screen() == Xm8Ra::RaOverlayScreen::GameDetail) {
			if (activate && pressed_target == 2 && PointInRect(x, y,
				RaGameDetailStartButtonRect()) &&
				(ra_overlay->CanLaunchSelectedLibraryGame() ||
					ra_overlay->CanResolveSelectedLibraryConflict())) {
				return HandleRaOverlayAction(
					ra_overlay->CanLaunchSelectedLibraryGame() ?
					Xm8Ra::RaOverlayAction::OpenLibraryGame :
					Xm8Ra::RaOverlayAction::ResolveLibraryConflict);
			}
			return true;
		}
		if (ra_overlay->Screen() ==
			Xm8Ra::RaOverlayScreen::AchievementDetail) {
			ra_overlay_finger_list_target_valid = false;
			return true;
		}
		size_t list_target = 0;
		const bool same_list_target = activate &&
			ra_overlay_finger_list_target_valid &&
			ra_overlay->ListTargetAt(x, y, &list_target) &&
			ra_overlay_finger_list_target == list_target;
		ra_overlay_finger_list_target_valid = false;
		return HandleRaOverlayAction(ra_overlay->OnListPointer(x, y,
			same_list_target));
	}

	if (ra_overlay->Screen() == Xm8Ra::RaOverlayScreen::Login) {
		Xm8Ra::RaOverlayLoginTarget target;
		if (!ra_overlay->LoginTargetAt(x, y, &target)) {
			if (e->type == SDL_FINGERUP) {
				ra_overlay_finger_target_valid = false;
			}
			return true;
		}
		if (e->type == SDL_FINGERDOWN) {
			ra_overlay_finger_target = target;
			ra_overlay_finger_target_valid = true;
			return HandleRaOverlayAction(ra_overlay->OnLoginTarget(target,
				false));
		}

		const bool activate = ra_overlay_finger_target_valid &&
			ra_overlay_finger_target == target;
		ra_overlay_finger_target_valid = false;
		return HandleRaOverlayAction(ra_overlay->OnLoginTarget(target,
			activate));
	}
	return true;
}

//
// HandleRaOverlayJoystick()
// handle RA overlay joystick input
//
bool App::HandleRaOverlayJoystick()
{
	if (ra_overlay == NULL || !ra_overlay->IsBlocking()) {
		return false;
	}

	uint32 status[2];
	input->GetJoystick((uint32*)status);
	const Uint32 mix = (Uint32)(status[0] | (status[1] << 8));
	const Uint32 pressed = mix & ~ra_overlay_joystick_prev;
	ra_overlay_joystick_prev = mix;

	Xm8Ra::RaOverlayAction action = Xm8Ra::RaOverlayAction::None;
	if ((pressed & 0x0001) != 0) {
		action = ra_overlay->OnControlKey(Xm8Ra::RaOverlayKey::Up);
	}
	else if ((pressed & 0x0002) != 0) {
		action = ra_overlay->OnControlKey(Xm8Ra::RaOverlayKey::Down);
	}
	else if ((pressed & 0x0004) != 0) {
		action = ra_overlay->OnControlKey(Xm8Ra::RaOverlayKey::Left);
	}
	else if ((pressed & 0x0008) != 0) {
		action = ra_overlay->OnControlKey(Xm8Ra::RaOverlayKey::Right);
	}
	else if ((pressed & 0x0010) != 0) {
		action = ra_overlay->OnControlKey(Xm8Ra::RaOverlayKey::Enter);
	}
	else if ((pressed & 0x2000) != 0) {
		action = ra_overlay->OnControlKey(Xm8Ra::RaOverlayKey::PageUp);
	}
	else if ((pressed & 0x4000) != 0) {
		action = ra_overlay->OnControlKey(Xm8Ra::RaOverlayKey::PageDown);
	}
	else if ((pressed & 0x0020) != 0 || (pressed & 0x0100) != 0) {
		action = ra_overlay->OnControlKey(Xm8Ra::RaOverlayKey::Escape);
	}

	return HandleRaOverlayAction(action);
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
		UpdateRaOverlayTextInput();
		return false;
	}

	const bool started = ra_service->BeginLoginWithPassword(username,
		password, &error);
	std::fill(password.begin(), password.end(), '\0');
	if (!started) {
		ra_overlay->SetLoginStatus(error.empty() ?
			"Login failed to start" : error);
		AddRaNotice("RA: login failed");
		UpdateRaOverlayTextInput();
		return false;
	}

	ra_saved_login_started = false;
	ra_menu_status.Set(Xm8Ra::RaMenuStatusState::LoginPending);
	ra_manual_login_started = true;
	ra_library_sync_started_for_login = false;
	SDL_StopTextInput();
	ClearRaOverlayPointerState();
	return true;
}

namespace {

std::string ToSjisMenuText(Converter *converter, const std::string& text);
size_t SjisCharBytes(const char *source, size_t offset);
int SjisCharWidth(const char *source, size_t offset);
int SjisTextWidth(const char *source);
size_t SjisCharCount(const char *source);
size_t SjisByteOffsetForChar(const char *source, size_t char_index);
void CopyClippedMenuText(char *target, size_t target_size,
	const char *source, int width);
void CopyAutoScrollMenuText(char *target, size_t target_size,
	const char *source, int width, Uint32 elapsed_ms);
void WrapSjisMenuText(std::vector<std::string> *lines, const char *source,
	int width);
const char *RaHealthLabel(int health_state);
std::string FormatRaUnixTime(int64_t unix_time);
SDL_Rect RaGameDetailStartButtonRect();
bool PointInRect(int x, int y, const SDL_Rect& rect);

} // namespace

//
// DrawRaOverlay()
// draw RA notice overlay
//
void App::DrawRaOverlay()
{
	if (!ra_mode_enabled || ra_overlay == NULL) {
		if (video != NULL) {
			video->SetRaStatusActive(false);
		}
		return;
	}
	ProcessRaImages();
	auto clear_menu_detail = [this]() {
		SDL_Rect detail_rect = {
			(SCREEN_WIDTH / 2) - (MENUITEM_WIDTH / 2),
			(SCREEN_HEIGHT / 2) +
				((MENUITEM_HEIGHT * MENUITEM_LINES) / 2),
			MENUITEM_WIDTH,
			MENUITEM_HEIGHT
		};
		font->DrawFillRect(video->GetMenuFrame(), &detail_rect,
			MENUITEM_BACK | 0x00000000);
		ra_menu_detail_active = false;
	};
	if (ra_overlay->Screen() != Xm8Ra::RaOverlayScreen::None) {
		clear_menu_detail();
	}
	if (ra_overlay->Screen() == Xm8Ra::RaOverlayScreen::Library ||
		ra_overlay->Screen() == Xm8Ra::RaOverlayScreen::Achievements ||
		ra_overlay->Screen() == Xm8Ra::RaOverlayScreen::Leaderboards) {
		const bool library_screen =
			ra_overlay->Screen() == Xm8Ra::RaOverlayScreen::Library;
		const bool achievements_screen =
			ra_overlay->Screen() == Xm8Ra::RaOverlayScreen::Achievements;
		const Xm8Ra::RaOverlayLibraryListSnapshot library =
			library_screen ? ra_overlay->LibraryListSnapshot() :
				Xm8Ra::RaOverlayLibraryListSnapshot();
		const Xm8Ra::RaOverlayAchievementListSnapshot achievements =
			achievements_screen ? ra_overlay->AchievementListSnapshot() :
				Xm8Ra::RaOverlayAchievementListSnapshot();
		const Xm8Ra::RaOverlayLeaderboardListSnapshot leaderboards =
			(library_screen || achievements_screen) ?
				Xm8Ra::RaOverlayLeaderboardListSnapshot() :
				ra_overlay->LeaderboardListSnapshot();
		const size_t selected_index = library_screen ?
			library.selected_index : (achievements_screen ?
				achievements.selected_index : leaderboards.selected_index);
		const size_t first_visible_index = library_screen ?
			library.first_visible_index : (achievements_screen ?
				achievements.first_visible_index :
				leaderboards.first_visible_index);
		const size_t item_count = library_screen ?
			library.games.size() : (achievements_screen ?
				achievements.achievements.size() :
				leaderboards.leaderboards.size());
		const std::string status_message = library_screen ?
			library.status_message : (achievements_screen ?
				achievements.status_message : leaderboards.status_message);
		const uint32_t selection_revision = library_screen ?
			library.selection_revision : (achievements_screen ?
				achievements.selection_revision : 0);
		if ((library_screen || achievements_screen) &&
			ra_overlay_auto_scroll_revision != selection_revision) {
			ra_overlay_auto_scroll_revision = selection_revision;
			ra_overlay_auto_scroll_started = SDL_GetTicks();
		}

		video->SetMenuMode(true);
		Uint32 *buf = video->GetMenuFrame();
		const Uint32 alpha = (Uint32)setting->GetMenuAlpha() << 24;
		Uint32 fore = MENUITEM_FORE | alpha;
		Uint32 back = MENUITEM_BACK | alpha;
		SDL_Rect rect = {
			(SCREEN_WIDTH / 2) - (MENUITEM_WIDTH / 2),
			(SCREEN_HEIGHT / 2) - ((MENUITEM_HEIGHT * MENUITEM_LINES) / 2),
			MENUITEM_WIDTH,
			MENUITEM_HEIGHT * MENUITEM_LINES
		};
		font->DrawFillRect(buf, &rect, MENUITEM_BACK | 0x00000000);
		SDL_Rect title_rect = {rect.x, rect.y, rect.w, MENUITEM_HEIGHT};
		font->DrawFillRect(buf, &title_rect, fore);
		title_rect.x++;
		title_rect.y++;
		title_rect.w -= 2;
		title_rect.h -= 2;
		font->DrawFillRect(buf, &title_rect, MENUITEM_TITLE | alpha);
		font->DrawSjisCenterOr(buf, &title_rect,
			library_screen ? "<< Library >>" :
				(achievements_screen ? "<< Achievements >>" :
					"<< Leaderboards >>"),
			fore);

		const size_t visible_rows = MENUITEM_LINES - 1;
		const bool show_status = item_count == 0 || !status_message.empty();
		const size_t remaining_rows = item_count - first_visible_index;
		const size_t rows = show_status ? 1 :
			(remaining_rows < visible_rows ? remaining_rows : visible_rows);
		for (size_t i = 0; i < rows; ++i) {
			const size_t item_index = first_visible_index + i;
			SDL_Rect row = {rect.x,
				rect.y + MENUITEM_HEIGHT * (static_cast<int>(i) + 1),
				rect.w, MENUITEM_HEIGHT};
			font->DrawFillRect(buf, &row, fore);

			bool reverse = false;
			if (!show_status && item_index == selected_index) {
				const Uint32 diff = SDL_GetTicks();
				if ((diff & 0x0200) == 0) {
					reverse = true;
				}
			}
			row.x++;
			row.y++;
			row.w -= 2;
			row.h -= 2;
			Uint32 row_fore = fore;
			Uint32 row_back = back;
			if (reverse) {
				row_fore = MENUITEM_BACK | alpha;
				row_back = MENUITEM_FORE | alpha;
			}
			font->DrawFillRect(buf, &row, row_back);

			char line[256];
			if (show_status) {
				std::snprintf(line, sizeof(line), "%s",
					status_message.empty() ? "No items" :
						status_message.c_str());
			}
			else if (library_screen) {
				const Xm8Ra::RaOverlayLibraryItem& item =
					library.games[item_index];
				std::snprintf(line, sizeof(line), "%s%s",
					item.identification_state ==
						Xm8Ra::kRaIdentificationConflict ? "[!] " : "",
					item.title.c_str());
			}
			else if (achievements_screen) {
				const Xm8Ra::RaOverlayAchievementItem& item =
					achievements.achievements[item_index];
				const char *mark = item.unlocked != 0 ? "[*]" : "[ ]";
				std::snprintf(line, sizeof(line), "%s %s  %u pts",
					mark, item.title.c_str(), item.points);
			}
			else {
				const Xm8Ra::RaOverlayLeaderboardItem& item =
					leaderboards.leaderboards[item_index];
				const char *direction =
					item.lower_is_better ? "low" : "high";
				std::snprintf(line, sizeof(line), "%s  %s",
					item.title.c_str(), direction);
			}
			SDL_Rect text_rect = {row.x + 24, row.y, row.w - 48, row.h};
			const std::string sjis_line = ToSjisMenuText(converter, line);
			char display_line[256];
			if ((library_screen || achievements_screen) && !show_status &&
				item_index == selected_index) {
				CopyAutoScrollMenuText(display_line, sizeof(display_line),
					sjis_line.c_str(), text_rect.w,
					SDL_GetTicks() - ra_overlay_auto_scroll_started);
			}
			else {
				CopyClippedMenuText(display_line, sizeof(display_line),
					sjis_line.c_str(), text_rect.w);
			}
			font->DrawSjisBoldOr(buf, &text_rect, display_line, row_fore);

			unsigned char arrow[3] = {0, 0, 0};
			if (!show_status && first_visible_index > 0 && i == 0) {
				arrow[0] = 0x81;
				arrow[1] = 0xaa;
			}
			else if (!show_status &&
				item_count > first_visible_index + visible_rows &&
				i == visible_rows - 1) {
				arrow[0] = 0x81;
				arrow[1] = 0xab;
			}
			if (arrow[0] != 0) {
				SDL_Rect arrow_rect = row;
				arrow_rect.x = row.x + row.w - 16;
				font->DrawSjisBoldOr(buf, &arrow_rect,
					(const char*)arrow, row_fore);
			}
		}

		if (!show_status && item_count > 0) {
			const std::string game_title = achievements_screen ?
				achievements.game_title : leaderboards.game_title;
			char detail[192];
			if (library_screen &&
				selected_index < library.games.size()) {
				const Xm8Ra::RaOverlayLibraryItem& selected =
					library.games[selected_index];
				const char *health = RaHealthLabel(selected.health_state);
				if (selected.ra_game_id > 0) {
					std::snprintf(detail, sizeof(detail),
						"%u/%u  Game %lld  RA %lld  %s",
						static_cast<unsigned int>(selected_index + 1),
						static_cast<unsigned int>(item_count),
						static_cast<long long>(selected.game_id),
						static_cast<long long>(selected.ra_game_id),
						health);
				}
				else {
					std::snprintf(detail, sizeof(detail),
						"%u/%u  Game %lld  RA -  %s",
						static_cast<unsigned int>(selected_index + 1),
						static_cast<unsigned int>(item_count),
						static_cast<long long>(selected.game_id),
						health);
				}
			}
			else if (achievements_screen &&
				selected_index < achievements.achievements.size()) {
				const Xm8Ra::RaOverlayAchievementItem& selected =
					achievements.achievements[selected_index];
				if (!selected.measured_progress.empty()) {
					std::snprintf(detail, sizeof(detail), "%u/%u  %s  %s",
						static_cast<unsigned int>(selected_index + 1),
						static_cast<unsigned int>(item_count),
						selected.description.c_str(),
						selected.measured_progress.c_str());
				}
				else {
					std::snprintf(detail, sizeof(detail), "%u/%u  %s",
						static_cast<unsigned int>(selected_index + 1),
						static_cast<unsigned int>(item_count),
						selected.description.c_str());
				}
			}
			else {
				const Xm8Ra::RaOverlayLeaderboardItem& selected =
					leaderboards.leaderboards[selected_index];
				if (selected.has_scoreboard) {
					const std::string scoreboard_detail =
						RaLeaderboardScoreboardDetail(selected);
					std::snprintf(detail, sizeof(detail), "%u/%u  %s",
						static_cast<unsigned int>(selected_index + 1),
						static_cast<unsigned int>(item_count),
						scoreboard_detail.c_str());
				}
				else if (selected.entries_pending ||
					selected.entries_failed || selected.has_entries) {
					const std::string entries_detail =
						RaLeaderboardEntriesDetail(selected);
					std::snprintf(detail, sizeof(detail), "%u/%u  %s",
						static_cast<unsigned int>(selected_index + 1),
						static_cast<unsigned int>(item_count),
						entries_detail.c_str());
				}
				else if (!selected.bucket_label.empty()) {
					std::snprintf(detail, sizeof(detail),
						"%u/%u  %s  %s",
						static_cast<unsigned int>(selected_index + 1),
						static_cast<unsigned int>(item_count),
						selected.bucket_label.c_str(),
						selected.description.c_str());
				}
				else {
					std::snprintf(detail, sizeof(detail), "%u/%u  %s",
						static_cast<unsigned int>(selected_index + 1),
						static_cast<unsigned int>(item_count),
						selected.description.c_str());
				}
			}
			SDL_Rect detail_rect = {rect.x, rect.y + rect.h + 6,
				rect.w, 22};
			font->DrawFillRect(buf, &detail_rect, MENUITEM_BACK | alpha);
			detail_rect.x += 8;
			detail_rect.w -= 16;
			const std::string sjis_detail =
				ToSjisMenuText(converter, detail);
			char display_detail[256];
			if (library_screen || achievements_screen ||
				ra_overlay->Screen() ==
					Xm8Ra::RaOverlayScreen::Leaderboards) {
				CopyAutoScrollMenuText(display_detail,
					sizeof(display_detail), sjis_detail.c_str(),
					detail_rect.w,
					SDL_GetTicks() - ra_overlay_auto_scroll_started);
			}
			else {
				CopyClippedMenuText(display_detail,
					sizeof(display_detail), sjis_detail.c_str(),
					detail_rect.w);
			}
			font->DrawSjisLeftOr(buf, &detail_rect, display_detail, fore);
			ra_menu_detail_active = true;
		}
		else {
			const std::string game_title = achievements_screen ?
				achievements.game_title : leaderboards.game_title;
			if (!game_title.empty()) {
				SDL_Rect detail_rect = {rect.x, rect.y + rect.h + 6,
					rect.w, 22};
				font->DrawFillRect(buf, &detail_rect,
					MENUITEM_BACK | alpha);
				detail_rect.x += 8;
				detail_rect.w -= 16;
				const std::string sjis_title =
					ToSjisMenuText(converter, game_title);
				char clipped_title[192];
				CopyClippedMenuText(clipped_title, sizeof(clipped_title),
					sjis_title.c_str(), detail_rect.w);
				font->DrawSjisLeftOr(buf, &detail_rect, clipped_title,
					fore);
				ra_menu_detail_active = true;
			}
		}
		video->DrawCtrl();
	}
	else if (ra_overlay->Screen() == Xm8Ra::RaOverlayScreen::GameDetail) {
		const Xm8Ra::RaOverlayLibraryListSnapshot library =
			ra_overlay->LibraryListSnapshot();
		video->SetMenuMode(true);
		Uint32 *buf = video->GetMenuFrame();
		const Uint32 alpha = (Uint32)setting->GetMenuAlpha() << 24;
		const Uint32 fore = MENUITEM_FORE | alpha;
		const Uint32 back = MENUITEM_BACK | alpha;
		SDL_Rect rect = {
			(SCREEN_WIDTH / 2) - (MENUITEM_WIDTH / 2),
			(SCREEN_HEIGHT / 2) - ((MENUITEM_HEIGHT * MENUITEM_LINES) / 2),
			MENUITEM_WIDTH,
			MENUITEM_HEIGHT * MENUITEM_LINES
		};
		font->DrawFillRect(buf, &rect, MENUITEM_BACK | 0x00000000);

		SDL_Rect title_rect = {rect.x, rect.y, rect.w, MENUITEM_HEIGHT};
		font->DrawFillRect(buf, &title_rect, fore);
		title_rect.x++;
		title_rect.y++;
		title_rect.w -= 2;
		title_rect.h -= 2;
		font->DrawFillRect(buf, &title_rect, MENUITEM_TITLE | alpha);
		font->DrawSjisCenterOr(buf, &title_rect, "<< Game Detail >>",
			fore);

		SDL_Rect body = {rect.x, rect.y + MENUITEM_HEIGHT, rect.w,
			rect.h - MENUITEM_HEIGHT};
		font->DrawFillRect(buf, &body, fore);
		body.x++;
		body.y++;
		body.w -= 2;
		body.h -= 2;
		font->DrawFillRect(buf, &body, back);

		if (library.selected_index < library.games.size()) {
			const Xm8Ra::RaOverlayLibraryItem& game =
				library.games[library.selected_index];
			SDL_Rect badge_outer = {body.x + 16, body.y + 14, 72, 72};
			font->DrawFillRect(buf, &badge_outer, fore);
			SDL_Rect badge_inner = {badge_outer.x + 2,
				badge_outer.y + 2, badge_outer.w - 4,
				badge_outer.h - 4};
			font->DrawFillRect(buf, &badge_inner, MENUITEM_BACK | alpha);
			DrawRaBadgeImage(buf, &badge_inner, game.badge_url,
				Xm8Ra::RaImageKind::GameBadge);

			char progress[80];
			if (game.has_progress && game.core_total > 0) {
				std::snprintf(progress, sizeof(progress),
					"Progress %d/%d  Points %d/%d",
					game.core_unlocked, game.core_total,
					game.points_unlocked, game.points_total);
			}
			else {
				std::snprintf(progress, sizeof(progress),
					"Progress unknown");
			}

			char ids[128];
			if (game.identification_state ==
				Xm8Ra::kRaIdentificationConflict) {
				std::snprintf(ids, sizeof(ids),
					"Game ID %lld  RA ID conflict",
					static_cast<long long>(game.game_id));
			}
			else {
				std::snprintf(ids, sizeof(ids), "Game ID %lld  RA ID %lld",
					static_cast<long long>(game.game_id),
					static_cast<long long>(game.ra_game_id));
			}

			const std::string last_played =
				"Last played " + FormatRaUnixTime(game.last_played_at);
			const std::string media =
				std::string("Media ") +
				std::to_string(game.media_count) + "  " +
					RaHealthLabel(game.health_state);
			const bool media_conflict = game.identification_state ==
				Xm8Ra::kRaIdentificationConflict;
			const bool resolvable_conflict = media_conflict &&
				(game.conflict_kind == Xm8Ra::RaOverlayConflictKind::Merge ||
				 game.conflict_kind == Xm8Ra::RaOverlayConflictKind::Split);

			std::vector<std::string> lines;
			WrapSjisMenuText(&lines,
				ToSjisMenuText(converter, game.title).c_str(),
				body.w - 128);
			lines.push_back(ToSjisMenuText(converter, progress));
			lines.push_back(ToSjisMenuText(converter, ids));
			lines.push_back(ToSjisMenuText(converter, last_played));
			lines.push_back(ToSjisMenuText(converter, media));
			if (media_conflict) {
				lines.push_back(ToSjisMenuText(converter,
					resolvable_conflict ?
					"Media conflict: press Enter to resolve" :
					"Media conflict: manual configuration required"));
			}

			const int line_height = 20;
			const int text_x = body.x + 104;
			for (size_t row = 0; row < lines.size(); ++row) {
				SDL_Rect line_rect = {text_x,
					body.y + 16 + static_cast<int>(row) * line_height,
					body.w - 120, line_height};
				font->DrawSjisLeftOr(buf, &line_rect,
					lines[row].c_str(), fore);
			}

			SDL_Rect start_button = RaGameDetailStartButtonRect();
			font->DrawFillRect(buf, &start_button, fore);
			SDL_Rect start_inner = {start_button.x + 2,
				start_button.y + 2, start_button.w - 4,
				start_button.h - 4};
			font->DrawFillRect(buf, &start_inner,
				(media_conflict && !resolvable_conflict) ? back :
				(MENUITEM_TITLE | alpha));
			const char *button_label = "START";
			if (game.conflict_kind == Xm8Ra::RaOverlayConflictKind::Merge)
				button_label = "MERGE";
			else if (game.conflict_kind == Xm8Ra::RaOverlayConflictKind::Split)
				button_label = "SPLIT";
			else if (media_conflict) button_label = "MANUAL";
			font->DrawSjisCenterOr(buf, &start_inner,
				button_label, fore);
		}
		video->DrawCtrl();
	}
	else if (ra_overlay->Screen() ==
		Xm8Ra::RaOverlayScreen::AchievementDetail) {
		const Xm8Ra::RaOverlayAchievementDetailSnapshot detail =
			ra_overlay->AchievementDetailSnapshot();
		video->SetMenuMode(true);
		Uint32 *buf = video->GetMenuFrame();
		const Uint32 alpha = (Uint32)setting->GetMenuAlpha() << 24;
		const Uint32 fore = MENUITEM_FORE | alpha;
		const Uint32 back = MENUITEM_BACK | alpha;
		SDL_Rect rect = {
			(SCREEN_WIDTH / 2) - (MENUITEM_WIDTH / 2),
			(SCREEN_HEIGHT / 2) - ((MENUITEM_HEIGHT * MENUITEM_LINES) / 2),
			MENUITEM_WIDTH,
			MENUITEM_HEIGHT * MENUITEM_LINES
		};
		font->DrawFillRect(buf, &rect, MENUITEM_BACK | 0x00000000);

		SDL_Rect title_rect = {rect.x, rect.y, rect.w, MENUITEM_HEIGHT};
		font->DrawFillRect(buf, &title_rect, fore);
		title_rect.x++;
		title_rect.y++;
		title_rect.w -= 2;
		title_rect.h -= 2;
		font->DrawFillRect(buf, &title_rect, MENUITEM_TITLE | alpha);
		font->DrawSjisCenterOr(buf, &title_rect,
			"<< Achievement Detail >>", fore);

		SDL_Rect body = {rect.x, rect.y + MENUITEM_HEIGHT, rect.w,
			rect.h - MENUITEM_HEIGHT};
		font->DrawFillRect(buf, &body, fore);
		body.x++;
		body.y++;
		body.w -= 2;
		body.h -= 2;
		font->DrawFillRect(buf, &body, back);

		SDL_Rect badge_outer = {body.x + 16, body.y + 14, 72, 72};
		font->DrawFillRect(buf, &badge_outer, fore);
		SDL_Rect badge_inner = {badge_outer.x + 2, badge_outer.y + 2,
			badge_outer.w - 4, badge_outer.h - 4};
		font->DrawFillRect(buf, &badge_inner, MENUITEM_BACK | alpha);
		std::string badge_url = detail.achievement.unlocked != 0 ?
			detail.achievement.badge_url :
			detail.achievement.badge_locked_url;
		if (badge_url.empty()) {
			badge_url = detail.achievement.unlocked != 0 ?
				detail.achievement.badge_locked_url :
				detail.achievement.badge_url;
		}
		DrawRaBadgeImage(buf, &badge_inner, badge_url,
			detail.achievement.unlocked != 0 ?
				Xm8Ra::RaImageKind::AchievementBadge :
				Xm8Ra::RaImageKind::AchievementBadgeLocked);

		char summary[128];
		const char *state = detail.achievement.unlocked != 0 ?
			"Unlocked" : "Locked";
		std::snprintf(summary, sizeof(summary), "%s  %u pts  %u/%u",
			state, detail.achievement.points,
			static_cast<unsigned int>(detail.selected_index + 1),
			static_cast<unsigned int>(detail.item_count));
		std::vector<std::string> lines;
		lines.push_back(ToSjisMenuText(converter, "Title"));
		WrapSjisMenuText(&lines,
			ToSjisMenuText(converter,
				detail.achievement.title).c_str(),
			body.w - 32);
		if (!detail.achievement.bucket_label.empty()) {
			lines.push_back(ToSjisMenuText(converter,
				detail.achievement.bucket_label));
		}
		if (!detail.achievement.measured_progress.empty()) {
			lines.push_back(ToSjisMenuText(converter,
				detail.achievement.measured_progress));
		}
		std::vector<std::string> description_lines;
		WrapSjisMenuText(&description_lines,
			ToSjisMenuText(converter,
				detail.achievement.description).c_str(),
			body.w - 32);
		if (!description_lines.empty()) {
			lines.push_back("");
			lines.push_back(ToSjisMenuText(converter, "Description"));
			lines.insert(lines.end(), description_lines.begin(),
				description_lines.end());
		}

		const int line_height = 20;
		const int text_x = body.x + 104;
		const int header_width = body.w - 120;
		SDL_Rect summary_rect = {text_x, body.y + 14,
			header_width, line_height};
		const std::string sjis_summary = ToSjisMenuText(converter, summary);
		font->DrawSjisLeftOr(buf, &summary_rect, sjis_summary.c_str(),
			fore);

		const int detail_top = body.y + 98;
		const int detail_lines = (body.y + body.h - detail_top - 8) /
			line_height;
		const int scrollable_count =
			static_cast<int>(lines.size());
		const int max_detail_scroll =
			scrollable_count > detail_lines ? scrollable_count - detail_lines :
				0;
		const int detail_scroll =
			detail.scroll_offset > max_detail_scroll ? max_detail_scroll :
				detail.scroll_offset;
		const int content_offset = detail_scroll;
		for (int row = 0; row < detail_lines; ++row) {
			const int source_index = content_offset + row;
			if (source_index < 0 ||
				source_index >= static_cast<int>(lines.size())) {
				break;
			}
			SDL_Rect line_rect = {body.x + 16,
				detail_top + row * line_height, body.w - 32,
				line_height};
			font->DrawSjisLeftOr(buf, &line_rect,
				lines[source_index].c_str(), fore);
		}

		if (detail_scroll > 0) {
			unsigned char arrow[3] = {0x81, 0xaa, 0};
			SDL_Rect arrow_rect = {body.x + body.w - 20, detail_top,
				16, line_height};
			font->DrawSjisBoldOr(buf, &arrow_rect, (const char*)arrow,
				fore);
		}
		if (detail_scroll < max_detail_scroll) {
			unsigned char arrow[3] = {0x81, 0xab, 0};
			SDL_Rect arrow_rect = {body.x + body.w - 20,
				detail_top + (detail_lines - 1) * line_height,
				16, line_height};
			font->DrawSjisBoldOr(buf, &arrow_rect, (const char*)arrow,
				fore);
		}
		video->DrawCtrl();
	}
	else if (ra_overlay->Screen() == Xm8Ra::RaOverlayScreen::Login) {
		const Xm8Ra::RaOverlayLoginSnapshot login =
			ra_overlay->LoginSnapshot();
		SDL_Rect panel = {104, 78, 432, 218};
		video->SetMenuMode(true);
		Uint32 *buf = video->GetMenuFrame();
		SDL_Rect clear = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
		font->DrawFillRect(buf, &clear, 0x00000000);
		const Uint32 panel_alpha = 0xe0000000;
		const Uint32 foreground_alpha = 0xff000000;
		font->DrawFillRect(buf, &panel,
			RGB_COLOR(16, 16, 16) | panel_alpha);
		font->DrawRect(buf, &panel,
			RGB_COLOR(255, 255, 255) | foreground_alpha,
			RGB_COLOR(16, 16, 16) | panel_alpha);

		SDL_Rect title = {panel.x, panel.y + 12, panel.w, 24};
		font->DrawSjisCenterOr(buf, &title, "RetroAchievements Login",
			RGB_COLOR(255, 255, 255) | foreground_alpha);

		SDL_Rect user_label = {panel.x + 24, panel.y + 56, 104, 22};
		SDL_Rect user_box = {panel.x + 128, panel.y + 52, 272, 28};
		SDL_Rect pass_label = {panel.x + 24, panel.y + 96, 104, 22};
		SDL_Rect pass_box = {panel.x + 128, panel.y + 92, 272, 28};
		font->DrawSjisLeftOr(buf, &user_label, "Username",
			RGB_COLOR(220, 220, 220) | foreground_alpha);
		font->DrawSjisLeftOr(buf, &pass_label, "Password",
			RGB_COLOR(220, 220, 220) | foreground_alpha);

		const bool user_focus =
			login.focus == Xm8Ra::RaOverlayLoginTarget::Username;
		const bool pass_focus =
			login.focus == Xm8Ra::RaOverlayLoginTarget::Password;
		font->DrawRect(buf, &user_box,
			(user_focus ? RGB_COLOR(255, 255, 128) :
				RGB_COLOR(128, 128, 128)) | foreground_alpha,
			RGB_COLOR(0, 0, 0) | panel_alpha);
		font->DrawRect(buf, &pass_box,
			(pass_focus ? RGB_COLOR(255, 255, 128) :
				RGB_COLOR(128, 128, 128)) | foreground_alpha,
			RGB_COLOR(0, 0, 0) | panel_alpha);

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
			RGB_COLOR(255, 255, 255) | foreground_alpha);
		font->DrawSjisLeftOr(buf, &pass_box, pass,
			RGB_COLOR(255, 255, 255) | foreground_alpha);
		if ((user_focus || pass_focus) &&
			((SDL_GetTicks() / 500) & 1) == 0) {
			const char *focused_text = user_focus ? user : pass;
			SDL_Rect cursor_box = user_focus ? user_box : pass_box;
			int cursor_x = cursor_box.x +
				static_cast<int>(std::strlen(focused_text)) * 8;
			const int cursor_limit = cursor_box.x + cursor_box.w - 2;
			if (cursor_x > cursor_limit) {
				cursor_x = cursor_limit;
			}
			SDL_Rect cursor = {cursor_x, cursor_box.y + 5, 2, 16};
			font->DrawFillRect(buf, &cursor,
				RGB_COLOR(255, 255, 255) | foreground_alpha);
		}

		SDL_Rect login_button = {256, 220, 112, 30};
		SDL_Rect cancel_button = {384, 220, 112, 30};
		const bool login_focus =
			login.focus == Xm8Ra::RaOverlayLoginTarget::Login;
		const bool cancel_focus =
			login.focus == Xm8Ra::RaOverlayLoginTarget::Cancel;
		font->DrawRect(buf, &login_button,
			(login_focus ? RGB_COLOR(255, 255, 128) :
				RGB_COLOR(128, 128, 128)) | foreground_alpha,
			login.can_submit ? RGB_COLOR(48, 72, 96) | panel_alpha :
				RGB_COLOR(32, 32, 32) | panel_alpha);
		font->DrawRect(buf, &cancel_button,
			(cancel_focus ? RGB_COLOR(255, 255, 128) :
				RGB_COLOR(128, 128, 128)) | foreground_alpha,
			RGB_COLOR(64, 64, 64) | panel_alpha);
		font->DrawSjisCenterOr(buf, &login_button, "Login",
			(login.can_submit ? RGB_COLOR(255, 255, 255) :
				RGB_COLOR(160, 160, 160)) | foreground_alpha);
		font->DrawSjisCenterOr(buf, &cancel_button, "Cancel",
			RGB_COLOR(255, 255, 255) | foreground_alpha);

		SDL_Rect hint = {panel.x + 24, panel.y + 178, panel.w - 48, 22};
		font->DrawSjisLeftOr(buf, &hint,
			"Tab/Arrows: Move  Enter: Select  Esc: Cancel",
			RGB_COLOR(200, 200, 200) | foreground_alpha);
		if (!login.status_message.empty()) {
			char status[72];
			std::snprintf(status, sizeof(status), "%s",
				login.status_message.c_str());
			SDL_Rect status_rect = {panel.x + 24, panel.y + 194,
				panel.w - 48, 22};
			font->DrawSjisLeftOr(buf, &status_rect, status,
				RGB_COLOR(255, 192, 96) | foreground_alpha);
		}
		video->DrawCtrl();
	}
	const bool presence_focused = app_menu && menu != NULL &&
		menu->IsRaRichPresenceFocused();
	const bool error_focused = app_menu && menu != NULL && ra_overlay != NULL &&
		menu->IsRaStatusFocused() &&
		!ra_overlay->LastSubmissionError().empty();
	if (presence_focused || error_focused) {
		if (!ra_menu_presence_scroll_active ||
			ra_menu_error_scroll_active != error_focused) {
			ra_menu_presence_scroll_active = true;
			ra_menu_error_scroll_active = error_focused;
			ra_menu_presence_scroll_started = SDL_GetTicks();
		}

		std::string detail;
		if (error_focused) {
			detail = ra_overlay->LastSubmissionError();
		}
		else {
			detail = ra_service != NULL ?
				ra_service->RichPresence() : std::string();
			if (detail.empty()) {
				detail = "No Rich Presence";
			}
		}
		const std::string detail_sjis = ToSjisMenuText(converter, detail);
		char display_presence[256];
		SDL_Rect presence_rect = {
			(SCREEN_WIDTH / 2) - (MENUITEM_WIDTH / 2),
			(SCREEN_HEIGHT / 2) +
				((MENUITEM_HEIGHT * MENUITEM_LINES) / 2) + 6,
			MENUITEM_WIDTH,
			24
		};
		CopyAutoScrollMenuText(display_presence, sizeof(display_presence),
			detail_sjis.c_str(), presence_rect.w - 16,
			SDL_GetTicks() - ra_menu_presence_scroll_started);
		Uint32 *buf = video->GetMenuFrame();
		const Uint32 alpha = (Uint32)setting->GetMenuAlpha() << 24;
		font->DrawFillRect(buf, &presence_rect, MENUITEM_BACK | alpha);
		presence_rect.x += 8;
		presence_rect.w -= 16;
		font->DrawSjisLeftOr(buf, &presence_rect, display_presence,
			MENUITEM_FORE | alpha);
		ra_menu_detail_active = true;
		video->DrawCtrl();
	}
	else {
		if (app_menu && ra_menu_detail_active) {
			clear_menu_detail();
			video->DrawCtrl();
		}
		ra_menu_presence_scroll_active = false;
		ra_menu_error_scroll_active = false;
	}

	const Uint32 notice_now = SDL_GetTicks();
	const bool blocking = ra_overlay->IsBlocking();
	ra_overlay->SetNoticesPaused(blocking, notice_now);
	const std::vector<Xm8Ra::RaVisibleNotice> notices =
		ra_overlay->VisibleNotices(notice_now);
	const bool notice_visible = !notices.empty();
	ra_overlay->SetStatusPagesPaused(blocking || notice_visible, notice_now);
	if (blocking) {
		video->SetRaStatusActive(false);
		return;
	}

	Xm8Ra::RaStatusPageSnapshot page;
	if (!notice_visible) {
		page = ra_overlay->VisibleStatusPage(notice_now);
	}
	const bool page_visible = page.type != Xm8Ra::RaStatusPageType::None;
	if (!notice_visible && !page_visible) {
		video->SetRaStatusActive(false);
		return;
	}

	video->SetRaStatusActive(true);
	Uint32 *buf = video->GetStatusFrame();
	const int status_height = video->GetStatusFrameHeight();
	const int content_top = video->GetStatusContentTop();
	const Uint32 alpha = setting->HasStatusLine() ? 0xff000000 :
		static_cast<Uint32>(setting->GetStatusAlpha()) << 24;
	SDL_Rect background = {0, 0, SCREEN_WIDTH, status_height};
	font->DrawFillRect(buf, &background, RGB_COLOR(0, 0, 0) | alpha);

	std::string text;
	std::string badge_url;
	if (notice_visible) {
		text = notices.front().text;
		badge_url = notices.front().badge_url;
	}
	else {
		std::ostringstream stream;
		stream << "RA " << (page.index + 1) << "/" << page.total << " ";
		switch (page.type) {
		case Xm8Ra::RaStatusPageType::Challenge:
			stream << "Challenge: " << page.title;
			break;
		case Xm8Ra::RaStatusPageType::Progress:
			stream << "Progress: " << page.title;
			if (!page.value.empty()) {
				stream << " " << page.value;
			}
			break;
		case Xm8Ra::RaStatusPageType::LeaderboardTracker:
			stream << "Leaderboard: " << page.value;
			break;
		default:
			break;
		}
		text = stream.str();
		badge_url = page.badge_url;
	}

	int text_x = 4;
	const bool reserve_badge = !badge_url.empty() ||
		(!notice_visible &&
			(page.type == Xm8Ra::RaStatusPageType::Challenge ||
			 page.type == Xm8Ra::RaStatusPageType::Progress));
	if (reserve_badge) {
		SDL_Rect badge = {2, content_top, 16, 16};
		font->DrawRect(buf, &badge, RGB_COLOR(191, 191, 191) | alpha,
			RGB_COLOR(0, 0, 0) | alpha);
		DrawRaBadgeImage(buf, &badge, badge_url,
			Xm8Ra::RaImageKind::AchievementBadge, false);
		text_x = 22;
	}
	const std::string display_text = ToSjisMenuText(converter, text);
	SDL_Rect text_rect = {text_x, content_top, SCREEN_WIDTH - text_x - 4, 16};
	font->DrawSjisLeftOr(buf, &text_rect, display_text.c_str(),
		RGB_COLOR(255, 255, 255) | alpha);
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
	ra_connectivity_monitor.reset();
	if (ra_overlay != NULL) {
		delete ra_overlay;
		ra_overlay = NULL;
	}
	if (ra_image_http_client != nullptr) {
		ra_image_http_client->CancelAll();
		ra_image_http_client.reset();
	}
	ra_badge_images.clear();
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
	StopRaSession();
	ra_pending_game_hash.clear();
	ra_pending_library_game_id = 0;
	ra_loaded_library_game_id = 0;
	ra_loaded_game_hash.clear();
	ClearRaMediaChangeState();
	ra_leaderboard_scoreboards.clear();
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
// IsRaOverlayBlocking()
// check blocking RA overlay
//
bool App::IsRaOverlayBlocking() const
{
#ifdef XM8_ENABLE_RETROACHIEVEMENTS
	if (ra_overlay == NULL || !ra_overlay->IsBlocking()) return false;
	// Browsing RA information in Hardcore is an input-capturing overlay, not
	// an emulator pause. Login remains blocking because it owns text input.
	return !IsRaHardcoreActive() ||
		ra_overlay->Screen() == Xm8Ra::RaOverlayScreen::Login;
#else
	return false;
#endif
}

#ifdef XM8_ENABLE_RETROACHIEVEMENTS
bool App::IsRaHardcoreMenuRunning() const
{
	return app_menu && IsRaHardcoreActive() && !app_background &&
		!app_powerdown;
}
#endif

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
#ifdef XM8_ENABLE_RETROACHIEVEMENTS
		if (!ra_mode_enabled) Load(0);
#else
		Load(0);
#endif

		// enter menu
		EnterMenu(MENU_MAIN);
	}
#else
	if (startup_disk_boot == false) {
		// load state 0 (auto)
#ifdef XM8_ENABLE_RETROACHIEVEMENTS
		if (!ra_mode_enabled) Load(0);
#else
		Load(0);
#endif

		// enter menu
		EnterMenu(MENU_MAIN);
	}
#endif // __ANDROID__

#ifdef XM8_ENABLE_RETROACHIEVEMENTS
	StartRaAfterBoot();
#endif

	// main loop
	while (app_quit == false) {
		const bool ra_overlay_blocking = IsRaOverlayBlocking();
#ifdef XM8_ENABLE_RETROACHIEVEMENTS
		const bool ra_hardcore_menu_running = IsRaHardcoreMenuRunning();
#else
		const bool ra_hardcore_menu_running = false;
#endif

		// stop virtual machine or menu
		if ((app_menu == true && !ra_hardcore_menu_running) ||
			(app_background == true) ||
			(app_powerdown == true) || ra_overlay_blocking) {
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
#ifdef XM8_ENABLE_RETROACHIEVEMENTS
				if (ra_mode_enabled && ra_service != NULL) {
					// Keep HTTP and rc_client idle processing alive while backgrounded.
					ret = SDL_WaitEventTimeout(&e, SLEEP_RA_BACKGROUND);
				}
				else
#endif
				{
					// Normal mode retains the existing indefinite wait.
					ret = SDL_WaitEvent(&e);
				}
			}
			else {
				if (app_menu == true || ra_overlay_blocking) {
					// menu or blocking overlay
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
			ProcessRaService(true);
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
			UnlockVM();
#ifdef XM8_ENABLE_RETROACHIEVEMENTS
			ProcessRaService(false);
#endif

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
		if (app_menu == true) {
			menu->ProcessMenu();
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
	if (startup_disk_boot == false
#ifdef XM8_ENABLE_RETROACHIEVEMENTS
		&& !ra_mode_enabled
#endif
	) {
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
	if (IsRaHardcoreMenuRunning()) {
		menu->Draw();
	}
#endif

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
	if ((app_menu == true) || (app_background == true) ||
		(app_powerdown == true) || (app_fullspeed == true) ||
		IsRaOverlayBlocking()) {
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
#ifdef XM8_ENABLE_RETROACHIEVEMENTS
		if (HandleRaOverlayMouse(e)) {
			break;
		}
#endif
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
#ifdef XM8_ENABLE_RETROACHIEVEMENTS
		if (HandleRaOverlayMouse(e)) {
			break;
		}
#endif
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
#ifdef XM8_ENABLE_RETROACHIEVEMENTS
		if (HandleRaOverlayMouse(e)) {
			break;
		}
#endif
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
#ifdef XM8_ENABLE_RETROACHIEVEMENTS
		if (HandleRaOverlayMouse(e)) {
			break;
		}
#endif
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
#ifdef XM8_ENABLE_RETROACHIEVEMENTS
		if (HandleRaOverlayJoystick()) {
			break;
		}
#endif
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
#ifdef XM8_ENABLE_RETROACHIEVEMENTS
		if (HandleRaOverlayJoystick()) {
			break;
		}
#endif
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
#ifdef XM8_ENABLE_RETROACHIEVEMENTS
		if (HandleRaOverlayJoystick()) {
			break;
		}
#endif
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
#ifdef XM8_ENABLE_RETROACHIEVEMENTS
		if (HandleRaOverlayFinger(e)) {
			break;
		}
#endif
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
#ifdef XM8_ENABLE_RETROACHIEVEMENTS
		if (HandleRaOverlayFinger(e)) {
			break;
		}
#endif
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
#ifdef XM8_ENABLE_RETROACHIEVEMENTS
		if (HandleRaOverlayFinger(e)) {
			break;
		}
#endif
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
#ifdef XM8_ENABLE_RETROACHIEVEMENTS
	if (!CheckRaOperation(Xm8Ra::RaRestrictedOperation::FullSpeed,
		"RA: Full Speed is unavailable in Hardcore")) {
		return;
	}
#endif
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
	const bool menu_stops_audio = app_menu
#ifdef XM8_ENABLE_RETROACHIEVEMENTS
		&& !IsRaHardcoreMenuRunning()
#endif
		;
	if ((app_background == false) && !menu_stops_audio &&
		(app_powerdown == false) && !IsRaOverlayBlocking()) {
		audio->Play();
	}
	if ((app_background == true) || menu_stops_audio ||
		(app_powerdown == true) || IsRaOverlayBlocking()) {
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
	ChangeSystemInternal(load, false);
}

void App::ChangeSystemInternal(bool load, bool preserve_ra_session)
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

#ifdef XM8_ENABLE_RETROACHIEVEMENTS
	if (!preserve_ra_session) {
		if (ra_service != NULL) {
			ra_service->UnloadGame();
		}
		StopRaSession();
		ra_pending_game_hash.clear();
		ra_pending_library_game_id = 0;
		ra_loaded_library_game_id = 0;
		ra_loaded_game_hash.clear();
		ClearRaMediaChangeState();
		ra_leaderboard_scoreboards.clear();
	}
#else
	(void)preserve_ra_session;
#endif

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

#ifdef XM8_ENABLE_RETROACHIEVEMENTS
	AttachRaHostFrameCallback();
#endif

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
#ifdef XM8_ENABLE_RETROACHIEVEMENTS
	if (!preserve_ra_session && ra_mode_enabled) {
		BeginRaSessionForMountedDrive1();
	}
#endif
}

//
// GetDiskDir()
// get disk dir
//
const char* App::GetDiskDir(int drive)
{
	if (!disk_open_dir.empty()) {
		return disk_open_dir.c_str();
	}

	// drive 1
	if ((drive == -1) || (drive == 0)) {
		if (diskmgr[0]->IsOpen() == true) {
#ifdef XM8_ENABLE_RETROACHIEVEMENTS
			if (RememberRaSourceDirForMountedDisk(0)) {
				return disk_open_dir.c_str();
			}
#endif
			return diskmgr[0]->GetDir();
		}
	}

	// drive 2
	if ((drive == -1) || (drive == 1)) {
		if (diskmgr[1]->IsOpen() == true) {
#ifdef XM8_ENABLE_RETROACHIEVEMENTS
			if (RememberRaSourceDirForMountedDisk(1)) {
				return disk_open_dir.c_str();
			}
#endif
			return diskmgr[1]->GetDir();
		}
	}

	// no open
	if (diskmgr[0]->IsOpen() == true) {
#ifdef XM8_ENABLE_RETROACHIEVEMENTS
		if (RememberRaSourceDirForMountedDisk(0)) {
			return disk_open_dir.c_str();
		}
#endif
		return diskmgr[0]->GetDir();
	}
	if (diskmgr[1]->IsOpen() == true) {
#ifdef XM8_ENABLE_RETROACHIEVEMENTS
		if (RememberRaSourceDirForMountedDisk(1)) {
			return disk_open_dir.c_str();
		}
#endif
		return diskmgr[1]->GetDir();
	}

	// application base path
	return (const char*)wrapper->get_app_path();
}

//
// RememberDiskOpenDir()
// remember user-selected disk directory
//
void App::RememberDiskOpenDir(const char *path)
{
	const std::string dir = DirectoryOfPath(path);
	if (!dir.empty()) {
		disk_open_dir = dir;
	}
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
#ifdef XM8_ENABLE_RETROACHIEVEMENTS
	const bool preserve_ra_session = ra_mode_enabled && ra_service != NULL &&
		Xm8Ra::IsRaSessionEvaluating(ra_session_state) &&
		ra_service->GameSessionSnapshot().state ==
			Xm8Ra::RaGameSessionState::Loaded;
#endif
	// virtual machine
	vm->reset();

	// resync rtc
	upd1990a->resync();

#ifdef XM8_ENABLE_RETROACHIEVEMENTS
	if (preserve_ra_session) {
		// Reset VM first, then reset rcheevos exactly once for the same game.
		ra_service->ResetProgress();
	}
	else {
		if (ra_service != NULL) ra_service->UnloadGame();
		StopRaSession();
		ra_pending_game_hash.clear();
		ra_pending_library_game_id = 0;
		ra_loaded_library_game_id = 0;
		ra_loaded_game_hash.clear();
		ClearRaMediaChangeState();
		ra_leaderboard_scoreboards.clear();
		RefreshRaAchievementsOverlay();
		RefreshRaLeaderboardsOverlay();
		BeginRaSessionForMountedDrive1();
	}
#endif

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

#ifdef XM8_ENABLE_RETROACHIEVEMENTS
	if (ra_mode_enabled) {
		if (!CheckRaOperation(Xm8Ra::RaRestrictedOperation::LoadState,
			"RA: Load State is unavailable in Hardcore")) return false;
		return LoadRaState(slot);
	}
#endif

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
		const bool loaded = LoadStateBody(&fileio, freq);
		fileio.Fclose();
		if (loaded) {
#ifdef XM8_ENABLE_RETROACHIEVEMENTS
			BeginRaSessionForMountedDrive1();
#endif
			UnlockVM();
			return true;
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

#ifdef XM8_ENABLE_RETROACHIEVEMENTS
	if (ra_mode_enabled) {
		if (!CheckRaOperation(Xm8Ra::RaRestrictedOperation::SaveState,
			"RA: Save State is unavailable in Hardcore")) return false;
		return SaveRaState(slot);
	}
#endif

	// lock vm
	LockVM();

	// state path
	sprintf(name, STATE_FILENAME, slot);
	strcpy(state_path, setting->GetSettingDir());
	strcat(state_path, name);

	// open
	if (fileio.Fopen(state_path, FILEIO_WRITE_BINARY) == true) {
		const bool saved = SaveStateBody(&fileio);

		// close
		fileio.Fclose();

		// success
		const bool complete = saved && !fileio.HasError();
		UnlockVM();
		return complete;
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

#ifdef XM8_ENABLE_RETROACHIEVEMENTS
	if (ra_mode_enabled) {
		Xm8Ra::RaStateExpectation expected;
		std::string path;
		std::string error;
		return GetRaStateContext(slot, &expected, &path, &error) &&
			platform->GetFileDateTime(path.c_str(), cur_time);
	}
#endif

	// state path
	sprintf(name, STATE_FILENAME, slot);
	strcpy(state_path, setting->GetSettingDir());
	strcat(state_path, name);

	// platform
	return platform->GetFileDateTime(state_path, cur_time);
}

bool App::LoadStateBody(FILEIO *fileio, int previous_audio_frequency,
	bool preserve_ra_session)
{
	if (!setting->LoadSetting(fileio)) {
		return false;
	}
	ChangeSystemInternal(true, preserve_ra_session);
	diskmgr[0]->Load(fileio);
	diskmgr[1]->Load(fileio);
	tapemgr->Load(fileio);
	if (!vm->load_state(fileio) || fileio->HasError()) {
		return false;
	}
	memset(draw_tick, 0, sizeof(draw_tick));
	draw_tick_count = 0;
	draw_tick_point = 0;
	if (setting->GetAudioFreq() != previous_audio_frequency) {
		ChangeAudio();
	}
	else if (audio->IsPlay()) {
		audio->Stop();
		audio->Play();
	}
	input->ChangeList(false, false);
	input->ChangeCursorToNumPad(setting->IsCursorToNumPad());
	input->ChangeNumToNumPad(setting->IsNumToNumPad());
	upd1990a->resync();
	return true;
}

bool App::SaveStateBody(FILEIO *fileio)
{
	setting->SaveSetting(fileio);
	diskmgr[0]->Save(fileio);
	diskmgr[1]->Save(fileio);
	tapemgr->Save(fileio);
	vm->save_state(fileio);
	return !fileio->HasError();
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

bool App::GetRaStateContext(int slot,
	Xm8Ra::RaStateExpectation *expected, std::string *path,
	std::string *error) const
{
	if (expected == nullptr || path == nullptr || ra_library == nullptr ||
		diskmgr[0] == nullptr || !diskmgr[0]->IsOpen()) {
		if (error != nullptr) *error = "RA state requires mounted Drive 1 media";
		return false;
	}
	const std::string disk_path = diskmgr[0]->GetPath();
	std::string media_root = ra_library->MediaRoot();
	if (!media_root.empty() && media_root.back() != '/' &&
		media_root.back() != '\\') {
		media_root += '/';
	}
	const std::string anchor_md5 = ParentDirectoryName(disk_path);
	if (disk_path.compare(0, media_root.size(), media_root) != 0 ||
		anchor_md5.size() != 32) {
		if (error != nullptr) *error = "RA anchor media is unavailable";
		return false;
	}

	Xm8Ra::RaStateExpectation context;
	context.anchor_md5 = anchor_md5;
	context.rcheevos_version = Xm8RaBuildInfo::RcheevosVersion();
	if (Xm8Ra::IsRaSessionOffline(ra_session_state)) {
		context.mode = Xm8Ra::RaStateMode::Offline;
		context.game_id = 0;
	}
	else if (Xm8Ra::IsRaSessionEvaluating(ra_session_state) &&
		ra_service != nullptr &&
		ra_service->GameSessionSnapshot().state ==
			Xm8Ra::RaGameSessionState::Loaded) {
		context.mode = Xm8Ra::RaStateMode::Casual;
		context.game_id = ra_service->GameSessionSnapshot().game_id;
	}
	else {
		if (error != nullptr) *error = "RA session is not ready for states";
		return false;
	}

	*path = Xm8Ra::RaStatePath(ra_library->Root(), context.mode,
		context.game_id, context.anchor_md5, slot);
	if (path->empty()) {
		if (error != nullptr) *error = "invalid RA state path";
		return false;
	}
	*expected = context;
	if (error != nullptr) error->clear();
	return true;
}

bool App::SaveRaState(int slot)
{
	Xm8Ra::RaStateExpectation expected;
	std::string path;
	std::string error;
	LockVM();
	if (!GetRaStateContext(slot, &expected, &path, &error)) {
		UnlockVM();
		AddRaNotice("RA: " + error);
		return false;
	}

	Xm8Ra::RaStateRecord record;
	record.mode = expected.mode;
	record.game_id = expected.game_id;
	record.anchor_md5 = expected.anchor_md5;
	record.rcheevos_version = expected.rcheevos_version;
	if (record.mode == Xm8Ra::RaStateMode::Casual &&
		!ra_service->SerializeProgress(&record.progress, &error)) {
		UnlockVM();
		AddRaNotice("RA: state progress save failed");
		return false;
	}

	std::ostringstream temporary_name;
	temporary_name << ra_library->TempRoot() << "/state-body-save-" << slot;
	const std::string temporary = temporary_name.str();
	FILEIO fileio;
	if (!fileio.Fopen(const_cast<char *>(temporary.c_str()),
		FILEIO_WRITE_BINARY)) {
		UnlockVM();
		AddRaNotice("RA: state save failed");
		return false;
	}
	const bool body_written = SaveStateBody(&fileio);
	fileio.Fclose();
	const bool body_read = body_written && !fileio.HasError() &&
		Xm8Ra::ReadRaStateFile(temporary, &record.body, &error);
	std::remove(temporary.c_str());
	std::vector<uint8_t> bytes;
	const bool saved = body_read && Xm8Ra::BuildRaState(record, &bytes,
		&error) && Xm8Ra::WriteRaStateFileAtomically(path, bytes, &error);
	UnlockVM();
	if (!saved) {
		AddRaNotice("RA: state save failed");
	}
	return saved;
}

bool App::LoadRaState(int slot)
{
	Xm8Ra::RaStateExpectation expected;
	std::string path;
	std::string error;
	LockVM();
	if (!GetRaStateContext(slot, &expected, &path, &error)) {
		UnlockVM();
		AddRaNotice("RA: " + error);
		return false;
	}

	std::vector<uint8_t> bytes;
	Xm8Ra::RaStateRecord record;
	if (!Xm8Ra::ReadRaStateFile(path, &bytes, &error) ||
		!Xm8Ra::ParseRaState(bytes, &record, &error) ||
		!Xm8Ra::ValidateRaState(record, expected, &error)) {
		UnlockVM();
		AddRaNotice("RA: state rejected - " + error);
		return false;
	}

	std::ostringstream base_name;
	base_name << ra_library->TempRoot() << "/state-body-load-" << slot;
	const std::string target_body = base_name.str();
	const std::string rollback_body = target_body + ".rollback";
	if (!Xm8Ra::WriteRaStateFileAtomically(target_body, record.body, &error)) {
		UnlockVM();
		AddRaNotice("RA: state preparation failed");
		return false;
	}

	FILEIO rollback;
	if (!rollback.Fopen(const_cast<char *>(rollback_body.c_str()),
		FILEIO_WRITE_BINARY)) {
		std::remove(target_body.c_str());
		UnlockVM();
		AddRaNotice("RA: state rollback preparation failed");
		return false;
	}
	const bool rollback_written = SaveStateBody(&rollback);
	rollback.Fclose();
	if (!rollback_written || rollback.HasError()) {
		std::remove(target_body.c_str());
		std::remove(rollback_body.c_str());
		UnlockVM();
		AddRaNotice("RA: state rollback preparation failed");
		return false;
	}

	const int previous_frequency = setting->GetAudioFreq();
	FILEIO target;
	bool loaded = target.Fopen(const_cast<char *>(target_body.c_str()),
		FILEIO_READ_BINARY) && LoadStateBody(&target, previous_frequency, true);
	target.Fclose();
	if (!loaded) {
		FILEIO restore;
		bool restored = false;
		if (restore.Fopen(const_cast<char *>(rollback_body.c_str()),
			FILEIO_READ_BINARY)) {
			restored = LoadStateBody(&restore, previous_frequency, true);
		}
		restore.Fclose();
		restored = restored && !restore.HasError();
		std::remove(target_body.c_str());
		std::remove(rollback_body.c_str());
		UnlockVM();
		if (!restored) {
			EnterRaOfflineSession("state rollback failed");
		}
		else {
			AddRaNotice("RA: state load failed; previous state restored");
		}
		return false;
	}
	std::remove(target_body.c_str());
	std::remove(rollback_body.c_str());

	if (record.mode == Xm8Ra::RaStateMode::Casual &&
		!ra_service->DeserializeProgress(record.progress, &error)) {
		ra_service->ResetProgress();
		AddRaNotice("RA: state loaded; achievement progress reset");
	}
	RefreshRaAchievementsOverlay();
	RefreshRaLeaderboardsOverlay();
	UnlockVM();
	return true;
}

//
// IsRaModeEnabled()
// get RA mode setting
//
bool App::IsRaModeEnabled() const
{
	return ra_mode_enabled;
}

//
// CheckRaStateAvailability()
// validate RA state menu access and notify on failure
//
bool App::CheckRaStateAvailability()
{
	if (!ra_mode_enabled) {
		AddRaNotice("RA: state is unavailable while RA mode is disabled");
		return false;
	}
	if (!CheckRaOperation(Xm8Ra::RaRestrictedOperation::LoadState,
		"RA: states are unavailable in Hardcore")) {
		return false;
	}
	Xm8Ra::RaStateExpectation expected;
	std::string path;
	std::string error;
	if (!GetRaStateContext(0, &expected, &path, &error)) {
		AddRaNotice("RA: state unavailable - " + error);
		return false;
	}
	return true;
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
	ra_menu_status.Set(enable ? Xm8Ra::RaMenuStatusState::Enabled :
		Xm8Ra::RaMenuStatusState::Disabled);
	StopRaSession();
	ra_saved_login_started = false;
	ra_manual_login_started = false;
	ra_library_sync_started_for_login = false;
	ra_pending_game_hash.clear();
	ra_pending_library_game_id = 0;
	ra_loaded_library_game_id = 0;
	ra_loaded_game_hash.clear();
	ClearRaMediaChangeState();
	ra_leaderboard_scoreboards.clear();
	if (!enable && ra_service != NULL) {
		ra_service->UnloadGame();
	}
	else if (enable) {
		BeginRaSavedTokenLogin(false);
		BeginRaSessionForMountedDrive1();
	}
	return true;
}

bool App::ToggleRaPlayMode()
{
	const Xm8Ra::RaPlayMode mode =
		ra_play_mode == Xm8Ra::RaPlayMode::Hardcore ?
		Xm8Ra::RaPlayMode::Casual : Xm8Ra::RaPlayMode::Hardcore;
	std::string error;
	if (!SaveRaPlayModeSetting(mode, &error)) {
		AddRaNotice("RA: setting save failed");
		return false;
	}
	ra_play_mode = mode;
	if (ra_mode_enabled) {
		if (ra_service != NULL) ra_service->UnloadGame();
		StopRaSession();
		if (mode == Xm8Ra::RaPlayMode::Hardcore) NormalSpeed();
		LockVM();
		vm->reset();
		upd1990a->resync();
		UnlockVM();
		BeginRaSessionForMountedDrive1();
	}
	AddRaNotice(mode == Xm8Ra::RaPlayMode::Hardcore ?
		"RA: Hardcore selected" : "RA: Casual selected");
	return true;
}

bool App::IsRaHardcoreSelected() const
{
	return ra_play_mode == Xm8Ra::RaPlayMode::Hardcore;
}

bool App::IsRaHardcoreActive() const
{
	return Xm8Ra::IsRaHardcoreSession(GetRaPolicyContext());
}

bool App::ToggleFastDisk()
{
	if (!CheckRaOperation(Xm8Ra::RaRestrictedOperation::FastDisk,
		"RA: Pseudo fast disk is unavailable during an online session")) {
		return false;
	}
	setting->SetFastDisk(!setting->IsFastDisk());
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

namespace {

std::string RaGameStatusMessage(Xm8Ra::RaLoginState login_state,
	Xm8Ra::RaGameSessionState game_state)
{
	if (login_state == Xm8Ra::RaLoginState::LoggedOut) {
		return "RA login required";
	}
	if (login_state == Xm8Ra::RaLoginState::LoginPending) {
		return "RA login pending";
	}
	if (login_state == Xm8Ra::RaLoginState::Failed) {
		return "RA login failed";
	}
	if (game_state == Xm8Ra::RaGameSessionState::LoadPending) {
		return "RA game loading";
	}
	if (game_state == Xm8Ra::RaGameSessionState::DisabledForSession) {
		return "Unsupported RA game";
	}
	return "No RA game loaded";
}

void CopyClippedMenuText(char *target, size_t target_size,
	const char *source, int width)
{
	if (target == nullptr || target_size == 0) {
		return;
	}
	target[0] = '\0';
	if (source == nullptr || width <= 0) {
		return;
	}

	const int max_width = width;
	if (max_width <= 0) {
		return;
	}
	if (SjisTextWidth(source) <= max_width &&
		std::strlen(source) < target_size) {
		std::snprintf(target, target_size, "%s", source);
		return;
	}

	const char *ellipsis = "...";
	const int ellipsis_width = 24;
	const int available = max_width > ellipsis_width ?
		max_width - ellipsis_width : max_width;
	size_t source_offset = 0;
	size_t target_offset = 0;
	int used_width = 0;
	while (source[source_offset] != '\0' && target_offset + 1 < target_size) {
		const int char_width = SjisCharWidth(source, source_offset);
		const size_t char_bytes = SjisCharBytes(source, source_offset);
		if (used_width + char_width > available ||
			target_offset + char_bytes >= target_size) {
			break;
		}
		std::memcpy(target + target_offset, source + source_offset,
			char_bytes);
		target_offset += char_bytes;
		source_offset += char_bytes;
		used_width += char_width;
	}
	target[target_offset] = '\0';
	if (target_offset + 4 <= target_size && max_width > ellipsis_width) {
		std::snprintf(target + target_offset, target_size - target_offset,
			"%s", ellipsis);
		return;
	}
}

std::string ToSjisMenuText(Converter *converter, const std::string& text)
{
	if (text.empty() || converter == NULL) {
		return text;
	}
	const Xm8Ra::RaSanitizedText sanitized =
		Xm8Ra::RaTextConverter::SanitizeUtf8(text);
	std::vector<char> sjis(sanitized.utf8.size() * 3 + 1);
	converter->UtfToSjis(sanitized.utf8.c_str(), sjis.data());
	return std::string(sjis.data());
}

size_t SjisCharBytes(const char *source, size_t offset)
{
	return Xm8Ra::RaTextConverter::SjisCharBytes(source, offset);
}

int SjisCharWidth(const char *source, size_t offset)
{
	return Xm8Ra::RaTextConverter::SjisCharWidth(source, offset);
}

int SjisTextWidth(const char *source)
{
	return Xm8Ra::RaTextConverter::SjisTextWidth(source);
}

size_t SjisCharCount(const char *source)
{
	return Xm8Ra::RaTextConverter::SjisCharCount(source);
}

size_t SjisByteOffsetForChar(const char *source, size_t char_index)
{
	return Xm8Ra::RaTextConverter::SjisByteOffsetForChar(source,
		char_index);
}

void CopyAutoScrollMenuText(char *target, size_t target_size,
	const char *source, int width, Uint32 elapsed_ms)
{
	if (target == nullptr || target_size == 0) {
		return;
	}
	target[0] = '\0';
	if (source == nullptr || width <= 0) {
		return;
	}
	if (SjisTextWidth(source) <= width) {
		std::snprintf(target, target_size, "%s", source);
		return;
	}

	const size_t char_count = SjisCharCount(source);
	size_t max_start = 0;
	for (size_t start = 0; start < char_count; ++start) {
		const size_t byte_offset = SjisByteOffsetForChar(source, start);
		if (SjisTextWidth(source + byte_offset) <= width) {
			max_start = start;
			break;
		}
	}
	if (max_start == 0) {
		max_start = char_count > 0 ? char_count - 1 : 0;
	}

	const Uint32 hold_ms = 1000;
	const Uint32 step_ms = 200;
	const Uint32 cycle_ms = hold_ms + static_cast<Uint32>(max_start) *
		step_ms + hold_ms;
	Uint32 position_ms = cycle_ms == 0 ? 0 : elapsed_ms % cycle_ms;
	size_t start_char = 0;
	if (position_ms >= hold_ms) {
		position_ms -= hold_ms;
		const size_t step = static_cast<size_t>(position_ms / step_ms);
		start_char = step > max_start ? max_start : step;
	}

	size_t source_offset = SjisByteOffsetForChar(source, start_char);
	size_t target_offset = 0;
	int used_width = 0;
	while (source[source_offset] != '\0' && target_offset + 1 < target_size) {
		const int char_width = SjisCharWidth(source, source_offset);
		const size_t char_bytes = SjisCharBytes(source, source_offset);
		if (used_width + char_width > width ||
			target_offset + char_bytes >= target_size) {
			break;
		}
		std::memcpy(target + target_offset, source + source_offset,
			char_bytes);
		target_offset += char_bytes;
		source_offset += char_bytes;
		used_width += char_width;
	}
	target[target_offset] = '\0';
}

void WrapSjisMenuText(std::vector<std::string> *lines, const char *source,
	int width)
{
	if (lines == nullptr || source == nullptr || *source == '\0') {
		return;
	}
	std::string line;
	int line_width = 0;
	for (size_t offset = 0; source[offset] != '\0';) {
		const int char_width = SjisCharWidth(source, offset);
		const size_t char_bytes = SjisCharBytes(source, offset);
		if (!line.empty() && line_width + char_width > width) {
			lines->push_back(line);
			line.clear();
			line_width = 0;
		}
		line.append(source + offset, char_bytes);
		line_width += char_width;
		offset += char_bytes;
	}
	if (!line.empty()) {
		lines->push_back(line);
	}
}

const char *RaHealthLabel(int health_state)
{
	switch (health_state) {
	case Xm8Ra::kRaMediaHealthOk:
		return "OK";
	case Xm8Ra::kRaMediaHealthSourceMissing:
		return "Source missing";
	case Xm8Ra::kRaMediaHealthSourceChanged:
		return "Source changed";
	case Xm8Ra::kRaMediaHealthWorkingMissing:
		return "Working missing";
	case Xm8Ra::kRaMediaHealthWorkingCorrupt:
		return "Working corrupt";
	default:
		return "Unknown";
	}
}

std::string FormatRaUnixTime(int64_t unix_time)
{
	if (unix_time <= 0) {
		return "Never";
	}
	std::time_t time_value = static_cast<std::time_t>(unix_time);
	std::tm *local = std::localtime(&time_value);
	if (local == NULL) {
		return "Unknown";
	}
	char buffer[32];
	if (std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M", local) == 0) {
		return "Unknown";
	}
	return buffer;
}

SDL_Rect RaGameDetailStartButtonRect()
{
	SDL_Rect panel = {
		(SCREEN_WIDTH / 2) - (MENUITEM_WIDTH / 2),
		(SCREEN_HEIGHT / 2) - ((MENUITEM_HEIGHT * MENUITEM_LINES) / 2),
		MENUITEM_WIDTH,
		MENUITEM_HEIGHT * MENUITEM_LINES
	};
	SDL_Rect body = {panel.x + 1, panel.y + MENUITEM_HEIGHT + 1,
		panel.w - 2, panel.h - MENUITEM_HEIGHT - 2};
	SDL_Rect button = {body.x + 104, body.y + 142, body.w - 128, 34};
	return button;
}

bool PointInRect(int x, int y, const SDL_Rect& rect)
{
	return x >= rect.x && x < rect.x + rect.w &&
		y >= rect.y && y < rect.y + rect.h;
}

} // namespace

//
// MakeRaLibraryOverlaySnapshot()
// build RA library overlay snapshot
//
Xm8Ra::RaOverlayLibraryListSnapshot App::MakeRaLibraryOverlaySnapshot() const
{
	Xm8Ra::RaOverlayLibraryListSnapshot overlay_snapshot;
	if (ra_library == NULL) {
		overlay_snapshot.status_message = "RA library unavailable";
		return overlay_snapshot;
	}

	std::vector<Xm8Ra::RaLibraryGameListItem> games;
	std::string error;
	std::string username;
	if (ra_service != NULL) {
		username = ra_service->LoginSnapshot().username;
	}
	if (!ra_library->ListGamesForUser(username, &games, &error)) {
		overlay_snapshot.status_message = error.empty() ?
			"Cannot load RA library" : error;
		return overlay_snapshot;
	}
	if (games.empty()) {
		overlay_snapshot.status_message =
			"No RetroAchievements games in library";
		return overlay_snapshot;
	}

	for (const Xm8Ra::RaLibraryGameListItem& source : games) {
		Xm8Ra::RaOverlayLibraryItem item;
		item.game_id = source.game_id;
		item.ra_game_id = source.ra_game_id;
		item.identification_state = source.identification_state;
		if (source.identification_state == Xm8Ra::kRaIdentificationConflict) {
			Xm8Ra::RaGameConflictInfo conflict;
			std::string conflict_error;
			if (ra_library->InspectGameConflict(source.game_id, &conflict,
				&conflict_error)) {
				item.conflict_kind = static_cast<Xm8Ra::RaOverlayConflictKind>(
					conflict.kind);
			}
		}
		item.title = source.title;
		item.media_count = source.media_count;
		item.health_state = source.health_state;
		item.last_played_at = source.last_played_at;
		item.has_progress = source.has_progress;
		item.core_total = source.core_total;
		item.core_unlocked = source.core_unlocked;
		item.hardcore_unlocked = source.hardcore_unlocked;
		item.points_total = source.points_total;
		item.points_unlocked = source.points_unlocked;
		item.badge_url = source.badge_url;
		overlay_snapshot.games.push_back(item);
	}
	return overlay_snapshot;
}

//
// MakeRaAchievementsOverlaySnapshot()
// build RA achievements overlay snapshot
//
Xm8Ra::RaOverlayAchievementListSnapshot App::MakeRaAchievementsOverlaySnapshot() const
{
	Xm8Ra::RaOverlayAchievementListSnapshot overlay_snapshot;
	if (ra_service == NULL) {
		overlay_snapshot.status_message = "RA service unavailable";
	}
	else {
		const Xm8Ra::RaAchievementListSnapshot service_snapshot =
			ra_service->AchievementListSnapshot();
		overlay_snapshot.game_loaded = service_snapshot.game_loaded;
		overlay_snapshot.has_achievements =
			service_snapshot.has_achievements;
		overlay_snapshot.game_title = service_snapshot.game_title;
		if (Xm8Ra::IsRaSessionOffline(ra_session_state)) {
			overlay_snapshot.status_message = "RA offline for this session";
		}
		else if (!service_snapshot.game_loaded) {
			const Xm8Ra::RaLoginSnapshot login =
				ra_service->LoginSnapshot();
			const Xm8Ra::RaGameSessionSnapshot game =
				ra_service->GameSessionSnapshot();
			overlay_snapshot.status_message =
				RaGameStatusMessage(login.state, game.state);
		}
		else if (!service_snapshot.has_achievements) {
			overlay_snapshot.status_message = "No achievements";
		}

		for (const Xm8Ra::RaAchievementListItem& source :
			service_snapshot.achievements) {
			Xm8Ra::RaOverlayAchievementItem item;
			item.id = source.id;
			item.points = source.points;
			item.unlocked = source.unlocked;
			item.title = source.title;
			item.description = source.description;
			item.measured_progress = source.measured_progress;
			item.badge_url = source.badge_url;
			item.badge_locked_url = source.badge_locked_url;
			item.bucket_label = source.bucket_label;
			overlay_snapshot.achievements.push_back(item);
		}
	}
	return overlay_snapshot;
}

//
// MakeRaLeaderboardsOverlaySnapshot()
// build RA leaderboards overlay snapshot
//
Xm8Ra::RaOverlayLeaderboardListSnapshot App::MakeRaLeaderboardsOverlaySnapshot() const
{
	Xm8Ra::RaOverlayLeaderboardListSnapshot overlay_snapshot;
	if (ra_service == NULL) {
		overlay_snapshot.status_message = "RA service unavailable";
	}
	else {
		const Xm8Ra::RaLeaderboardListSnapshot service_snapshot =
			ra_service->LeaderboardListSnapshot();
		const Xm8Ra::RaLeaderboardEntriesSnapshot entries_snapshot =
			ra_service->LeaderboardEntriesSnapshot();
		overlay_snapshot.game_loaded = service_snapshot.game_loaded;
		overlay_snapshot.game_title = service_snapshot.game_title;
		if (Xm8Ra::IsRaSessionOffline(ra_session_state)) {
			overlay_snapshot.status_message = "RA offline for this session";
		}
		else if (!service_snapshot.game_loaded) {
			const Xm8Ra::RaLoginSnapshot login =
				ra_service->LoginSnapshot();
			const Xm8Ra::RaGameSessionSnapshot game =
				ra_service->GameSessionSnapshot();
			overlay_snapshot.status_message =
				RaGameStatusMessage(login.state, game.state);
		}
		else if (!service_snapshot.has_leaderboards) {
			overlay_snapshot.status_message = "No leaderboards";
		}

		for (const Xm8Ra::RaLeaderboardListItem& source :
			service_snapshot.leaderboards) {
			Xm8Ra::RaOverlayLeaderboardItem item;
			item.id = source.id;
			item.state = source.state;
			item.format = source.format;
			item.lower_is_better = source.lower_is_better;
			item.title = source.title;
			item.description = source.description;
			item.tracker_value = source.tracker_value;
			item.bucket_label = source.bucket_label;
			const auto scoreboard =
				ra_leaderboard_scoreboards.find(item.id);
			if (scoreboard != ra_leaderboard_scoreboards.end()) {
				item.has_scoreboard = true;
				item.new_rank = scoreboard->second.new_rank;
				item.num_entries = scoreboard->second.num_entries;
				item.submitted_score =
					scoreboard->second.submitted_score;
				item.best_score = scoreboard->second.best_score;
				for (const auto& source_entry :
					scoreboard->second.top_entries) {
					Xm8Ra::RaOverlayLeaderboardItem::ScoreboardEntry entry;
					entry.rank = source_entry.rank;
					entry.username = source_entry.username;
					entry.score = source_entry.score;
					item.top_entries.push_back(entry);
				}
			}
			if (entries_snapshot.leaderboard_id == item.id) {
				item.entries_pending = entries_snapshot.state ==
					Xm8Ra::RaLeaderboardEntriesState::FetchPending;
				item.entries_failed = entries_snapshot.state ==
					Xm8Ra::RaLeaderboardEntriesState::Failed;
				item.has_entries = entries_snapshot.state ==
					Xm8Ra::RaLeaderboardEntriesState::Loaded;
				item.entry_total = entries_snapshot.total_entries;
				item.entries_message = entries_snapshot.message;
				for (const auto& source_entry :
					entries_snapshot.entries) {
					Xm8Ra::RaOverlayLeaderboardItem::ScoreboardEntry entry;
					entry.rank = source_entry.rank;
					entry.username = source_entry.username;
					entry.score = source_entry.display;
					item.entries.push_back(entry);
				}
			}
			overlay_snapshot.leaderboards.push_back(item);
		}
	}
	return overlay_snapshot;
}

//
// RefreshRaAchievementsOverlay()
// refresh achievements overlay if it is visible
//
void App::RefreshRaAchievementsOverlay()
{
	if (ra_overlay != NULL &&
		ra_overlay->Screen() == Xm8Ra::RaOverlayScreen::Achievements) {
		ra_overlay->OpenAchievements(MakeRaAchievementsOverlaySnapshot());
	}
}

//
// RefreshRaLeaderboardsOverlay()
// refresh leaderboards overlay if it is visible
//
void App::RefreshRaLeaderboardsOverlay()
{
	if (ra_overlay != NULL &&
		ra_overlay->Screen() == Xm8Ra::RaOverlayScreen::Leaderboards) {
		ra_overlay->UpdateLeaderboards(MakeRaLeaderboardsOverlaySnapshot());
	}
}

//
// EnsureRaLeaderboardEntriesForSelection()
// fetch selected leaderboard entries if needed
//
void App::EnsureRaLeaderboardEntriesForSelection()
{
	if (ra_overlay == NULL || ra_service == NULL ||
		ra_overlay->Screen() != Xm8Ra::RaOverlayScreen::Leaderboards) {
		return;
	}

	const Xm8Ra::RaOverlayLeaderboardListSnapshot leaderboards =
		ra_overlay->LeaderboardListSnapshot();
	if (leaderboards.selected_index >= leaderboards.leaderboards.size()) {
		return;
	}

	const uint32_t leaderboard_id =
		leaderboards.leaderboards[leaderboards.selected_index].id;
	if (leaderboard_id == 0 ||
		ra_leaderboard_scoreboards.find(leaderboard_id) !=
			ra_leaderboard_scoreboards.end()) {
		return;
	}

	const Xm8Ra::RaLeaderboardEntriesSnapshot entries =
		ra_service->LeaderboardEntriesSnapshot();
	if (entries.leaderboard_id == leaderboard_id &&
		(entries.state == Xm8Ra::RaLeaderboardEntriesState::FetchPending ||
		 entries.state == Xm8Ra::RaLeaderboardEntriesState::Loaded ||
		 entries.state == Xm8Ra::RaLeaderboardEntriesState::Failed)) {
		return;
	}

	std::string error;
	if (ra_service->BeginFetchLeaderboardEntries(leaderboard_id, 1, 5,
		&error)) {
		RefreshRaLeaderboardsOverlay();
	}
	else if (!error.empty()) {
		AddRaNotice("RA: " + error);
	}
}

//
// OpenRaLibraryOverlay()
// open RA library overlay
//
void App::OpenRaLibraryOverlay()
{
	if (ra_overlay == NULL) {
		AddRaNotice("RA: overlay unavailable");
		return;
	}

	ra_overlay->OpenLibrary(MakeRaLibraryOverlaySnapshot());
	ra_overlay_joystick_prev = 0;
	ClearRaOverlayPointerState();
	SDL_StopTextInput();
	if (app_menu == true) {
		LeaveMenu(false);
	}
	CtrlAudio();
}

//
// LaunchRaLibraryGame()
// launch registered RA library game
//
bool App::LaunchRaLibraryGame(int64_t game_id, std::string *error)
{
	if (ra_media_store == NULL || ra_library == NULL) {
		if (error != NULL) {
			*error = "RA library unavailable";
		}
		return false;
	}
	int64_t ra_game_id = 0;
	int identification_state = Xm8Ra::kRaIdentificationUnidentified;
	if (!ra_library->LoadGameIdentification(game_id, &ra_game_id,
		&identification_state, error)) {
		return false;
	}
	if (identification_state == Xm8Ra::kRaIdentificationConflict) {
		if (error != NULL) {
			*error = "media conflict must be resolved before launch";
		}
		return false;
	}
	if (identification_state != Xm8Ra::kRaIdentificationIdentified ||
		ra_game_id <= 0) {
		if (error != NULL) {
			*error = "game is not identified for RetroAchievements";
		}
		return false;
	}
	// Capture an already-mounted auxiliary disk as well. This migrates the
	// former Drive-1-only default the first time an existing game is started.
	if (!RememberRaLaunchDriveForMountedDisk(1, error)) {
		return false;
	}

	Xm8Ra::ResolvedLaunchProfile profile;
	if (!ra_media_store->ResolveLaunchProfile(game_id, &profile, error)) {
		return false;
	}

	std::string anchor_hash;
	for (int drive = 0; drive < MAX_DRIVE; drive++) {
		const Xm8Ra::ResolvedLaunchDisk& disk = profile.drives[drive];
		if (!disk.assigned || !disk.is_ra_anchor) {
			continue;
		}
		Xm8Ra::D88MediaInfo media;
		if (!Xm8Ra::ProbeD88File(disk.working_path.c_str(), &media, error)) {
			return false;
		}
		if (disk.bank_index < 0 ||
			disk.bank_index >= static_cast<int>(media.bank_md5s.size())) {
			if (error != NULL) {
				*error = "RA anchor bank hash is not available";
			}
			return false;
		}
		anchor_hash = media.bank_md5s[disk.bank_index];
	}
	if (anchor_hash.empty()) {
		if (error != NULL) {
			*error = "RA launch profile has no anchor";
		}
		return false;
	}

	for (int drive = 0; drive < MAX_DRIVE; drive++) {
		const Xm8Ra::ResolvedLaunchDisk& disk = profile.drives[drive];
		if (!disk.assigned) {
			continue;
		}
		DiskSpec spec = {disk.working_path, drive, disk.bank_index};
		int banks = 0;
		if (!ProbeDisk(spec, &banks, error)) {
			return false;
		}
	}

	struct Snapshot {
		bool open;
		std::string path;
		int bank;
	};
	Snapshot snapshots[MAX_DRIVE];

	LockVM();
	for (int drive = 0; drive < MAX_DRIVE; drive++) {
		snapshots[drive].open = diskmgr[drive]->IsOpen();
		if (snapshots[drive].open) {
			snapshots[drive].path = diskmgr[drive]->GetPath();
			snapshots[drive].bank = diskmgr[drive]->GetBank();
		}
	}
	auto restore = [this, &snapshots]() {
		for (int drive = 0; drive < MAX_DRIVE; drive++) {
			if (snapshots[drive].open) {
				diskmgr[drive]->Open(snapshots[drive].path.c_str(),
					snapshots[drive].bank);
			}
			else {
				diskmgr[drive]->Close();
			}
		}
	};

	for (int drive = 0; drive < MAX_DRIVE; drive++) {
		const Xm8Ra::ResolvedLaunchDisk& disk = profile.drives[drive];
		if (!disk.assigned) {
			diskmgr[drive]->Close();
			continue;
		}
		if (!diskmgr[drive]->Open(disk.working_path.c_str(),
			disk.bank_index)) {
			if (error != NULL && error->empty()) {
				*error = "failed to open RA working copy";
			}
			restore();
			UnlockVM();
			return false;
		}
	}
	// START represents a fresh boot of the saved disk configuration.
	vm->reset();
	upd1990a->resync();
	UnlockVM();

	if (ra_service != NULL) {
		ra_service->UnloadGame();
	}
	ra_pending_game_hash.clear();
	ra_pending_library_game_id = 0;
	ra_loaded_library_game_id = 0;
	ra_loaded_game_hash.clear();
	ClearRaMediaChangeState();
	ra_leaderboard_scoreboards.clear();
	BeginRaSessionForMedia(anchor_hash, game_id);
	ra_library->MarkGamePlayed(game_id, nullptr);
	return true;
}

//
// OpenRaAchievementsOverlay()
// open RA achievements overlay
//
void App::OpenRaAchievementsOverlay()
{
	if (ra_overlay == NULL) {
		AddRaNotice("RA: overlay unavailable");
		return;
	}

	ra_overlay->OpenAchievements(MakeRaAchievementsOverlaySnapshot());
	ra_overlay_joystick_prev = 0;
	ClearRaOverlayPointerState();
	SDL_StopTextInput();
	if (app_menu == true) {
		LeaveMenu(false);
	}
	CtrlAudio();
}

//
// OpenRaLeaderboardsOverlay()
// open RA leaderboards overlay
//
void App::OpenRaLeaderboardsOverlay()
{
	if (ra_overlay == NULL) {
		AddRaNotice("RA: overlay unavailable");
		return;
	}

	ra_overlay->OpenLeaderboards(MakeRaLeaderboardsOverlaySnapshot());
	EnsureRaLeaderboardEntriesForSelection();
	ra_overlay_joystick_prev = 0;
	ClearRaOverlayPointerState();
	SDL_StopTextInput();
	if (app_menu == true) {
		LeaveMenu(false);
	}
	CtrlAudio();
}

//
// OpenRaWebsite()
// open RetroAchievements in the default browser
//
void App::OpenRaWebsite()
{
	if (SDL_OpenURL("https://retroachievements.org") != 0) {
		AddRaNotice("RA: could not open website");
	}
}

//
// CloseRaOverlayToMenu()
// close RA overlay and return to RetroAchievements menu
//
void App::CloseRaOverlayToMenu()
{
	if (ra_overlay == NULL) {
		return;
	}
	ra_overlay->CloseScreen();
	SDL_StopTextInput();
	ClearRaOverlayPointerState();
	EnterMenu(MENU_RA);
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
		ra_menu_status.Set(Xm8Ra::RaMenuStatusState::Enabled);
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
	ra_overlay_joystick_prev = 0;
	ClearRaOverlayPointerState();
	SDL_StartTextInput();
	if (app_menu == true) {
		LeaveMenu(false);
	}
	CtrlAudio();
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
	ra_session_state = Xm8Ra::TransitionRaSession(ra_session_state,
		Xm8Ra::RaSessionSignal::SessionInvalidated);
	RestoreRaSessionOverrides();
	if (audio != NULL) CtrlAudio();
	ra_saved_login_started = false;
	ra_manual_login_started = false;
	ra_library_sync_started_for_login = false;
	ra_pending_game_hash.clear();
	ra_pending_library_game_id = 0;
	ra_loaded_library_game_id = 0;
	ra_loaded_game_hash.clear();
	ClearRaMediaChangeState();
	ra_menu_status.Set(ra_mode_enabled ? Xm8Ra::RaMenuStatusState::Enabled :
		Xm8Ra::RaMenuStatusState::Disabled);
	ra_leaderboard_scoreboards.clear();
	ReplaceRaNotice("RA: logged out");
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

	std::string text;
	switch (ra_menu_status.State()) {
	case Xm8Ra::RaMenuStatusState::Unavailable:
		text = "RA: unavailable";
		break;
	case Xm8Ra::RaMenuStatusState::Disabled:
		text = "RA: disabled";
		break;
	case Xm8Ra::RaMenuStatusState::Enabled:
		text = "RA: enabled";
		break;
	case Xm8Ra::RaMenuStatusState::LoginPending:
		text = "RA: login pending";
		break;
	case Xm8Ra::RaMenuStatusState::PendingGame:
		text = "RA: pending " + ra_menu_status.Detail();
		break;
	case Xm8Ra::RaMenuStatusState::ActiveGame:
		text = ra_menu_status.Detail().empty() ?
			"RA: game loaded" : "RA: " + ra_menu_status.Detail();
		break;
	case Xm8Ra::RaMenuStatusState::UnknownGame:
		text = "RA: Unknown game";
		break;
	case Xm8Ra::RaMenuStatusState::OfflineSession:
		text = "RA: offline for session";
		break;
	case Xm8Ra::RaMenuStatusState::Disconnected:
		text = "RA: disconnected";
		break;
	case Xm8Ra::RaMenuStatusState::LoggedIn:
		text = ra_menu_status.Detail().empty() ?
			"RA: logged in" : "RA: logged in " + ra_menu_status.Detail();
		break;
	case Xm8Ra::RaMenuStatusState::LoginFailed:
		text = "RA: login failed";
		break;
	case Xm8Ra::RaMenuStatusState::SubmissionError:
		text = ra_menu_status.Detail();
		break;
	}

	const std::string sjis = ToSjisMenuText(converter, text);
	const std::string bounded =
		Xm8Ra::RaTextConverter::SjisPrefix(sjis, capacity - 1);
	std::snprintf(buffer, capacity, "%s", bounded.c_str());
}

//
// GetRaMenuPresence()
// get current Rich Presence text for the RA menu
//
void App::GetRaMenuPresence(char *buffer, size_t capacity) const
{
	if (buffer == NULL || capacity == 0) {
		return;
	}
	std::string presence;
	if (ra_service != NULL &&
		ra_service->GameSessionSnapshot().state ==
			Xm8Ra::RaGameSessionState::Loaded) {
		presence = ra_service->RichPresence();
	}
	const std::string text = presence.empty() ? "Now: -" :
		"Now: " + presence;
	const std::string sjis = ToSjisMenuText(converter, text);
	const std::string bounded =
		Xm8Ra::RaTextConverter::SjisPrefix(sjis, capacity - 1);
	std::snprintf(buffer, capacity, "%s", bounded.c_str());
}

//
// ReadRaInspectionMemory()
// read current VM memory for RA
//
uint32_t App::ReadRaInspectionMemory(uint32_t address, uint8_t *buffer,
	uint32_t num_bytes) const
{
	if (vm == NULL || buffer == NULL) {
		return 0;
	}
	return static_cast<uint32_t>(
		vm->read_ra_inspection_memory(address, buffer, num_bytes));
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
