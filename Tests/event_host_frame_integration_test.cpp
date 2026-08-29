#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include <SDL.h>

#include "Fixtures/d88_fixture.h"
#include "emu.h"
#include "diskmgr.h"
#include "event.h"
#include "pc8801.h"

namespace {

struct FrameCounter {
	int count = 0;
};

void CountFrame(void *userdata)
{
	static_cast<FrameCounter *>(userdata)->count++;
}

void Require(bool condition, const char *message)
{
	if(!condition) {
		std::fprintf(stderr, "FAIL: %s\n", message);
		std::exit(1);
	}
}

} // namespace

// EVENT itself does not need a window. These definitions provide the smallest
// host boundary required to construct and run the real PC-8801MA VM in a test.
EMU::EMU(EMU_SDL *wrapper)
{
	(void)wrapper;
	std::memset(key_buffer(), 0, 256);
	std::memset(joy_buffer(), 0, sizeof(uint32) * 2);
	std::memset(mouse_buffer(), 0, sizeof(int) * 3);
}

_TCHAR *EMU::application_path()
{
	static _TCHAR path[] = "";
	return path;
}

_TCHAR *EMU::bios_path(_TCHAR *file_name)
{
	return file_name;
}

void EMU::mute_sound() {}
void EMU::get_host_time(cur_time_t *time) { std::memset(time, 0, sizeof(*time)); }
void EMU::printer_out(uint8 value) { (void)value; }
void EMU::printer_strobe(bool value) { (void)value; }
void EMU::out_debug_log(const _TCHAR *format, ...) { (void)format; }
scrntype *EMU::screen_buffer(int y) { (void)y; return nullptr; }
void EMU::set_key_buffer(uint8 *status) { std::memcpy(key_buffer(), status, 256); }
void EMU::set_joy_buffer(uint32 *status) { std::memcpy(joy_buffer(), status, sizeof(uint32) * 2); }
void EMU::Sleep(uint32 ms) { (void)ms; }

int main()
{
	EMU emu(nullptr);
	VM vm(&emu);
	// About 769 samples are mixed per 62.422 Hz VM frame. A 2400-sample
	// buffer therefore forces create_sound32() to complete several frames.
	vm.initialize_sound(48000, 2400);
	vm.reset();

	std::string fixture_error;
	const std::string fixture_dir = "/tmp/xm8-drive2-mount-integration";
	Require(D88Fixture::GenerateStandardSet(fixture_dir, &fixture_error),
		fixture_error.empty() ? "create Drive 2 fixture" : fixture_error.c_str());
	DiskManager drive1;
	DiskManager drive2;
	Require(drive1.Init(&vm, 0), "initialize Drive 1 disk manager");
	Require(drive2.Init(&vm, 1), "initialize Drive 2 disk manager");
	Require(drive2.Open((fixture_dir + "/multi.d88").c_str(), 0),
		"open a two-bank D88 directly in Drive 2");
	Require(drive2.IsOpen(), "Drive 2 manager must report the disk open");
	Require(vm.disk_inserted(1), "the real FDC must report Drive 2 inserted");
	Require(!vm.disk_inserted(0), "Drive 2 open must not modify Drive 1");
	if (const char *external_media = std::getenv("XM8_DRIVE2_MEDIA")) {
		int external_banks = 0;
		Require(DiskManager::Probe(external_media, &external_banks),
			"probe the requested external D88");
		Require(drive1.Open(external_media, 0),
			"open the requested external D88 bank 0 in Drive 1");
		Require(drive2.Open(external_media, 0),
			"open the requested external D88 bank 0 in Drive 2");
		Require(drive1.IsOpen() && vm.disk_inserted(0) &&
			drive2.IsOpen() && vm.disk_inserted(1),
			"the same external D88 bank must coexist in both drives");
		Require(drive2.Open(external_media, external_banks > 1 ? 1 : 0),
			"open the requested external D88 auxiliary bank in Drive 2");
		Require(drive1.IsOpen() && vm.disk_inserted(0),
			"external D88 must remain inserted in the real Drive 1");
		Require(drive2.IsOpen() && vm.disk_inserted(1),
			"external D88 must remain inserted in the real Drive 2");
	}

	EVENT *event = static_cast<EVENT *>(vm.get_device(1));
	Require(event != nullptr, "the real VM must expose its EVENT device");

	FrameCounter frames;
	event->set_host_frame_callback(CountFrame, &frames);

	vm.run();
	Require(frames.count == 1,
		"one direct VM frame must produce exactly one host notification");

	frames.count = 0;
	int extra_frames = 0;
	event->create_sound32(&extra_frames);
	Require(extra_frames > 1,
		"the sound-buffer path must drive multiple VM frames in this test");
	Require(frames.count == extra_frames,
		"every sound-driven VM frame must produce exactly one notification");
	event->create_sound32_after(event->sound_buffer_ptr());

	frames.count = 0;
	event->request_skip_frames();
	Require(event->now_skip(), "the render-skip request must take effect");
	vm.run();
	Require(frames.count == 1,
		"render skipping must not suppress the completed-frame notification");
	Require(!event->now_skip(),
		"ending render skip must restore the ordinary sound-driven path");

	frames.count = 0;
	int total_extra_frames = 0;
	for(int cycle = 0; cycle < 3; cycle++) {
		int cycle_frames = 0;
		event->create_sound32(&cycle_frames);
		total_extra_frames += cycle_frames;
		event->create_sound32_after(event->sound_buffer_ptr());
	}
	Require(total_extra_frames > 1,
		"the Full Speed-equivalent loop must execute multiple VM frames");
	Require(frames.count == total_extra_frames,
		"unpaced consecutive sound cycles must notify once per completed frame");

	std::puts("event_host_frame_integration_test: PASS");
	return 0;
}
