//
// eXcellent Multi-platform emulator type 8 - 'XM8'
// based on ePC-8801MA
//
// Author (ePC-8801MA) : Takeda.Toshiya
// Author (XM8) : Tanaka.Yasushi
//
// [ software keyboard ]
//

#ifdef SDL

#include "os.h"
#include "common.h"
#include "vm.h"
#include "app.h"
#include "setting.h"
#include "video.h"
#include "font.h"
#include "input.h"
#include "menuid.h"
#include "softkey.h"

//
// defines
//
#define SOFTKEY_FORECOLOR		RGB_COLOR(0xff, 0xff, 0xff)
										// foreground color
#define SOFTKEY_INSIDECOLOR		RGB_COLOR(0x60, 0x60, 0x60)
										// inside color
#define SOFTKEY_MAKECOLOR		RGB_COLOR(0xff, 0x00, 0x00)
										// make color

//
// SoftKey()
// constructor
//
SoftKey::SoftKey(App *a)
{
	// save parameter
	app = a;

	// object
	setting = NULL;
	font = NULL;
	input = NULL;
	frame_buf = NULL;

	// property
	key_rect.x = 0;
	key_rect.y = 0;
	key_rect.w = 0;
	key_rect.h = 0;
	key_multi = 0;
	key_name = NULL;
	key_action = 0;
	key_press = false;
	key_finger = -1;
	key_state = Normal;
	key_draw = Uninit;

	// bi-directional linked list
	key_next = NULL;
	key_prev = NULL;
}

//
// ~SoftKey()
// destructor
//
SoftKey::~SoftKey()
{
	Deinit();
}

//
// Init()
// initialize
//
bool SoftKey::Init(const softkey_param *param)
{
	Video *video;

	// get object
	setting = app->GetSetting();
	font = app->GetFont();
	input = app->GetInput();

	// save property
	key_rect.x = (param->left * 8);
	key_rect.y = (param->top * 8);
	key_rect.w = (param->width * 8);
	key_rect.h = (param->height * 8);
	key_multi = param->multi;
	key_name = param->name;
	key_action = param->action;

	// not pressed
	key_press = false;
	key_finger = -1;

	// get frame buffer
	video = app->GetVideo();
	frame_buf = video->GetSoftKeyFrame();

	return true;
}

//
// Deinit()
// deinitialize
//
void SoftKey::Deinit()
{
}

//
// Add()
// add new instance
//
void SoftKey::Add(SoftKey *key)
{
	SoftKey *next;

	// get next instance
	next = GetNext();

	// this instance is last ?
	if (next == NULL) {
		// last
		SetNext(key);
		key->SetPrev(this);
		key->SetNext(NULL);
	}
	else {
		// mid
		next = GetNext();

		// insert
		SetNext(key);
		key->SetPrev(this);
		key->SetNext(next);
		next->SetPrev(key);
	}
}

//
// Del()
// delete this instance
//
void SoftKey::Del()
{
	SoftKey *prev;
	SoftKey *next;

	// if presed, break key
	if (key_press == true) {
		BreakKey(true);
		key_press = false;
	}

	// get prev and next
	prev = GetPrev();
	next = GetNext();

	// prev
	if (prev != NULL) {
		prev->SetNext(next);
	}

	// next
	if (next != NULL) {
		next->SetPrev(prev);
	}
}

//
// Reset()
// reset draw state
//
void SoftKey::Reset()
{
	key_draw = Uninit;
}

//
// Draw()
// draw softkey
//
bool SoftKey::Draw()
{
	Uint32 fore;
	Uint32 inside;
	Uint32 alpha;
	SDL_Rect text_rect;

	// get key_state
	key_state = Normal;
	if (key_press == true) {
		key_state = Press;
	}
	else {
		if (key_action >= 0x100) {
			// 2+4, 6+8 etc.
			if (input->GetKeyStatus((SDL_Scancode)(key_action >> 8)) == true) {
				if (input->GetKeyStatus((SDL_Scancode)(key_action & 0xff)) == true) {
					key_state = Make;
				}
			}
		}
		else {
			// others
			if (input->GetKeyStatus((SDL_Scancode)key_action) == true) {
				key_state = Make;
			}
		}
	}

	// compare state
	if (key_state == key_draw) {
		return false;
	}

	// get alpha level
	alpha = (Uint32)setting->GetSoftKeyAlpha();
	alpha <<= 24;

	// make foreground color
	fore = alpha | SOFTKEY_FORECOLOR;

	// rect
	if (key_state == Press) {
		// rect (fore == inside)
		font->DrawSoftKeyRect(frame_buf, &key_rect, fore, fore);

		// inverse
		fore &= 0xff000000;

		// shift area
		text_rect.x = key_rect.x - 2;
		text_rect.y = key_rect.y - 2;
		text_rect.w = key_rect.w + 4;
		text_rect.h = key_rect.h + 4;
	}
	else {
		// make inside color
		inside = alpha | SOFTKEY_INSIDECOLOR;

		// normal area
		text_rect = key_rect;

		// rect (fore != inside)
		font->DrawSoftKeyRect(frame_buf, &key_rect, fore, inside);
	}

	// make ?
	if ((key_state == Make) || (key_state == Press)) {
		fore = alpha | SOFTKEY_MAKECOLOR;
	}

	// text
	if (key_multi == 1) {
		font->DrawSoftKey1x(frame_buf, &text_rect, key_name, fore);
	}
	else {
		font->DrawSoftKey4x(frame_buf, &text_rect, key_name, fore);
	}
	
	// update state
	key_draw = key_state;

	return true;
}

//
// Shift
// set shift state
//
void SoftKey::Shift(Uint32 shift)
{
	const key_shift *ptr;

	// initialize
	ptr = SoftKey::shift_table;

	// compare scan code
	for (;;) {
		if (ptr->code == SDL_SCANCODE_UNKNOWN) {
			return;
		}

		if (ptr->code == key_action) {
			break;
		}

		// next
		ptr++;
	}

	// redraw next time
	key_draw = Uninit;

	if ((shift & KANA_BIT) != 0) {
		if ((shift & SHIFT_BIT) != 0) {
			key_name = ptr->skana;
		}
		else {
			key_name = ptr->kana;
		}
		return;
	}

	if ((shift & CAPS_BIT) != 0) {
		if ((shift & SHIFT_BIT) != 0) {
			key_name = ptr->scaps;
		}
		else {
			key_name = ptr->caps;
		}
		return;
	}

	if ((shift & SHIFT_BIT) != 0) {
		key_name = ptr->shift;
	}
	else {
		key_name = ptr->normal;
	}
}

//
// IsPress()
// check press
//
bool SoftKey::IsPress()
{
	return key_press;
}

//
// LostFocus()
// lost focus
//
void SoftKey::LostFocus()
{
	// break key if pressed
	if (key_press == true) {
		BreakKey(true);
		key_press = false;
	}
}

//
// OnInput()
// input event
//
void SoftKey::OnInput(int x, int y, bool button, int finger)
{
	bool focus;

	// initialize
	focus = false;

	// check x
	if ((x >= key_rect.x) && (x < (key_rect.x + key_rect.w))) {
		// check y
		if ((y >= key_rect.y) && (y < (key_rect.y + key_rect.h))) {
			focus = true;
		}
	}

	// focus ?
	if (focus == true) {
		// button state
		if (button == true) {
			if (key_press == false) {
				// break another key pressed by same finger
				input->OnInputMove(this, key_finger);

				// make key
				MakeKey();
				key_press = true;
				key_finger = finger;
			}
		}
		else {
			if (key_press == true) {
				// break key
				BreakKey(false);
				key_press = false;
			}
		}
	}
	else {
		// break key due to lost focus
		if (key_press == true) {
			if (key_finger == finger) {
				BreakKey(true);
				key_press = false;
			}
		}
	}
}

//
// OnInputMove()
// move event
//
void SoftKey::OnInputMove(int finger)
{
	// press ?
	if (key_press == true) {
		// same finger ?
		if (key_finger == finger) {
			// auto break
			BreakKey(true);
			key_press = false;
		}
	}
}

//
// MakeKey()
// make key
//
void SoftKey::MakeKey()
{
	if (key_action >= 0x100) {
		// 2+4, 6+8 etc.
		input->OnKeyDown(true, (SDL_Scancode)(key_action >> 8));
	}
	else {
		// MENU, NEXT etc.
		if (key_action < (int)SDL_SCANCODE_A) {
			return;
		}
	}

	// others
	input->OnKeyDown(true, (SDL_Scancode)(key_action & 0xff));
}

//
// BreakKey()
// break key
//
void SoftKey::BreakKey(bool lost)
{
	// MENU, NEXT etc.
	if (lost == false) {
		if (key_action < (int)SDL_SCANCODE_A) {
			switch (key_action) {
			case 0:
				app->EnterMenu(MENU_MAIN);
				break;
			case 1:
				input->NextList();
				break;
			default:
				break;
			}
			return;
		}
	}

	if (key_action >= 0x100) {
		// 2+4, 6+8 etc.
		input->OnKeyUp(true, (SDL_Scancode)(key_action >> 8));
	}

	// others
	input->OnKeyUp(true, (SDL_Scancode)(key_action & 0xff));
}

//
// GetPrev()
// get previous instance
//
SoftKey* SoftKey::GetPrev()
{
	return key_prev;
}

//
// SetPrev()
// set previous instance
//
void SoftKey::SetPrev(SoftKey *key)
{
	key_prev = key;
}

//
// GetNext()
// get next instance
//
SoftKey* SoftKey::GetNext()
{
	return key_next;
}

//
// SetNext()
// set next instance
//
void SoftKey::SetNext(SoftKey *key)
{
	key_next = key;
}

//
// shift table
//
const SoftKey::key_shift SoftKey::shift_table[48 + 1] = {
	// normal, shift, caps, shift+caps, kana, shift+kana
	{ "1", "!", "1", "!", "\xc7", "\xc7", SDL_SCANCODE_1 },
	{ "2", "\x22", "2", "\x22", "\xcc", "\xcc", SDL_SCANCODE_2 },
	{ "3", "#", "3", "#", "\xb1", "\xa7", SDL_SCANCODE_3 },
	{ "4", "$", "4", "$", "\xb3", "\xa9", SDL_SCANCODE_4 },
	{ "5", "%", "5", "%", "\xb4", "\xaa", SDL_SCANCODE_5 },
	{ "6", "&", "6", "&", "\xb5", "\xab", SDL_SCANCODE_6 },
	{ "7", "'", "7", "'", "\xd4", "\xac", SDL_SCANCODE_7 },
	{ "8", "(", "8", "(", "\xd5", "\xad", SDL_SCANCODE_8 },
	{ "9", ")", "9", ")", "\xd6", "\xae", SDL_SCANCODE_9 },
	{ "0", "0", "0", "0", "\xdc", "\xa6", SDL_SCANCODE_0 },
	{ "-", "=", "-", "=", "\xce", "\xce", SDL_SCANCODE_MINUS },
	{ "^", "~", "^", "~", "\xcd", "\xcd", SDL_SCANCODE_EQUALS },
	{ "\\", "|", "\\", "|", "\xb0", "\xb0", SDL_SCANCODE_GRAVE },

	{ "q", "Q", "Q", "q", "\xc0", "\xc0", SDL_SCANCODE_Q },
	{ "w", "W", "W", "w", "\xc3", "\xc3", SDL_SCANCODE_W },
	{ "e", "E", "E", "e", "\xb2", "\xa8", SDL_SCANCODE_E },
	{ "r", "R", "R", "r", "\xbd", "\xbd", SDL_SCANCODE_R },
	{ "t", "T", "T", "t", "\xb6", "\xb6", SDL_SCANCODE_T },
	{ "y", "Y", "Y", "y", "\xdd", "\xdd", SDL_SCANCODE_Y },
	{ "u", "U", "U", "u", "\xc5", "\xc5", SDL_SCANCODE_U },
	{ "i", "I", "I", "i", "\xc6", "\xc6", SDL_SCANCODE_I },
	{ "o", "O", "O", "o", "\xd7", "\xd7", SDL_SCANCODE_O },
	{ "p", "P", "P", "p", "\xbe", "\xbe", SDL_SCANCODE_P },
	{ "@", "~", "@", "~", "\xde", "\xde", SDL_SCANCODE_LEFTBRACKET },
	{ "[", "{", "[", "{", "\xdf", "\xa2", SDL_SCANCODE_RIGHTBRACKET },

	{ "a", "A", "A", "a", "\xc1", "\xc1", SDL_SCANCODE_A },
	{ "s", "S", "S", "s", "\xc4", "\xc4", SDL_SCANCODE_S },
	{ "d", "D", "D", "d", "\xbc", "\xbc", SDL_SCANCODE_D },
	{ "f", "F", "F", "f", "\xca", "\xca", SDL_SCANCODE_F },
	{ "g", "G", "G", "g", "\xb7", "\xb7", SDL_SCANCODE_G },
	{ "h", "H", "H", "h", "\xb8", "\xb8", SDL_SCANCODE_H },
	{ "j", "J", "J", "j", "\xcf", "\xcf", SDL_SCANCODE_J },
	{ "k", "K", "K", "k", "\xc9", "\xc9", SDL_SCANCODE_K },
	{ "l", "L", "L", "l", "\xd8", "\xd8", SDL_SCANCODE_L },
	{ ";", "+", ";", "+", "\xda", "\xda", SDL_SCANCODE_SEMICOLON },
	{ ":", "*", ":", "*", "\xb9", "\xb9", SDL_SCANCODE_APOSTROPHE },
	{ "]", "}", "]", "}", "\xd1", "\xa3", SDL_SCANCODE_BACKSLASH },

	{ "z", "Z", "Z", "z", "\xc2", "\xaf", SDL_SCANCODE_Z },
	{ "x", "X", "X", "x", "\xbb", "\xbb", SDL_SCANCODE_X },
	{ "c", "C", "C", "c", "\xbf", "\xbf", SDL_SCANCODE_C },
	{ "v", "V", "V", "v", "\xcb", "\xcb", SDL_SCANCODE_V },
	{ "b", "B", "B", "b", "\xba", "\xba", SDL_SCANCODE_B },
	{ "n", "N", "N", "n", "\xd0", "\xd0", SDL_SCANCODE_N },
	{ "m", "M", "M", "m", "\xd3", "\xd3", SDL_SCANCODE_M },
	{ ",", "<", ",", "<", "\xc8", "\xa4", SDL_SCANCODE_COMMA },
	{ ".", ">", ".", ">", "\xd9", "\xa1", SDL_SCANCODE_PERIOD },
	{ "/", "?", "/", "?", "\xd2", "\xa5", SDL_SCANCODE_SLASH },
	{ " ", "_", " ", "_", "\xdb", " ", SDL_SCANCODE_NONUSHASH },

	{ "", "", "", "", "", "", SDL_SCANCODE_UNKNOWN }
};

#endif // SDL
