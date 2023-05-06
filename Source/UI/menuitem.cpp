//
// eXcellent Multi-platform emulator type 8 - 'XM8'
// based on ePC-8801MA
//
// Author (ePC-8801MA) : Takeda.Toshiya
// Author (XM8) : Tanaka.Yasushi
//
// [ menu item ]
//

#ifdef SDL

#include "os.h"
#include "common.h"
#include "vm.h"
#include "app.h"
#include "setting.h"
#include "video.h"
#include "font.h"
#include "menuitem.h"

//
// defines
//
#define BLINK_MASK				0x0200
										// blink mask (ms)

//
// MenuItem()
// constructor
//
MenuItem::MenuItem(App *a)
{
	// save parameter
	app = a;

	// object
	setting = NULL;
	font = NULL;
	frame_buf = NULL;

	// property
	item_type = NotItem;
	item_next = NULL;
	item_name = NULL;
	item_index = -1;
	item_id = -1;
	item_group = -1;
	item_check = false;
	item_min = 0;
	item_max = 0;
	item_unit = 1;
	item_cur = 0;
	item_user = 0;
	tick_focus = 0;
}

//
// ~MenuItem()
// destructor
//
MenuItem::~MenuItem()
{
	Deinit();
}

//
// Init()
// initialize
//
bool MenuItem::Init(int index, MenuItemType type, const char *name, int id, int group)
{
	Video *video;

	SDL_assert(type != NotItem);
	SDL_assert(name != NULL);
	SDL_assert(id >= 0);

	// get object and frame buffer
	setting = app->GetSetting();
	font = app->GetFont();
	video = app->GetVideo();
	frame_buf = video->GetMenuFrame();

	item_name = (char*)SDL_malloc(strlen(name) + 1);
	if (item_name == NULL) {
		return false;
	}
	strcpy(item_name, name);

	item_type = type;
	item_next = NULL;
	item_index = index;
	item_id = id;
	item_group = group;
	item_check = false;
	item_min = 0;
	item_max = 0;
	item_unit = 1;
	item_cur = 0;
	item_user = 0;

	return true;
}

//
// Deinit()
// deinitialize
//
void MenuItem::Deinit()
{
	if (item_name != NULL) {
		SDL_free(item_name);
		item_name = NULL;
	}
}

//
// Add()
// add next item
//
void MenuItem::Add(MenuItem *item)
{
	SDL_assert(item_next == NULL);
	item_next = item;
}

//
// Del()
// delete next item
//
void MenuItem::Del(MenuItem *item)
{
	SDL_assert(item_next == item);

	if (item_next == item) {
		item_next = NULL;
	}
}

//
// SetNext()
// set next item (for qsort)
//
void MenuItem::SetNext(MenuItem *item)
{
	item_next = item;
}

//
// GetNext()
// get next item
//
MenuItem* MenuItem::GetNext()
{
	return item_next;
}

//
// SetIndex()
// set menu index
//
void MenuItem::SetIndex(int index)
{
	item_index = index;
}

//
// SetFocus()
// set focus
//
void MenuItem::SetFocus()
{
	tick_focus = SDL_GetTicks();
}

//
// GetType()
// get type
//
MenuItem::MenuItemType MenuItem::GetType()
{
	return item_type;
}

//
// GetID()
// get id
//
int MenuItem::GetID()
{
	return item_id;
}

//
// GetGroup()
// get group
//
int MenuItem::GetGroup()
{
	return item_group;
}

//
// GetName()
// get name
//
const char* MenuItem::GetName()
{
	return item_name;
}

//
// SetName()
// set name
//
void MenuItem::SetName(const char *name)
{
	char *ptr;

	// compare
	if (strcmp(name, item_name) == 0) {
		return;
	}

	// allocate new area
	ptr = (char*)SDL_malloc(strlen(name) + 1);
	if (ptr != NULL) {
		// free current
		SDL_free(item_name);
		item_name = NULL;

		// copy
		strcpy(ptr, name);

		// set current
		item_name = ptr;
	}
}

//
// GetUser()
// get user data
//
Uint32 MenuItem::GetUser()
{
	return item_user;
}

//
// SetUser()
// set user data
//
void MenuItem::SetUser(Uint32 user)
{
	item_user = user;
}

//
// GetRadio()
// get radio satte
//
bool MenuItem::GetRadio()
{
	return item_check;
}

//
// SetRadio()
// set radio state
//
void MenuItem::SetRadio(bool radio)
{
	item_check = radio;
}

//
// GetCheck()
// get check satte
//
bool MenuItem::GetCheck()
{
	return item_check;
}

//
// SetCheck()
// set check state
//
void MenuItem::SetCheck(bool check)
{
	item_check = check;
}

//
// SetMinMax()
// set slider minimum and maximum
//
void MenuItem::SetMinMax(int minimum, int maximum, int unit)
{
	item_min = minimum;
	item_max = maximum;
	item_unit = unit;
}

//
// GetSlider()
// get slider state
//
int MenuItem::GetSlider()
{
	return item_cur;
}

//
// SetSlider()
// set slider state
//
void MenuItem::SetSlider(int current)
{
	if ((current >= item_min) && (current <= item_max)) {
		item_cur = current;
	}
	if (current < item_min) {
		item_cur = item_min;
	}
	if (item_max < current) {
		item_cur = item_max;
	}
}

//
// SetSlider()
// set slide state (mouse/touch)
//
void MenuItem::SetSlider(int pos, int maxpos)
{
	if (item_min < item_max) {
		// ratio
		item_cur = pos * (item_max - item_min);
		item_cur /= maxpos;
		item_cur += item_min;

		// unit
		if ((item_cur % item_unit) > (item_unit / 2)) {
			item_cur += item_unit;
		}
		item_cur -= (item_cur % item_unit);

		// min, max
		if (item_cur < item_min) {
			item_cur = item_min;
		}
		if (item_max < item_cur) {
			item_cur = item_max;
		}
	}
}

//
// UpDownSlider()
// up/down slider
//
void MenuItem::UpDownSlider(bool up)
{
	if (up) {
		item_cur += item_unit;
		if (item_cur > item_max) {
			item_cur = item_max;
		}
	}
	else {
		item_cur -= item_unit;
		if (item_cur < item_min) {
			item_cur = item_min;
		}
	}
}

//
// Draw()
// draw item
//
void MenuItem::Draw(SDL_Rect *rect, int top, int lines, int focus, bool file)
{
	bool reverse;
	Uint32 alpha;
	Uint32 fore;
	Uint32 back;
	Uint32 diff;
	SDL_Rect draw_rect;
	SDL_Rect slider_rect;
	unsigned char arrow[3];

	// over check
	if ((top + (MENUITEM_LINES - 1)) <= item_index) {
		return;
	}
	if (item_index < top) {
		return;
	}

	// get color
	alpha = (Uint32)setting->GetMenuAlpha();
	alpha <<= 24;
	fore = MENUITEM_FORE | alpha;
	back = MENUITEM_BACK | alpha;

	// get draw rect
	draw_rect.x = rect->x;
	draw_rect.y = rect->y + (MENUITEM_HEIGHT * (item_index - top + 1));
	draw_rect.w = rect->w;
	draw_rect.h = MENUITEM_HEIGHT;

	// fill(outside)
	font->DrawFillRect(frame_buf, &draw_rect, fore);

	// fill(inside)
	reverse = false;
	if (focus == item_index) {
		// focus (blink)
		diff = SDL_GetTicks() - tick_focus;
		if ((diff & BLINK_MASK) == 0) {
			reverse = true;
		}
	}
	draw_rect.x++;
	draw_rect.y++;
	draw_rect.w -= 2;
	draw_rect.h -= 2;
	if (reverse == true) {
		fore = MENUITEM_BACK | alpha;
		back = MENUITEM_FORE | alpha;
	}
	font->DrawFillRect(frame_buf, &draw_rect, back);

	// fill (slider)
	if ((item_type == SliderItem) && (item_min < item_max)) {
		slider_rect = draw_rect;
		slider_rect.w = (draw_rect.w * (item_cur - item_min)) / (item_max - item_min);
		if (focus == item_index) {
			font->DrawFillRect(frame_buf, &slider_rect, MENUITEM_SLIDER_BACK | alpha);
		}
		else {
			font->DrawFillRect(frame_buf, &slider_rect, MENUITEM_SLIDER_FORE | alpha);
		}
	}

	// text
	if (file == true) {
		DrawFile(&draw_rect, fore);
	}
	else {
		DrawText(&draw_rect, fore);
	}

	// up mark
	if (top > 0) {
		if (top == item_index) {
			arrow[0] = 0x81;
			arrow[1] = 0xaa;
			arrow[2] = 0x00;
			draw_rect.x = (draw_rect.x + draw_rect.w) - 16;
			font->DrawSjisBoldOr(frame_buf, &draw_rect, (const char*)arrow, fore);
		}
	}

	// down mark
	if (lines > (top + (MENUITEM_LINES - 1))) {
		if ((top + (MENUITEM_LINES - 2)) == item_index) {
			arrow[0] = 0x81;
			arrow[1] = 0xab;
			arrow[2] = 0x00;
			draw_rect.x = (draw_rect.x + draw_rect.w) - 16;
			font->DrawSjisBoldOr(frame_buf, &draw_rect, (const char*)arrow, fore);
		}
	}
}

//
// DrawText()
// draw item text
//
void MenuItem::DrawText(SDL_Rect *rect, Uint32 fore)
{
	SDL_Rect text_rect;
	unsigned char mark[16];
	size_t len;

	// copy
	text_rect = *rect;

	// type
	switch (item_type) {
	// no item
	case NotItem:
		return;

	// push button
	case ButtonItem:
		break;

	// radio button
	case RadioItem:
		if (item_check == true) {
			mark[0] = 0x81;
			mark[1] = 0x9c;
			mark[2] = 0x00;
		}
		else {
			mark[0] = 0x81;
			mark[1] = 0x9b;
			mark[2] = 0x00;
		}
		font->DrawSjisBoldOr(frame_buf, &text_rect, (const char*)mark, fore);
		break;

	// check box
	case CheckItem:
		if (item_check == true) {
			mark[0] = 0x81;
			mark[1] = 0xa1;
			mark[2] = 0x00;
		}
		else {
			mark[0] = 0x81;
			mark[1] = 0xa0;
			mark[2] = 0x00;
		}
		font->DrawSjisBoldOr(frame_buf, &text_rect, (const char*)mark, fore);
		break;

	// slider
	case SliderItem:
		break;
	}

	// main text
	text_rect.x += 24;
	text_rect.w -= 24;
	font->DrawSjisBoldOr(frame_buf, &text_rect, item_name, fore);

	// slider text
	if (item_type == SliderItem) {
		sprintf((char*)&mark[2], "%d", item_cur);
		len = strlen((char*)&mark[2]);

		// <- arrow
		if (item_min < item_cur) {
			mark[0] = 0x81;
			mark[1] = 0xa9;
		}
		else {
			mark[0] = 0x81;
			mark[1] = 0x40;
		}

		// -> arrow
		if (item_cur < item_max) {
			mark[len + 2] = 0x81;
			mark[len + 3] = 0xa8;
			mark[len + 4] = 0x00;
		}
		else {
			mark[len + 2] = 0x81;
			mark[len + 3] = 0x40;
			mark[len + 4] = 0x00;
		}

		text_rect.x = (int)(text_rect.x + text_rect.w - (len + 3) * 16);
		text_rect.w = (int)(len + 2) * 16;
		font->DrawSjisBoldOr(frame_buf, &text_rect, (char*)mark, fore);
	}
}

//
// DrawFile()
// draw item text (file)
//
void MenuItem::DrawFile(SDL_Rect *rect, Uint32 fore)
{
	SDL_Rect text_rect;

	// copy
	text_rect = *rect;

	// for arrow
	text_rect.x += 4;
	text_rect.w -= (16 + 4);

	font->DrawSjisLeftOr(frame_buf, &text_rect, item_name, fore);
}

#endif // SDL
