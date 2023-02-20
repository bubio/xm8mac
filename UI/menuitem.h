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

#ifndef MENUITEM_H
#define MENUITEM_H

//
// defines
//
#define MENUITEM_WIDTH			480
										// menu width
#define MENUITEM_HEIGHT			40
										// menu height (per item)
#define MENUITEM_LINES			8
										// menu lines (title included)
#define MENUITEM_FORE			RGB_COLOR(255, 255, 255)
										// menu foreground color
#define MENUITEM_BACK			RGB_COLOR(47, 47, 47)
										// menu background color
#define MENUITEM_SLIDER_FORE	RGB_COLOR(160, 32, 128)
										// menu foreground color(slider)
#define MENUITEM_SLIDER_BACK	RGB_COLOR(64, 160, 80)
										// menu foreground color(slider)
#define MENUITEM_TITLE			RGB_COLOR(0, 0, 127)
										// menu foreground color (title)

//
// menu item
//
class MenuItem
{
public:
	enum MenuItemType {
		NotItem,
										// not initialized
		ButtonItem,
										// push button
		RadioItem,
										// radio button
		CheckItem,
										// check box
		SliderItem
										// slider
	};

public:
	MenuItem(App *a);
										// constructor
	virtual ~MenuItem();
										// destructor
	bool Init(int index, MenuItemType type, const char *name, int id, int group);
										// initialize
	void Deinit();
										// deinitialize

	// linked list
	void Add(MenuItem *item);
										// add next item
	void Del(MenuItem *item);
										// delete next item
	void SetNext(MenuItem *item);
										// set next item
	MenuItem* GetNext();
										// get next item
	void SetIndex(int index);
										// set menu index

	// action
	void Draw(SDL_Rect *rect, int top, int lines, int focus, bool file);
										// draw item
	void SetFocus();
										// set focus
	MenuItemType GetType();
										// get type
	int GetID();
										// get id
	int GetGroup();
										// get group
	const char* GetName();
										// get name
	void SetName(const char *name);
										// set name
	Uint32 GetUser();
										// get user data
	void SetUser(Uint32 user);
										// set user data
	bool GetRadio();
										// get radio state
	void SetRadio(bool radio);
										// set radio state
	bool GetCheck();
										// get check state
	void SetCheck(bool check);
										// set check state
	void SetMinMax(int minimum, int maximum, int unit);
										// set slider min and max
	int GetSlider();
										// get slider state
	void SetSlider(int current);
										// set slider state
	void SetSlider(int pos, int maxpos);
										// set slider state (mouse/touch)
	void UpDownSlider(bool up);
										// up/down slider

private:
	void DrawText(SDL_Rect *rect, Uint32 fore);
										// draw item text
	void DrawFile(SDL_Rect *rect, Uint32 fore);
										// draw item text (file)
	App *app;
										// application
	Setting *setting;
										// setting
	Font *font;
										// font
	Uint32 *frame_buf;
										// frame buffer
	MenuItemType item_type;
										// type
	MenuItem *item_next;
										// next item
	char *item_name;
										// item name
	int item_index;
										// item index
	int item_id;
										// item id
	int item_group;
										// group id
	bool item_check;
										// item check
	int item_min;
										// item minimum
	int item_max;
										// item maximum
	int item_unit;
										// item unit
	int item_cur;
										// item current
	Uint32 item_user;
										// item user data
	Uint32 tick_focus;
										// tick at SetFocus()
};

#endif // MENUITEM_H

#endif // SDL
