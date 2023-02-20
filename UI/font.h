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

#ifndef FONT_H
#define FONT_H

//
// font
//
class Font
{
public:
	Font(App *a);
										// constructor
	virtual ~Font();
										// destructor
	bool Init(SDL_Window *win);
										// initialize
	void Deinit();
										// deinitialize

	// rom version
	Uint8 GetROMVersion(int offset);
										// get rom version

	// rendering to frame buffer
	void DrawRect(Uint32 *buf, SDL_Rect *rect, Uint32 fore, Uint32 back);
										// draw rectangle
	void DrawFillRect(Uint32 *buf, SDL_Rect *rect, Uint32 fore);
										// draw rectangle with fill
	void DrawHalfRect(Uint32 *buf, SDL_Rect *rect, Uint32 fore, Uint32 back);
										// draw rectangle with half fill
	void DrawSjisCenterOr(Uint32 *buf, SDL_Rect *rect, const char *string, Uint32 fore);
										// draw shift-jis string with centering and or
	void DrawSjisLeftOr(Uint32 *buf, SDL_Rect *rect, const char *string, Uint32 fore);
										// draw shift-jis string with left and or
	void DrawSjisBoldOr(Uint32 *buf, SDL_Rect *rect, const char *string, Uint32 fore);
										// draw ank string with bold and or
	void DrawSoftKeyRect(Uint32 *buf, SDL_Rect *rect, Uint32 fore, Uint32 inside);
										// draw softkey rect
	void DrawSoftKey1x(Uint32 *buf, SDL_Rect *rect, const char *string, Uint32 fore);
										// draw softkey string (8x8)
	void DrawSoftKey1xSub(Uint32 *buf, const char *string, Uint32 fore);
										// draw softkey string (8x8) sub
	void DrawSoftKey4x(Uint32 *buf, SDL_Rect *rect, const char *string, Uint32 fore);
										// draw softkey string (32x32)
	void DrawAnk9xCenterOr(Uint32 *buf, SDL_Rect *rect, const char *string, Uint32 fore);
										// draw ank string (48x48) with centering and or
	void DrawAnkHalf(Uint32 *buf, const char *string, Uint32 fore, Uint32 back);
										// draw ank string (8x16)
	void DrawAnkQuarter(Uint32 *buf, char ch, Uint32 fore, Uint32 back);
										// draw ank character (8x8)

private:
	App *app;
										// application
	SDL_Window *window;
										// window
	bool CheckROM();
										// check rom files
	bool CheckSub(const char *rom, bool msgbox);
										// check one rom file
	int GetAnkAddr(Uint8 ank);
										// get offset from ank character (16x16)
	int GetAnkHalfAddr(Uint8 ank);
										// get offset from ank character (8x16)
	int GetAnkQuarterAddr(Uint8 ank);
										// get offset from ank character (8x8)
	int GetKanjiAddr(Uint16 sjis);
										// get offset from kanji character (16x16)
	Uint32 SjisToJis(Uint32 sjis);
										// shift-jis to jis
	Uint32 JisToOffset(Uint32 jis);
										// jis to kanji rom offset
	int kanji_offset[0x4000];
										// shift-jis to kanji rom offset table
	Uint8 kanji_rom[0x40000];
										// kanji rom
	Uint8 rom_version[4];
										// rom version
	char rom_message[_MAX_PATH * 3];
										// rom not found message
	static const int ank_table[0xa0];
										// ank table
};

#endif // FONT_H

#endif // SDL
