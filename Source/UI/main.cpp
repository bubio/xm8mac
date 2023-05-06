//
// eXcellent Multi-platform emulator type 8 - 'XM8'
// based on ePC-8801MA
//
// Author (ePC-8801MA) : Takeda.Toshiya
// Author (XM8) : Tanaka.Yasushi
//
// [ main function ]
//

#ifdef SDL

#include "os.h"
#include "common.h"
#include "app.h"

//
// main()
// program entry point
//
int main(int argc, char *argv[])
{
	int ret;
	App *app;

#if SDL_VERSION_ATLEAST(2, 0, 4)
	SDL_SetHint(SDL_HINT_IME_INTERNAL_EDITING, "1");
#endif
#if SDL_VERSION_ATLEAST(2, 0, 10)
	SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
#endif
#if SDL_VERSION_ATLEAST(2, 0, 2)
	SDL_SetHint(SDL_HINT_ACCELEROMETER_AS_JOYSTICK, "0");
#endif
#if SDL_VERSION_ATLEAST(2, 0, 12)
	SDL_SetHint(SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS, "0");
#endif

	SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");

		// initialize SDL
	ret = SDL_Init( 
		SDL_INIT_VIDEO |
		SDL_INIT_AUDIO |
		SDL_INIT_TIMER |
		SDL_INIT_EVENTS
	);

	if (ret != 0) {
		fprintf(stderr, "XM8: SDL_Init() failed\n");
		return 1;
	}

	// initialize joystick subsystem
	ret = SDL_InitSubSystem(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) ;
	if (ret != 0) {
		fprintf(stderr, "XM8: SDL_InitSubSystem(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) failed\n");
		SDL_Quit();
		return 1;
	}

	// new
	app = new App;

	// initialize application
	if (app->Init() == true) {
		// run
		app->Run();
	}

	// deinitialize application
	app->Deinit();

	// delete
	delete app;

	// quit joystick subsystem
	SDL_QuitSubSystem(SDL_INIT_JOYSTICK);

	// quit SDL
	SDL_Quit();

	return 0;
}

#endif // SDL
