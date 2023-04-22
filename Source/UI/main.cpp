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

	// initialize SDL
	ret = SDL_Init( SDL_INIT_VIDEO |
					SDL_INIT_AUDIO |
					SDL_INIT_TIMER |
					SDL_INIT_EVENTS);

	if (ret != 0) {
		fprintf(stderr, "XM8: SDL_Init() failed\n");
		return 1;
	}

	// set joystick hint
	SDL_SetHint(SDL_HINT_ACCELEROMETER_AS_JOYSTICK, "0");

	// initialize joystick subsystem
	ret = SDL_InitSubSystem(SDL_INIT_JOYSTICK);
	if (ret != 0) {
		fprintf(stderr, "XM8: SDL_InitSubSystem(SDL_INIT_JOYSTICK) failed\n");
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
