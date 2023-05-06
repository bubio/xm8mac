/*
	Skelton for retropc emulator

	Author : Takeda.Toshiya
	Date   : 2007.02.09 -

	[ 1bit PCM ]
*/

#include "pcm1bit.h"

void PCM1BIT::initialize()
{
	signal = false;
	on = true;
	mute = false;
	changed = 0;
	last_vol = 0;
	
	register_frame_event(this);
}

void PCM1BIT::reset()
{
	prev_clock = current_clock();
	positive_clocks = negative_clocks = 0;
}

void PCM1BIT::write_signal(int id, uint32 data, uint32 mask)
{
	if(id == SIG_PCM1BIT_SIGNAL) {
#ifdef SDL
		bool next = ((data & mask) != 0);
		if (changed ==0) {
			// off state
			positive_clocks = 0;
			negative_clocks = 0;
			prev_clock = current_clock();
			signal = false;
		}

		if (signal != next) {
			mix_sound_block();

			if(signal) {
				positive_clocks += (passed_clock(prev_clock) << 10);
			} else {
				negative_clocks += (passed_clock(prev_clock) << 10);
			}
			prev_clock = current_clock();
			// mute if signal is not changed in 2 frames
			changed = 2;
			signal = next;
		}
#else
		bool next = ((data & mask) != 0);
		if(signal != next) {
			if(signal) {
				positive_clocks += passed_clock(prev_clock);
			} else {
				negative_clocks += passed_clock(prev_clock);
			}
			prev_clock = current_clock();
			// mute if signal is not changed in 2 frames
			changed = 2;
			signal = next;
		}
#endif // SDL
	} else if(id == SIG_PCM1BIT_ON) {
		on = ((data & mask) != 0);
	} else if(id == SIG_PCM1BIT_MUTE) {
		mute = ((data & mask) != 0);
	}
}

void PCM1BIT::event_frame()
{
	if(changed) {
		changed--;
	}
}

void PCM1BIT::mix(int32* buffer, int cnt)
{
#ifdef SDL
	int64 mix_step64;
	int mix_step;
	int positive_count;
	int negative_count;

	if (on && !mute && changed) {
		int clocks = positive_clocks + negative_clocks;

		if (clocks > (20000 << 10)) {
			// CPU 8MHz:cut under 200Hz
			// CPU 4MHz:cut under 100Hz
			positive_clocks = 0;
			negative_clocks = 0;
			return;
		}

		if (clocks == 0) {
			return;
		}

		// mix_step = number of clocks per sample (x1024)
		mix_step64 = (int64)cpu_clock;
		mix_step64 <<= 10;
		mix_step64 /= get_mix_rate();
		mix_step = (int)mix_step64;

		// clear count
		positive_count = 0;
		negative_count = 0;

		for(int i = 0; i < cnt; i++) {
			if ((positive_clocks > 0) && (negative_clocks > 0)) {
				if (positive_clocks < negative_clocks) {
					*buffer++ += max_vol; // L
					*buffer++ += max_vol; // R
					positive_clocks -= mix_step;
					positive_count++;
				}
				else {
					*buffer++ += -max_vol; // L
					*buffer++ += -max_vol; // R
					negative_clocks -= mix_step;
					negative_count++;
				}
				continue;
			}

			if (positive_clocks > 0) {
				*buffer++ += max_vol; // L
				*buffer++ += max_vol; // R
				positive_clocks -= mix_step;
				positive_count++;
			}

			if (negative_clocks > 0) {
				*buffer++ += -max_vol; // L
				*buffer++ += -max_vol; // R
				negative_clocks -= mix_step;
				negative_count++;
			}
		}

		for (int i = 0; i < (cnt - positive_count - negative_count); i++) {
			if (positive_count > negative_count) {
				*buffer++ += max_vol; // L
				*buffer++ += max_vol; // R
			}
			else {
				*buffer++ += -max_vol; // L
				*buffer++ += -max_vol; // R
			}
		}
		if (positive_count > negative_count) {
			last_vol = max_vol;
		}
		else {
			last_vol = -max_vol;
		}
	} else if(last_vol > 0) {
		// suppress petite noise when go to mute
		for(int i = 0; i < cnt && last_vol > 0; i++, last_vol--) {
			*buffer++ += last_vol; // L
			*buffer++ += last_vol; // R
		}
	} else if(last_vol < 0) {
		// suppress petite noise when go to mute
		for(int i = 0; i < cnt && last_vol < 0; i++, last_vol++) {
			*buffer++ += last_vol; // L
			*buffer++ += last_vol; // R
		}
	}
#else
	if(on && !mute && changed) {
		if(signal) {
			positive_clocks += passed_clock(prev_clock);
		} else {
			negative_clocks += passed_clock(prev_clock);
		}
		int clocks = positive_clocks + negative_clocks;
		last_vol = clocks ? (max_vol * positive_clocks - max_vol * negative_clocks) / clocks : signal ? max_vol : -max_vol;
		
		for(int i = 0; i < cnt; i++) {
			*buffer++ += last_vol; // L
			*buffer++ += last_vol; // R
		}
	} else if(last_vol > 0) {
		// suppress petite noise when go to mute
		for(int i = 0; i < cnt && last_vol != 0; i++, last_vol--) {
			*buffer++ += last_vol; // L
			*buffer++ += last_vol; // R
		}
	} else if(last_vol < 0) {
		// suppress petite noise when go to mute
		for(int i = 0; i < cnt && last_vol != 0; i++, last_vol++) {
			*buffer++ += last_vol; // L
			*buffer++ += last_vol; // R
		}
	}
	prev_clock = current_clock();
	positive_clocks = negative_clocks = 0;
#endif // SDL
}

#ifdef SDL
void PCM1BIT::update_timing(int new_clocks, double new_frames_per_sec, int new_lines_per_frame)
{
	cpu_clock = new_clocks;
}
#endif // SDL

void PCM1BIT::init(int rate, int volume)
{
	max_vol = volume;
}

#define STATE_VERSION	2

void PCM1BIT::save_state(FILEIO* state_fio)
{
	state_fio->FputUint32(STATE_VERSION);
	state_fio->FputInt32(this_device_id);
	
	state_fio->FputBool(signal);
	state_fio->FputBool(on);
	state_fio->FputBool(mute);
	state_fio->FputInt32(changed);
	state_fio->FputUint32(prev_clock);
	state_fio->FputInt32(positive_clocks);
	state_fio->FputInt32(negative_clocks);
}

bool PCM1BIT::load_state(FILEIO* state_fio)
{
	if(state_fio->FgetUint32() != STATE_VERSION) {
		return false;
	}
	if(state_fio->FgetInt32() != this_device_id) {
		return false;
	}
	signal = state_fio->FgetBool();
	on = state_fio->FgetBool();
	mute = state_fio->FgetBool();
	changed = state_fio->FgetInt32();
	prev_clock = state_fio->FgetUint32();
	positive_clocks = state_fio->FgetInt32();
	negative_clocks = state_fio->FgetInt32();
	
	// post process
	last_vol = 0;
	return true;
}

