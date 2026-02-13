/*
	Skelton for retropc emulator

	Author : Takeda.Toshiya
	Date   : 2017.03.08-

	[ noise player ]
*/

#include "noise.h"

#include <string.h>

#define EVENT_SAMPLE	0

#pragma pack(push, 1)
typedef struct {
	char id[4];
	uint32 size;
} wav_chunk_t;

typedef struct {
	char riff[4];
	uint32 file_size;
	char wave[4];
	wav_chunk_t fmt_chunk;
	uint16 format_id;
	uint16 channels;
	uint32 sample_rate;
	uint32 data_speed;
	uint16 block_size;
	uint16 sample_bits;
} wav_header_t;
#pragma pack(pop)

static int noise_decibel_to_volume(int decibel)
{
	// +1 equals +0.5dB (same as fmgen/common_source_project).
	const double step = 1.0592537251772889;
	double factor = 1.0;
	if(decibel >= 0) {
		for(int i = 0; i < decibel; i++) {
			factor *= step;
		}
	} else {
		for(int i = 0; i < -decibel; i++) {
			factor /= step;
		}
	}
	return (int)(1024.0 * factor + 0.5);
}

static int32 noise_apply_volume(int32 sample, int volume)
{
	return (sample * volume) / 1024;
}

void NOISE::initialize()
{
	register_id = -1;
	ptr = 0;
	sample_l = sample_r = 0;
}

void NOISE::release()
{
	if(buffer_l != NULL) {
		free(buffer_l);
		buffer_l = NULL;
	}
	if(buffer_r != NULL) {
		free(buffer_r);
		buffer_r = NULL;
	}
}

void NOISE::reset()
{
	stop();
}

void NOISE::event_callback(int event_id, int err)
{
	if(++ptr < samples) {
		get_sample();
	} else if(loop) {
		ptr = 0;
		get_sample();
	} else {
		stop();
	}
}

void NOISE::mix(int32* buffer, int cnt)
{
	if(register_id != -1 && !mute) {
		int32 val_l = noise_apply_volume(sample_l, volume_l);
		int32 val_r = noise_apply_volume(sample_r, volume_r);
		
		for(int i = 0; i < cnt; i++) {
			*buffer++ += val_l; // L
			*buffer++ += val_r; // R
		}
	}
}

void NOISE::set_volume(int ch, int decibel_l, int decibel_r)
{
	volume_l = noise_decibel_to_volume(decibel_l);
	volume_r = noise_decibel_to_volume(decibel_r);
}

bool NOISE::load_wav_file(const _TCHAR *file_name)
{
	if(samples != 0) {
		// already loaded
		return true;
	}
	FILEIO *fio = new FILEIO();
	bool result = false;

	_TCHAR path[_MAX_PATH];
	_tcscpy_s(path, array_length(path), file_name);
	if(fio->Fopen(path, FILEIO_READ_BINARY)) {
		wav_header_t header;
		wav_chunk_t chunk;
		
		fio->Fread(&header, sizeof(header), 1);
		
		if(header.format_id == 1 && (header.sample_bits == 8 || header.sample_bits == 16)) {
			if(header.fmt_chunk.size > 16) {
				fio->Fseek(header.fmt_chunk.size - 16, FILEIO_SEEK_CUR);
			}
			while(1) {
				if(fio->Fread(&chunk, sizeof(chunk), 1) != 1) {
					break;
				}
				if(strncmp(chunk.id, "data", 4) == 0) {
					break;
				}
				fio->Fseek(chunk.size, FILEIO_SEEK_CUR);
			}
			if(strncmp(chunk.id, "data", 4) == 0 && header.channels > 0 &&
			   (samples = chunk.size / header.channels) > 0) {
				if(header.sample_bits == 16) {
					samples /= 2;
				}
				sample_rate = header.sample_rate;
				
				buffer_l = (int16 *)malloc(samples * sizeof(int16));
				buffer_r = (int16 *)malloc(samples * sizeof(int16));
				
				for(int i = 0; i < samples; i++) {
					int sample_lr[2] = {0, 0};
					for(int ch = 0; ch < header.channels; ch++) {
						int16 sample = 0;
						if(header.sample_bits == 16) {
							union {
								int16 s16;
								struct {
									uint8 l, h;
								} b;
							} pair;
							pair.b.l = fio->FgetUint8();
							pair.b.h = fio->FgetUint8();
							sample = pair.s16;
						} else {
							sample = (int16)(fio->FgetUint8());
							sample = (sample - 128) * 256;
						}
						if(ch < 2) sample_lr[ch] = sample;
					}
					buffer_l[i] = sample_lr[0];
					buffer_r[i] = sample_lr[(header.channels > 1) ? 1 : 0];
				}
				result = true;
			}
		}
		fio->Fclose();
	}
	delete fio;
	
	return result;
}

void NOISE::play()
{
	if(samples > 0 && register_id == -1 && !mute) {
		register_event(this, EVENT_SAMPLE, 1000000.0 / sample_rate, true, &register_id);
		ptr = 0;
		get_sample();
	}
}

void NOISE::stop()
{
	if(samples > 0 && register_id != -1) {
		cancel_event(this, register_id);
		register_id = -1;
		sample_l = sample_r = 0;
	}
}

void NOISE::get_sample()
{
	if(buffer_l != NULL && ptr < samples) {
		sample_l = buffer_l[ptr];
	} else {
		sample_l = 0;
	}
	if(buffer_r != NULL && ptr < samples) {
		sample_r = buffer_r[ptr];
	} else {
		sample_r = 0;
	}
}

#define STATE_VERSION	1

bool NOISE::process_state(FILEIO* state_fio, bool loading)
{
	if(loading) {
		if(state_fio->FgetUint32() != STATE_VERSION) {
			return false;
		}
		if(state_fio->FgetInt32() != this_device_id) {
			return false;
		}
		register_id = state_fio->FgetInt32();
		ptr = state_fio->FgetInt32();
		sample_l = state_fio->FgetInt32();
		sample_r = state_fio->FgetInt32();
		loop = state_fio->FgetBool();
		mute = state_fio->FgetBool();
	} else {
		state_fio->FputUint32(STATE_VERSION);
		state_fio->FputInt32(this_device_id);
		state_fio->FputInt32(register_id);
		state_fio->FputInt32(ptr);
		state_fio->FputInt32(sample_l);
		state_fio->FputInt32(sample_r);
		state_fio->FputBool(loop);
		state_fio->FputBool(mute);
	}
	return true;
}

