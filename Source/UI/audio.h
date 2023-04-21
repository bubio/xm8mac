//
// eXcellent Multi-platform emulator type 8 - 'XM8'
// based on ePC-8801MA
//
// Author (ePC-8801MA) : Takeda.Toshiya
// Author (XM8) : Tanaka.Yasushi
//
// [ audio driver ]
//

#ifdef SDL

#ifndef AUDIO_H
#define AUDIO_H

//
// audio driver
//
class Audio
{
public:
	typedef struct _OpenParam {
		int device;
										// device index
		int freq;
										// frequency (44100, 55467 etc)
		int samples;
										// samples for SDL_OpenAudioDevice()
		int per;
										// samples per vm->create_sound()
		int buffer;
										// buffer size (ms)
	} OpenParam;

public:
	Audio();
										// constructor
	virtual ~Audio();
										// destructor
	bool Init();
										// initialize
	void Deinit();
										// deinitialize
	bool Open(const OpenParam *param);
										// open
	void Close();
										// close
	int GetDeviceNum();
										// get number of device
	const char* GetDeviceName(int device);
										// get device name
	void Play();
										// play
	void Stop();
										// stop
	bool IsPlay();
										// check play or pause
	int GetFreeSamples();
										// get free samples
	int Write(Uint8 *stream, int len);
										// write to sample buffer
	static void CommonCallback(void *userdata, Uint8 *stream, int len);
										// callback from SDL
	void Callback(Uint8 *steram, int len);
										// callback main

private:
	void Reset();
										// reset buffer
	SDL_sem *audio_sem;
										// semaphore object
	int num_of_devices;
										// number of devices
	char *name_of_devices;
										// name of devices
	SDL_AudioDeviceID device_id;
										// opened device id
	SDL_AudioSpec device_spec;
										// opened device spec
	bool device_pause;
										// pause status (device)
	bool play_pause;
										// pause status (play-stop)
	Uint8 *sample_buffer;
										// sample buffer
	int sample_num;
										// number of samples (current)
	int sample_size;
										// size of sample buffer
	int sample_read;
										// head of sample buffer
	int sample_write;
										// tail of sample buffer
	int sample_per;
										// number of samples per Write()
};

#endif // AUDIO_H

#endif // SDL
