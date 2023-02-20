//
// eXcellent Multi-platform emulator type 8 - 'XM8'
// based on ePC-8801MA
//
// Author (ePC-8801MA) : Takeda.Toshiya
// Author (XM8) : Tanaka.Yasushi
//
// [ menu driver ]
//

#ifdef SDL

#ifndef MENU_H
#define MENU_H

//
// menu driver
//
class Menu
{
public:
	Menu(App *a);
										// constructor
	virtual ~Menu();
										// destructor
	bool Init();
										// initialize
	void Deinit();
										// deinitialize

	// control
	void EnterMenu(int menu_id);
										// enter menu
	void UpdateMenu();
										// update main menu
	void ProcessMenu();
										// process menu
	void EnterMain(int id);
										// enter main menu
	void EnterDrive1(int id);
										// enter drive1 menu
	void EnterDrive2(int id);
										// enter drive2 menu
	void EnterCmt(int id);
										// enter cmt menu
	void EnterLoad();
										// enter load menu
	void EnterSave();
										// enter save menu
	void EnterSystem(int id);
										// enter system menu
	void EnterVideo();
										// enter video menu
	void EnterAudio();
										// enter audio menu
	void EnterInput(int id);
										// enter input menu
	void EnterReset();
										// enter reset menu
	void EnterQuit();
										// enter quit menu
	void EnterSoftKey();
										// enter softkey menu
	void EnterDip();
										// enter dip menu
	int EnterDipSub();
										// enter dip menu (sub)
	void EnterJoymap(int id);
										// enter joymap menu
	void EnterVmKey(int id);
										// enter vmkey menu
	void EnterFile();
										// enter file menu
	void EnterJoyTest();
										// enter joytest menu
	void Command(bool down, int id);
										// command
	void CmdBack();
										// command (back)
	void CmdMain(int id);
										// command (main menu)
	void CmdDrive1(int id);
										// command (drive1)
	void CmdDrive2(int id);
										// command (drive2)
	void CmdCmt(int id);
										// command (cmt)
	void CmdLoad(int id);
										// command (load)
	void CmdSave(int id);
										// command (save)
	void CmdSystem(int id);
										// command (system)
	void CmdVideo(bool down, int id);
										// command (video)
	void CmdAudio(bool down, int id);
										// command (audio)
	void CmdInput(bool down, int id);
										// command (input)
	void CmdReset(int id);
										// command (reset)
	void CmdQuit(int id);
										// command (quit)
	void CmdSoftKey(int id);
										// command (softkey)
	void CmdDip(int id);
										// command (dip)
	void CmdJoymap(int id);
										// command (joymap)
	void CmdVmKey(int id);
										// command (vmkey)
	void CmdFile(int id);
										// command (file)
	void JoyTest();
										// joystick test
	void Draw();
										// draw menu

	// event
	void OnKeyDown(SDL_Event *e);
										// key down
	void OnMouseMotion(SDL_Event *e);
										// mouse motion
	void OnMouseButtonDown(SDL_Event *e);
										// mouse button down
	void OnMouseButtonUp(SDL_Event *e);
										// mouse button up
	void OnMouseWheel(SDL_Event *e);
										// mouse wheel
	void OnJoystick();
										// joystick
	void OnFingerDown(SDL_Event *e);
										// finger down
	void OnFingerUp(SDL_Event *e);
										// finger up
	void OnFingerMotion(SDL_Event *e);
										// finger motion

private:
	void MakeExpect(const char *name);
										// make file_expect[]
	App *app;
										// app
	Platform *platform;
										// platform
	Setting *setting;
										// setting
	Video *video;
										// video
	Input *input;
										// input
	MenuList *list;
										// curret menu list
	DiskManager **diskmgr;
										// disk manager
	TapeManager *tapemgr;
										// tape manager
	Converter *converter;
										// converter
	int top_id;
										// top menu id
	char file_dir[_MAX_PATH * 3];
										// file select directory
	char file_target[_MAX_PATH * 3];
										// file select target
	char file_expect[_MAX_PATH * 3];
										// file select expect name
	int file_id;
										// parent file select id
	int softkey_id;
										// parent softkey type id
	int joymap_id;
										// parent joymap id
	static const int vmkey_table[62 * 2];
										// MENU_VMKEY table
	static const Uint32 joytest_table[15 * 2];
										// MENU_JOYTEST table
	static const char *joytest_name[15];
										// MENU_JOYTEST name table
};

#endif // MENU_H

#endif // SDL
