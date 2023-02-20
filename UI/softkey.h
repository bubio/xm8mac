//
// eXcellent Multi-platform emulator type 8 - 'XM8'
// based on ePC-8801MA
//
// Author (ePC-8801MA) : Takeda.Toshiya
// Author (XM8) : Tanaka.Yasushi
//
// [ software keybaord ]
//

#ifdef SDL

#ifndef SOFTKEY_H
#define SOFTKEY_H

//
// defines
//
#define SHIFT_BIT				0x01
										// shift bit
#define CAPS_BIT				0x02
										// caps bit
#define KANA_BIT				0x04
										// kana bit

//
// softkey parameter
//
typedef struct _softkey_param {
	int left;
										// left (per 8dot)
	int top;
										// top (per 8dot)
	int width;
										// width (per 8dot)
	int height;
										// height (per 8dot)
	int multi;
										// multiple
	const char *name;
										// key name
	int action;
										// action code
} softkey_param;

//
// software keyboard
//
class SoftKey
{
public:
	typedef struct _key_shift {
		const char *normal;
										// normal text
		const char *shift;
										// shift text
		const char *caps;
										// caps text
		const char *scaps;
										// shift + caps text
		const char *kana;
										// kana text
		const char *skana;
										// shift + kana text
		SDL_Scancode code;
										// scan code
	} key_shift;

public:
	SoftKey(App *a);
										// constructor
	virtual ~SoftKey();
										// destructor
	bool Init(const softkey_param *param);
										// initialize
	void Deinit();
										// deinitialize
	void Add(SoftKey *key);
										// add new instance
	void Del();
										// delete this instance
	void Reset();
										// reset draw state
	void Shift(Uint32 shift);
										// shift state
	bool Draw();
										// draw softkey
	bool IsPress();
										// check press
	void LostFocus();
										// lost focus

	// event
	void OnInput(int x, int y, bool button, int finger);
										// input event
	void OnInputMove(int finger);
										// move event

	// bi-directional linked list
	SoftKey* GetPrev();
										// get previous instance
	void SetPrev(SoftKey *key);
										// set previous instance
	SoftKey* GetNext();
										// get next instance
	void SetNext(SoftKey *key);
										// set previous instance

private:
	enum DrawState {
		Uninit,
										// uninitialized
		Normal,
										// normal state
		Make,
										// make state (from keyboard or softkey)
		Press
										// press state (softkey)
	};

	void MakeKey();
										// make key
	void BreakKey(bool lost);
										// break key
	App *app;
										// application
	Setting *setting;
										// setting
	Font *font;
										// font
	Input *input;
										// input
	Uint32 *frame_buf;
										// frame buffer
	SDL_Rect key_rect;
										// rect
	int key_multi;
										// multiple (1 or 4)
	const char *key_name;
										// name
	int key_action;
										// action code
	bool key_press;
										// key pressed
	int key_finger;
										// finger id (-1:mouse)
	DrawState key_state;
										// key draw state (latest)
	DrawState key_draw;
										// key draw state (last draw)
	SoftKey *key_next;
										// next instance
	SoftKey *key_prev;
										// previous instance
	static const SoftKey::key_shift shift_table[48 + 1];
										// shift table
};

#endif // SOFTKEY_H

#endif // SDL
