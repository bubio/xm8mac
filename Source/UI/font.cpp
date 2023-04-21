//
// eXcellent Multi-platform emulator type 8 - 'XM8'
// based on ePC-8801MA
//
// Author (ePC-8801MA) : Takeda.Toshiya
// Author (XM8) : Tanaka.Yasushi
//
// [ font manager ]
//

#ifdef SDL

#include "os.h"
#include "common.h"
#include "fileio.h"
#include "vm.h"
#include "app.h"
#include "emu_sdl.h"
#include "platform.h"
#include "font.h"

//
// defines
//
#define PC88_FILE				"PC88.ROM"
										// PC-8801mkIISR ROM file compatible with P88SR
#define N88_FILE				"N88.ROM"
										// N88-BASIC file
#define N80_FILE				"N80.ROM"
										// N-BASIC file
#define DISK_FILE				"DISK.ROM"
										// PC-80S31K file
#define N88EXT0_FILE			"N88_0.ROM"
										// 4th ROM (for V2 mode, 4 bank)
#define N88EXT1_FILE			"N88_1.ROM"
										// 4th ROM (for V2 mode, 4 bank)
#define N88EXT2_FILE			"N88_2.ROM"
										// 4th ROM (for V2 mode, 4 bank)
#define N88EXT3_FILE			"N88_3.ROM"
										// 4th ROM (for V2 mode, 4 bank)
#define KANJI1_FILE				"KANJI1.ROM"
										// kanji file (JIS level 1)
#define KANJI2_FILE				"KANJI2.ROM"
										// kanji file (JIS level 2)
#define ROM_VER_ADDR1			0x1850
										// N-BASIC ROM version address
#define ROM_VER_ADDR2			0x79d7
										// N88-BASIC ROM version address 1
#define ROM_VER_ADDR3			0x79d8
										// N88-BASIC ROM version address 2
#define ROM_VER_ADDR4			0x07ee
										// DISK ROM version address

//
// Font()
// constructor
//
Font::Font(App *a)
{
	// save application
	app = a;

	// window object
	window = NULL;

	// JIS level 1
	memset(kanji_rom, 0, sizeof(kanji_rom) / 2);

	// JIS level 2
	memset(&kanji_rom[sizeof(kanji_rom) / 2], 0xff, sizeof(kanji_rom) / 2);

	// shift-jis to kanji rom offset
	memset(kanji_offset, 0, sizeof(kanji_offset));

	// rom version
	memset(rom_version, 0, sizeof(rom_version));
}

//
// ~Font()
// destructor
//
Font::~Font()
{
	Deinit();
}

//
// Init()
// initialize
//
bool Font::Init(SDL_Window *win)
{
	FILEIO fio;
	_TCHAR *path;
	EMU_SDL *wrapper;
	Uint32 loop;

	// save window
	window = win;

	// check rom
	if (CheckROM() == false) {
		return false;
	}

	// get EMU_SDL instance
	wrapper = app->GetWrapper();

	// kanji 1
	path = wrapper->get_bios_path(KANJI1_FILE);
	if (fio.Fopen(path, FILEIO_READ_BINARY) == true) {
		fio.Fread(kanji_rom, 1, sizeof(kanji_rom) / 2);
		fio.Fclose();
	}

	// kanji 2
	path = wrapper->get_bios_path(KANJI2_FILE);
	if (fio.Fopen(path, FILEIO_READ_BINARY) == true) {
		fio.Fread(&kanji_rom[sizeof(kanji_rom) / 2], 1, sizeof(kanji_rom) / 2);
		fio.Fclose();
	}

	// shift-jis to kanji rom offst
	for (loop=0x8000; loop<0xa000; loop++) {
		kanji_offset[loop - 0x8000] = JisToOffset(SjisToJis(loop));
	}
	for (loop=0xe000; loop<0x10000; loop++) {
		kanji_offset[loop - 0xc000] = JisToOffset(SjisToJis(loop));
	}

	return true;
}

//
// Deinit()
// deinitialize
//
void Font::Deinit()
{
}

//
// CheckROM()
// check rom files
//
bool Font::CheckROM()
{
	// PC88.ROM contains N88/N80/Sub/Extend ROMs
	if (CheckSub(PC88_FILE, false) == true) {
		// save rom version
		rom_version[0] = kanji_rom[ROM_VER_ADDR1 + 0x16000];
		rom_version[1] = kanji_rom[ROM_VER_ADDR2 + 0x00000];
		rom_version[2] = kanji_rom[ROM_VER_ADDR3 + 0x00000];
		rom_version[3] = kanji_rom[ROM_VER_ADDR4 + 0x14000];

		// PC88.ROM is OK
		if (CheckSub(N80_FILE, false) == true) {
			rom_version[0] = kanji_rom[ROM_VER_ADDR1];
		}
		if (CheckSub(N88_FILE, false) == true) {
			rom_version[1] = kanji_rom[ROM_VER_ADDR2];
			rom_version[2] = kanji_rom[ROM_VER_ADDR3];
		}
		if (CheckSub(DISK_FILE, false) == true) {
			rom_version[3] = kanji_rom[ROM_VER_ADDR4];
		}
	}
	else {
		// PC88.ROM is not found
		if (CheckSub(N80_FILE, true) == false) {
			return false;
		}
		rom_version[0] = kanji_rom[ROM_VER_ADDR1];

		if (CheckSub(N88_FILE, true) == false) {
			return false;
		}
		rom_version[1] = kanji_rom[ROM_VER_ADDR2];
		rom_version[2] = kanji_rom[ROM_VER_ADDR3];

		if (CheckSub(DISK_FILE, true) == false) {
			return false;
		}
		rom_version[3] = kanji_rom[ROM_VER_ADDR4];

		if (CheckSub(N88EXT0_FILE, true) == false) {
			return false;
		}

		if (CheckSub(N88EXT1_FILE, true) == false) {
			return false;
		}

		if (CheckSub(N88EXT2_FILE, true) == false) {
			return false;
		}

		if (CheckSub(N88EXT3_FILE, true) == false) {
			return false;
		}
	}

	// KANJI1.ROM is mandatory to display menu
	if (CheckSub(KANJI1_FILE, true) == false) {
		return false;
	}

	return true;
}

//
// CheckSub()
// check one rom file
//
bool Font::CheckSub(const char *rom, bool msgbox)
{
	FILEIO fio;
	Platform *platform;
	EMU_SDL *wrapper;
	_TCHAR *path;
	Uint32 size;

	// get EMU_SDL instance
	wrapper = app->GetWrapper();

	// get path
	path = wrapper->get_bios_path((_TCHAR*)rom);

	// try to open
	if (fio.Fopen(path, FILEIO_READ_BINARY) == true) {
		// seek and get size
		fio.Fseek(0, FILEIO_SEEK_END);
		size = (Uint32)fio.Ftell();
		fio.Fseek(0, FILEIO_SEEK_SET);

		// read
		if (size <= sizeof(kanji_rom)) {
			fio.Fread(kanji_rom, 1, (uint32)size);
		}

		// open ok
		fio.Fclose();
		return true;
	}

	// msgbox
	if (msgbox == true) {
		platform = app->GetPlatform();
		sprintf(rom_message, "The ROM file is not found:\n");
		strcat(rom_message, path);
		platform->MsgBox(window, rom_message);
	}

	// not found
	return false;
}

//
// GetROMVersion()
// get rom version
//
Uint8 Font::GetROMVersion(int offset)
{
	if (offset >= (int)sizeof(rom_version)) {
		return 0;
	}

	return rom_version[offset];
}

//
// DrawRect()
// draw rectangle
//
void Font::DrawRect(Uint32 *buf, SDL_Rect *rect, Uint32 fore, Uint32 back)
{
	int x;
	int y;

	// rect.x & rect.y
	if ((rect->x != 0) || (rect->y != 0)) {
		buf += ((rect->y * SCREEN_WIDTH) + rect->x);
	}

	// y loop
	for (y=0; y<rect->h; y++) {
		if ((y == 0) || (y == (rect->h - 1))) {
			for (x=0; x<rect->w; x++) {
				*buf++ = fore;
			}
		}
		else {
			*buf++ = fore;
			for (x=0; x<(rect->w - 2); x++) {
				*buf++ = back;
			}
			*buf++ = fore;
		}
		buf += (SCREEN_WIDTH - rect->w);
	}
}

//
// DrawFillRect()
// draw rectangle with fill
//
void Font::DrawFillRect(Uint32 *buf, SDL_Rect *rect, Uint32 fore)
{
	int x;
	int y;

	// rect.x & rect.y
	if ((rect->x != 0) || (rect->y != 0)) {
		buf += ((rect->y * SCREEN_WIDTH) + rect->x);
	}

	// y loop
	for (y=0; y<rect->h; y++) {
		for (x=0; x<rect->w; x++) {
			*buf++ = fore;
		}
		buf += (SCREEN_WIDTH - rect->w);
	}
}

//
// DrawHalfRect()
// draw rectangle with half fill
//
void Font::DrawHalfRect(Uint32 *buf, SDL_Rect *rect, Uint32 fore, Uint32 back)
{
	int x;
	int y;

	// rect.x & rect.y
	if ((rect->x != 0) || (rect->y != 0)) {
		buf += ((rect->y * SCREEN_WIDTH) + rect->x);
	}

	// y loop
	for (y=0; y<rect->h; y++) {
		if ((y & 1) == 0) {
			for (x=0; x<rect->w; x++) {
				*buf++ = fore;
			}
		}
		else {
			for (x=0; x<rect->w; x++) {
				*buf++ = back;
			}
		}
		buf += (SCREEN_WIDTH - rect->w);
	}
}

//
// DrawSJisCenterOr()
// draw shift-jis string with centering and or
//
void Font::DrawSjisCenterOr(Uint32 *buf, SDL_Rect *rect, const char *string, Uint32 fore)
{
	size_t len;
	Uint16 high;
	Uint16 low;
	int offset;
	int x;
	int y;
	int w;
	Uint16 data;

	// rect.x & rect.y
	if ((rect->x != 0) || (rect->y != 0)) {
		buf += ((rect->y * SCREEN_WIDTH) + rect->x);
	}

	// centering (x)
	len = strlen(string);
	len *= 8;
	if (len == 0) {
		return;
	}
	if (len > (size_t)rect->w) {
		return;
	}
	buf += ((rect->w / 2) - (len / 2));

	// centering (y)
	buf += (SCREEN_WIDTH * ((rect->h / 2) - (16 / 2)));

	// get widhth
	w = rect->w;

	// loop
	while (*string != '\0') {
		// get font address
		high = (Uint16)*string++;
		high &= 0x00ff;
		if (((high >= 0x80) && (high < 0xa0)) || (high >= 0xe0)) {
			// shift-jis kanji
			high <<= 8;
			low = (Uint16)*string++;
			low &= 0x00ff;
			if (low == 0) {
				break;
			}
			offset = GetKanjiAddr(high | low);

			// check w
			if (w < 16) {
				break;
			}
			w -= 16;

			// y loop
			for (y=0; y<16; y++) {
				// fetch
				data = (Uint16)kanji_rom[offset++];
				data <<= 8;
				data |= (Uint16)kanji_rom[offset++];

				// x loop
				for (x=0; x<16; x++) {
					if ((data & 0x8000) != 0) {
						*buf = fore;
					}
					data <<= 1;
					buf++;
				}

				// next x
				buf += (SCREEN_WIDTH - 16);
			}

			// next y
			buf -= (SCREEN_WIDTH * 16 - 16);
		}
		else {
			// ank
			offset = GetAnkHalfAddr((Uint8)high);

			// check w
			if (w < 8) {
				break;
			}
			w -= 8;

			// y loop
			for (y=0; y<16; y++) {
				data = (Uint16)kanji_rom[offset++];

				// x loop
				for (x=0; x<8; x++) {
					if ((data & 0x80) != 0) {
						*buf = fore;
					}
					data <<= 1;
					buf++;
				}

				// next y
				buf += (SCREEN_WIDTH - 8);
			}

			// next character
			buf -= (SCREEN_WIDTH * 16 - 8);
		}
	}
}

//
// DrawSJisLeftOr()
// draw shift-jis string with left and or
//
void Font::DrawSjisLeftOr(Uint32 *buf, SDL_Rect *rect, const char *string, Uint32 fore)
{
	Uint16 high;
	Uint16 low;
	int offset;
	int x;
	int y;
	int w;
	Uint16 data;

	// rect.x & rect.y
	if ((rect->x != 0) || (rect->y != 0)) {
		buf += ((rect->y * SCREEN_WIDTH) + rect->x);
	}

	// centering (y)
	buf += (SCREEN_WIDTH * ((rect->h / 2) - (16 / 2)));

	// get width
	w = rect->w;

	// loop
	while (*string != '\0') {
		// get font address
		high = (Uint16)*string++;
		high &= 0x00ff;
		if (((high >= 0x80) && (high < 0xa0)) || (high >= 0xe0)) {
			// shift-jis kanji
			high <<= 8;
			low = (Uint16)*string++;
			low &= 0x00ff;
			if (low == 0) {
				break;
			}
			offset = GetKanjiAddr(high | low);

			// check w
			if (w < 16) {
				break;
			}
			w -= 16;

			// y loop
			for (y=0; y<16; y++) {
				// fetch
				data = (Uint16)kanji_rom[offset++];
				data <<= 8;
				data |= (Uint16)kanji_rom[offset++];

				// x loop
				for (x=0; x<16; x++) {
					if ((data & 0x8000) != 0) {
						*buf = fore;
					}
					data <<= 1;
					buf++;
				}

				// next x
				buf += (SCREEN_WIDTH - 16);
			}

			// next y
			buf -= (SCREEN_WIDTH * 16 - 16);
		}
		else {
			// ank
			offset = GetAnkHalfAddr((Uint8)high);

			// check w
			if (w < 8) {
				break;
			}
			w -= 8;

			// y loop
			for (y=0; y<16; y++) {
				data = (Uint16)kanji_rom[offset++];

				// x loop
				for (x=0; x<8; x++) {
					if ((data & 0x80) != 0) {
						*buf = fore;
					}
					data <<= 1;
					buf++;
				}

				// next y
				buf += (SCREEN_WIDTH - 8);
			}

			// next character
			buf -= (SCREEN_WIDTH * 16 - 8);
		}
	}
}

//
// DrawSjisBoldOr()
// draw shift-jis string with bold and or
//
void Font::DrawSjisBoldOr(Uint32 *buf, SDL_Rect *rect, const char *string, Uint32 fore)
{
	Uint16 high;
	Uint16 low;
	int offset;
	int x;
	int y;
	int w;
	Uint16 data;

	// rect.x & rect.y
	if ((rect->x != 0) || (rect->y != 0)) {
		buf += ((rect->y * SCREEN_WIDTH) + rect->x);
	}

	// centering (y)
	buf += (SCREEN_WIDTH * ((rect->h / 2) - (16 / 2)));

	// get width
	w = rect->w;

	// loop
	while (*string != '\0') {
		// get font address
		high = (Uint16)*string++;
		high &= 0x00ff;
		if (((high >= 0x80) && (high < 0xa0)) || (high >= 0xe0)) {
			// shift-jis kanji
			high <<= 8;
			low = (Uint16)*string++;
			low &= 0x00ff;
			if (low == 0) {
				break;
			}
			offset = GetKanjiAddr(high | low);
		}
		else {
			// ank
			offset = GetAnkAddr((Uint8)high);
		}

		// check w
		if (w < 16) {
			break;
		}
		w -= 16;

		// y loop
		for (y=0; y<16; y++) {
			// fetch and bold
			data = (Uint16)kanji_rom[offset++];
			data <<= 8;
			data |= (Uint16)kanji_rom[offset++];
			data |= (data << 1);

			// x loop
			for (x=0; x<16; x++) {
				if ((data & 0x8000) != 0) {
					*buf = fore;
				}
				data <<= 1;
				buf++;
			}

			// next y
			buf += (SCREEN_WIDTH - 16);
		}

		// next character
		buf -= (SCREEN_WIDTH * 16 - 16);
	}
}

//
// DrawSoftKeyRect()
// draw softkey rect
//
void Font::DrawSoftKeyRect(Uint32 *buf, SDL_Rect *rect, Uint32 fore, Uint32 inside)
{
	int x;
	int y;
	Uint32 color;
	SDL_Rect fill_rect;
	SDL_Rect cur_rect;

	// make fill rect (width+=4, height+=4)
	fill_rect.x = rect->x - 2;
	fill_rect.y = rect->y - 2;
	fill_rect.w = rect->w + 4;
	fill_rect.h = rect->h + 4;

	// stretch +2 dot if fore == inside
	if (fore == inside) {
		cur_rect = fill_rect;
	}
	else {
		// clear previous state
		DrawFillRect(buf, &fill_rect, 0x00000000);
		cur_rect = *rect;
	}

	// rect.x & rect.y
	if ((cur_rect.x != 0) || (cur_rect.y != 0)) {
		buf += ((cur_rect.y * SCREEN_WIDTH) + cur_rect.x);
	}

	// y loop
	for (y=0; y<cur_rect.h; y++) {

		// x loop
		for (x=0; x<cur_rect.w; x++) {
			// initialize
			color = fore;

			if (y < 2) {
				// cornor (top)
				if (x < (2 - y)) {
					color = 0;
				}
				else {
					if (x >= (cur_rect.w - (2 - y))) {
						color = 0;
					}
				}
			}
			else {
				// cornor (bottom)
				if (y >= (cur_rect.h - 2)) {
					if (x <= (y + 2 - cur_rect.h)) {
						color = 0;
					}
					else {
						if (x >= (cur_rect.w - (y + 3 - cur_rect.h))) {
							color = 0;
						}
					}
				}
				else {
					// middle
					if ((x < 2) || (x >= (cur_rect.w - 2))) {
						color = fore;
					}
					else {
						color = inside;
					}

					if (y == 2) {
						if ((x == 2) || (x == (cur_rect.w - 3))) {
							color = fore;
						}
					}

					if (y == (cur_rect.h - 3)) {
						if ((x == 2) || (x == (cur_rect.w - 3))) {
							color = fore;
						}
					}
				}
			}

			// write
			if (color != 0) {
				*buf = color;
			}

			// next x
			buf++;
		}

		// next y
		buf += (SCREEN_WIDTH - cur_rect.w);
	}
}

//
// DrawSoftKey1x()
// draw softkey string (8x8)
//
void Font::DrawSoftKey1x(Uint32 *buf, SDL_Rect *rect, const char *string, Uint32 fore)
{
	size_t len;
	const char *second;
	Uint32 *buf_line;

	// rect.x & rect.y
	if ((rect->x != 0) || (rect->y != 0)) {
		buf += ((rect->y * SCREEN_WIDTH) + rect->x);
	}

	// check lines
	second = strstr(string, "\n");
	if (second != NULL) {
		len = (size_t)(second - string);
		second++;
	}
	else {
		len = strlen(string);
	}

	// centering (first line - x)
	len *= 8;
	if (len == 0) {
		return;
	}
	if (len > (size_t)rect->w) {
		return;
	}
	buf_line = &buf[((rect->w / 2) - (len / 2))];

	// centerling (first line - y)
	if (second != NULL) {
		buf_line += (SCREEN_WIDTH * ((rect->h / 2) - (34 / 2)));
	}
	else {
		buf_line += (SCREEN_WIDTH * ((rect->h / 2) - (16 / 2)));
	}

	// first line
	DrawSoftKey1xSub(buf_line, string, fore);

	// first line only ?
	if (second == NULL) {
		return;
	}

	// centering (second line - x)
	len = strlen(second);
	len *= 8;
	if (len == 0) {
		return;
	}
	if (len > (size_t)rect->w) {
		return;
	}
	buf_line = &buf[((rect->w / 2) - (len / 2))];

	// centerling (second line - y)
	buf_line += (SCREEN_WIDTH * ((rect->h / 2) - (34 / 2)));
	buf_line += (SCREEN_WIDTH * 18);

	// second line
	DrawSoftKey1xSub(buf_line, second, fore);
}

//
// DrawSoftKey1xSub()
// draw softkey string (8x8) sub
//
void Font::DrawSoftKey1xSub(Uint32 *buf, const char *string, Uint32 fore)
{
	Uint16 high;
	Uint16 low;
	Uint16 data;
	int offset;
	int x;
	int y;

	// loop
	for (;;) {
		// fetch
		high = (Uint16)*string++;
		high &= 0x00ff;
		if ((high == (Uint16)'\0') || (high == (Uint16)'\n')) {
			break;
		}

		if (((high >= 0x80) && (high < 0xa0)) || (high >= 0xe0)) {
			// shift-jis kanji
			high <<= 8;
			low = (Uint16)*string++;
			low &= 0x00ff;
			if ((low== (Uint16)'\0') || (low == (Uint16)'\n')) {
				break;
			}
			offset = GetKanjiAddr(high | low);

			// y loop
			for (y=0; y<16; y++) {
				// fetch and bold
				data = (Uint16)kanji_rom[offset++];
				data <<= 8;
				data |= (Uint16)kanji_rom[offset++];
				data |= (data >> 1);

				// x loop
				for (x=0; x<16; x++) {
					if ((data & 0x8000) != 0) {
						*buf = fore;
					}

					data <<= 1;
					buf++;
				}

				// next y
				buf += (SCREEN_WIDTH - 16);
			}

			// next character
			buf -= (SCREEN_WIDTH * 16 - 16);
		}
		else {
			// ank
			offset = GetAnkHalfAddr((Uint8)high);

			// y loop
			for (y=0; y<16; y++) {
				// fetch and bold
				data = (Uint16)kanji_rom[offset++];
				data |= (data >> 1);

				// x loop
				for (x=0; x<8; x++) {
					if ((data & 0x80) != 0) {
						*buf = fore;
					}

					data <<= 1;
					buf++;
				}

				// next y
				buf += (SCREEN_WIDTH - 8);
			}

			// next character
			buf -= (SCREEN_WIDTH * 16 - 8);
		}
	}
}

//
// DrawSoftKey4x()
// draw softkey string (32x32)
//
void Font::DrawSoftKey4x(Uint32 *buf, SDL_Rect *rect, const char *string, Uint32 fore)
{
	size_t len;
	int offset;
	int x;
	int y;
	Uint16 data;

	// rect.x & rect.y
	if ((rect->x != 0) || (rect->y != 0)) {
		buf += ((rect->y * SCREEN_WIDTH) + rect->x);
	}

	// centering (x)
	len = strlen(string);
	len *= 32;
	if (len == 0) {
		return;
	}
	if (len > (size_t)rect->w) {
		return;
	}
	buf += ((rect->w / 2) - (len / 2));

	// centering (y)
	buf += (SCREEN_WIDTH * ((rect->h / 2) - (32 / 2)));

	// loop
	while (*string != '\0') {
		// get font address
		offset = GetAnkAddr((Uint8)*string++);

		// y loop
		for (y=0; y<16; y++) {
			// fetch and bold
			data = (Uint16)kanji_rom[offset++];
			data <<= 8;
			data |= (Uint16)kanji_rom[offset++];
			data |= (data << 1);

			// x loop
			for (x=0; x<16; x++) {
				if ((data & 0x8000) != 0) {
					buf[0] = fore;
					buf[1] = fore;
					buf[SCREEN_WIDTH + 0] = fore;
					buf[SCREEN_WIDTH + 1] = fore;
				}

				data <<= 1;
				buf += 2;
			}

			// next y
			buf += (SCREEN_WIDTH * 2 - 32);
		}

		// next character
		buf -= (SCREEN_WIDTH * 32 - 32);
	}
}

//
// DrawAnkHalf()
// draw ank string (8x16)
//
void Font::DrawAnkHalf(Uint32 *buf, const char *string, Uint32 fore, Uint32 back)
{
	int offset;
	int x;
	int y;
	Uint8 data;

	// loop
	while (*string != '\0') {
		// get font address
		offset = GetAnkHalfAddr(*string++);

		// y loop
		for (y=0; y<16; y++) {
			// fetch
			data = kanji_rom[offset++];

			// x loop
			for (x=0; x<8; x++) {
				if (data & 0x80) {
					*buf++ = fore;
				}
				else {
					*buf++ = back;
				}
				data <<= 1;
			}

			// next y
			buf += (SCREEN_WIDTH - 8);
		}

		// next character
		buf -= (SCREEN_WIDTH * 16 - 8);
	}
}

//
// DrawAnkQuarter()
// draw ank quarter character (8x8)
//
void Font::DrawAnkQuarter(Uint32 *buf, char ch, Uint32 fore, Uint32 back)
{
	int offset;
	int x;
	int y;
	Uint8 data;

	// get font address
	offset = GetAnkQuarterAddr(ch);

	// y loop
	for (y=0; y<8; y++) {
		// fetch
		data = kanji_rom[offset++];

		// x loop
		for (x=0; x<8; x++) {
			if (data & 0x80) {
				*buf++ = fore;
			}
			else {
				*buf++ = back;
			}
			data <<= 1;
		}

		// next y
		buf += (SCREEN_WIDTH - 8);
	}
}

//
// GetAnkAddr()
// get offset from ank character (16x16)
//
int Font::GetAnkAddr(Uint8 ank)
{
	if (ank < 0x20) {
		switch (ank) {
		// right
		case 0x1c:
			return 0x2940;
		// left
		case 0x1d:
			return 0x2960;
		// up
		case 0x1e:
			return 0x2980;
		// down
		case 0x1f:
			return 0x29a0;
		default:
			break;
		}

		return 0x2400;
	}

	if ((ank >= 0x20) && (ank < 0x80)) {
		return ank_table[(ank - 0x20)];
	}

	if ((ank >= 0xa0) && (ank < 0xe0)) {
		return ank_table[(ank - 0x40)];
	}

	return 0x2400;
}

//
// GetAnkHalfAddr()
// get offset from ank character (8x16)
//
int Font::GetAnkHalfAddr(Uint8 ank)
{
	return (ank << 4);
}

//
// GetAnkQuarterAddr()
// get offset from ank character (8x8)
//
int Font::GetAnkQuarterAddr(Uint8 ank)
{
	return (ank << 3) + 0x1000;
}

//
// GetKanjiAddr()
// get offset from kanji character (16x16)
//
int Font::GetKanjiAddr(Uint16 kanji)
{
	if ((kanji >= 0x8000) && (kanji < 0xa000)) {
		return kanji_offset[kanji - 0x8000];
	}
	if (kanji >= 0xe000) {
		return kanji_offset[kanji - 0xc000];
	}

	return 0;
}

//
// SJIStoJIS()
// convert from SJIS(CP932) to JIS(ISO-2022)
//
Uint32 Font::SjisToJis(Uint32 sjis)
{
	Uint8 high;
	Uint8 low;

	// high and low
	high = (Uint8)(sjis >> 8);
	low = (Uint8)sjis;

	// high
	if (high <= 0x9f) {
		high -= 0x71;
	}
	else {
		high -= 0xb1;
	}
	high <<= 1;
	high++;

	// low
	if (low >= 0x7f) {
		low--;
	}

	if (low >= 0x9e) {
		low -= 0x7d;
		high++;
	}
	else {
		low -= 0x1f;
	}

	return (Uint32)((high << 8) | low);
}

//
// JistoOffset
// convert from JIS(ISO-2022) to kanji rom offset
//
Uint32 Font::JisToOffset(Uint32 jis)
{
	Uint32 offset;

	// initialize
	offset = 0;

	// symblic
	if ((jis >= 0x2000) && (jis < 0x3000)) {
		offset |= (Uint32)((jis & 0x001f) << 5);
		offset |= (Uint32)((jis & 0x0060) << 8);
		offset |= (Uint32)((jis & 0x0700) << 2);
	}

	// kanji (JIS level 1)
	if ((jis >= 0x3000) && (jis < 0x5000)) {
		offset |= (Uint32)((jis & 0x001f) << 5);
		offset |= (Uint32)((jis & 0x0060) << 10);
		offset |= (Uint32)((jis & 0x1f00) << 2);
	}

	// kanji (JIS level 2 - pattern A)
	if ((jis >= 0x5000) && (jis < 0x7000)) {
		offset |= (Uint32)((jis & 0x001f) << 5);
		offset |= (Uint32)((jis & 0x0060) << 10);
		offset |= (Uint32)((jis & 0x0f00) << 2);
		offset |= (Uint32)((jis & 0x2000) << 1);
		offset |= (Uint32)0x20000;
	}

	// kanji (JIS level 2 - pattern B)
	if ((jis >= 0x7000) && (jis < 0x8000)) {
		offset |= (Uint32)((jis & 0x001f) << 5);
		offset |= (Uint32)((jis & 0x0060) << 8);
		offset |= (Uint32)((jis & 0x0700) << 2);
		offset |= (Uint32)0x20000;
	}

	return offset;
}

//
// ank table
//
const int Font::ank_table[0xa0] = {
	// 0x20
	0x2400, 0x2540, 0x65a0, 0x6680, 0x6600, 0x6660, 0x66a0, 0x6580,
	0x4540, 0x4560, 0x66c0, 0x4780, 0x2480, 0x47a0, 0x24a0, 0x27e0,

	// 0x30
	0x2e00, 0x2e20, 0x2e40, 0x2e60, 0x2e80, 0x2ea0, 0x2ec0, 0x2ee0,
	0x2f00, 0x2f20, 0x24e0, 0x2500, 0x4640, 0x6420, 0x4660, 0x2520,

	// 0x40
	0x66e0, 0x4c20, 0x4c40, 0x4c60, 0x4c80, 0x4ca0, 0x4cc0, 0x4ce0,
	0x4d00, 0x4d20, 0x4d40, 0x4d60, 0x4d80, 0x4da0, 0x4dc0, 0x4de0,

	// 0x50
	0x4e00, 0x4e20, 0x4e40, 0x4e60, 0x4e80, 0x4ea0, 0x4ec0, 0x4ee0,
	0x4f00, 0x4f20, 0x4f40, 0x45c0, 0x65e0, 0x45e0, 0x2600, 0x2640,

	// 0x60
	0x25c0, 0x6c20, 0x6c40, 0x6c60, 0x6c80, 0x6ca0, 0x6cc0, 0x6ce0,
	0x6d00, 0x6d20, 0x6d40, 0x6d60, 0x6d80, 0x6da0, 0x6dc0, 0x6de0,

	// 0x70
	0x6e00, 0x6e20, 0x6e40, 0x6e60, 0x6e80, 0x6ea0, 0x6ec0, 0x6ee0,
	0x6f00, 0x6f20, 0x6f40, 0x4600, 0x4460, 0x4620, 0x25e0, 0x4480,

	// 0xa0
	0x2400, 0x2460, 0x46c0, 0x46e0, 0x2440, 0x24c0, 0x7640, 0x3420,
	0x3460, 0x34a0, 0x34e0, 0x3520, 0x7460, 0x74a0, 0x74e0, 0x5460,

	// 0xb0
	0x2780, 0x3440, 0x3480, 0x34c0, 0x3500, 0x3540, 0x3560, 0x35a0,
	0x35e0, 0x3620, 0x3660, 0x36a0, 0x36e0, 0x3720, 0x3760, 0x37a0,

	// 0xc0
	0x37e0, 0x5420, 0x5480, 0x54c0, 0x5500, 0x5540, 0x5560, 0x5580,
	0x55a0, 0x55c0, 0x55e0, 0x5640, 0x56a0, 0x5700, 0x5760, 0x57c0,

	// 0xd0
	0x57e0, 0x7400, 0x7420, 0x7440, 0x7480, 0x74c0, 0x7500, 0x7520,
	0x7540, 0x7560, 0x7580, 0x75a0, 0x75e0, 0x7660, 0x2560, 0x2580
};

#endif // SDL
