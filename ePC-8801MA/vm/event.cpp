/*
	Skelton for retropc emulator

	Author : Takeda.Toshiya
	Date   : 2006.11.29-

	[ event manager ]
*/

#include "event.h"
#ifdef SDL
#include "z80.h"
#endif // SDL

#define EVENT_MIX	0

#ifdef SDL
#define SINGLE_EXEC_TIMEOUT		10000
										// exit single exec mode (us)
#endif // SDL

void EVENT::initialize()
{
	// load config
	if(!(0 <= config.cpu_power && config.cpu_power <= 4)) {
		config.cpu_power = 0;
	}
	power = config.cpu_power;
	
	// initialize sound buffer
	sound_buffer = NULL;
	sound_tmp = NULL;
	
	dont_skip_frames = 0;
	prev_skip = next_skip = false;
	sound_changed = false;
}

void EVENT::initialize_sound(int rate, int samples)
{
	// initialize sound
	sound_samples = samples;
	sound_tmp_samples = samples * 2;
	sound_buffer = (uint16*)malloc(sound_samples * sizeof(uint16) * 2);
	memset(sound_buffer, 0, sound_samples * sizeof(uint16) * 2);
	sound_tmp = (int32*)malloc(sound_tmp_samples * sizeof(int32) * 2);
	memset(sound_tmp, 0, sound_tmp_samples * sizeof(int32) * 2);
	buffer_ptr = 0;
	
#ifdef SDL
	// save rate, initialize pointer
	mix_rate = rate;
	mix_ptr = 0;
#else
	// register event
	this->register_event(this, EVENT_MIX, 1000000.0 / rate, true, NULL);
#endif // SDL
}

void EVENT::release()
{
	// release sound
	if(sound_buffer) {
		free(sound_buffer);
	}
	if(sound_tmp) {
		free(sound_tmp);
	}
}

void EVENT::reset()
{
	// clear events except loop event
	for(int i = 0; i < MAX_EVENT; i++) {
#ifdef SDL
		if (event[i].active && !event[i].loop_enable) {
#else
		if(event[i].active && event[i].loop_clock == 0) {
#endif // SDL
			cancel_event(NULL, i);
		}
	}

#ifdef SDL
	// clear all remain_clock and passed_clock
	for (int i = 0; i < MAX_EVENT; i++) {
		event[i].remain_clock = event[i].loop_clock;
		event[i].passed_clock = 0;
		event[i].update_done = false;
	}

	// single exeution mode
	single_exec = false;

	// event clock and mix clock
	event_clocks = 0;
	mix_clock = 0;

	// sample multiple
	sample_multi = 0x1000;

	// sub cpu counter
	d_cpu[1].accum_clocks = 0;

	// version 1.20
	vblank_clocks = 0;
	single_exec_clock = 0;
#endif // SDL
	
	event_remain = 0;
	cpu_remain = cpu_accum = cpu_done = 0;
	
	// reset sound
	if(sound_buffer) {
		memset(sound_buffer, 0, sound_samples * sizeof(uint16) * 2);
	}
	if(sound_tmp) {
		memset(sound_tmp, 0, sound_tmp_samples * sizeof(int32) * 2);
	}
//	buffer_ptr = 0;
	
#ifdef _DEBUG_LOG
	initialize_done = true;
#endif
}

#ifdef SDL
void EVENT::request_single_exec()
{
	if (single_exec == false) {
		// enter single execution mode
		single_exec = true;

		// abort main cpu and sub cpu
		d_cpu[0].device->write_signal(SIG_CPU_FIRQ, 1, 1);
		d_cpu[1].device->write_signal(SIG_CPU_FIRQ, 1, 1);
	}

	// save (extend) clock
	single_exec_clock = current_clock();
}
#endif // SDL

void EVENT::drive()
{
#ifdef SDL
	if (single_exec == true) {
		// if passed 10ms, disable single_exec
		if (passed_usec(single_exec_clock) > SINGLE_EXEC_TIMEOUT) {
			single_exec = false;
		}
	}
#endif // SDL

	// raise pre frame events to update timing settings
	for(int i = 0; i < frame_event_count; i++) {
		frame_event[i]->event_pre_frame();
	}
	
	// generate clocks per line
	if(frames_per_sec != next_frames_per_sec || lines_per_frame != next_lines_per_frame) {
		frames_per_sec = next_frames_per_sec;
		lines_per_frame = next_lines_per_frame;
		
		int sum = (int)((double)d_cpu[0].cpu_clocks / frames_per_sec + 0.5);
		int remain = sum;
		
		for(int i = 0; i < lines_per_frame; i++) {
			vclocks[i] = (int)(sum / lines_per_frame);
			remain -= vclocks[i];
		}
		for(int i = 0; i < remain; i++) {
			int index = (int)((double)lines_per_frame * (double)i / (double)remain);
			vclocks[index]++;
		}
		for(int i = 1; i < dcount_cpu; i++) {
			d_cpu[i].update_clocks = (int)(1024.0 * (double)d_cpu[i].cpu_clocks / (double)d_cpu[0].cpu_clocks + 0.5);
		}
		for(DEVICE* device = vm->first_device; device; device = device->next_device) {
			if(device->event_manager_id() == this_device_id) {
				device->update_timing(d_cpu[0].cpu_clocks, frames_per_sec, lines_per_frame);
			}
		}

#ifdef SDL
		// main cpu clocks for v-blank
		vblank_clocks = 0;
#endif // SDL
	}
	
	// run virtual machine for 1 frame period
	for(int i = 0; i < frame_event_count; i++) {
		frame_event[i]->event_frame();
	}
#ifdef SDL
	for(int v = 0; v < lines_per_frame;) {
		int exec_lines;
		int min_event_clock;
		int main_cpu_exec;
		int sub_cpu_exec;
		int sub_cpu_done;

		// get execute lines from PC88::event_vline()
		exec_lines = vline_event[0]->event_vline(v);

		if (exec_lines == 1) {
			// V-DISP
			event_remain += vclocks[v];
			cpu_remain += vclocks[v++];
		}
		else {
			// V-BLANK
			exec_lines = lines_per_frame - v;
			if (vblank_clocks == 0) {
				for (int loop=0; loop<exec_lines; loop++) {
					vblank_clocks += vclocks[v++];
				}
			}
			else {
				v += exec_lines;
			}
			event_remain += vblank_clocks;
			cpu_remain += vblank_clocks;
		}

		while (event_remain > 0) {
			// reset regist_event_ctrl and cpu_done
			regist_event_ctrl = false;
			cpu_done = 0;

			// resume for running (clear Z80::abort)
			d_cpu[0].device->write_signal(SIG_CPU_FIRQ, 0, 0);
			d_cpu[1].device->write_signal(SIG_CPU_FIRQ, 0, 0);

			// running main cpu
			main_cpu_exec = event_remain;
			min_event_clock = (first_fire_event->remain_clock - first_fire_event->passed_clock + 0x3ff) >> 10;

			// max(event_remain, min_event_clock)
			if ((min_event_clock > 0) && (min_event_clock < event_remain)) {
				main_cpu_exec = min_event_clock;
			}

			if (main_cpu_exec > 0) {
				// single execution ?
				if (single_exec == true) {
					if (main_cpu_exec > 4) {
						main_cpu_exec = 4;
					}
				}
				cpu_done = d_cpu[0].device->run(main_cpu_exec);
			}

			// if registered new event during main cpu execution, recalc min_event_clock
			if (regist_event_ctrl == true) {
				event_remain -= cpu_done;
				continue;
			}

			// running sub cpu
			if (d_cpu[1].update_clocks == 0x400) {
				// main 4MHz
				if (cpu_remain > 0) {
					sub_cpu_exec = cpu_remain;
					if (single_exec == true) {
						if (sub_cpu_exec > 4) {
							sub_cpu_exec = 4;
						}
					}

					sub_cpu_done = d_cpu[1].device->run(sub_cpu_exec);
					cpu_remain -= sub_cpu_done;
				}
			}
			else {
				// main 8MHz
				if (cpu_remain > 2) {
					sub_cpu_exec = cpu_remain / 2;
					if (single_exec == true) {
						if (sub_cpu_exec > 4) {
							sub_cpu_exec = 4;
						}
					}
					
					sub_cpu_done = d_cpu[1].device->run(sub_cpu_exec);
					cpu_remain -= (sub_cpu_done * 2);
				}
			}

			// if registered new event during sub cpu execution, recalc min_event_clock
			if (regist_event_ctrl == true) {
				event_remain -= cpu_done;
				continue;
			}

			// update event
			if (cpu_done > 0) {
				regist_event_ctrl = true;
				update_event(cpu_done);
				event_remain -= cpu_done;
			}
		}
	}

	// mix sound at the end of frame
	mix_sound_block();
#else
	for(int v = 0; v < lines_per_frame; v++) {
		// run virtual machine per line
		for(int i = 0; i < vline_event_count; i++) {
			vline_event[i]->event_vline(v, vclocks[v]);
		}

		if(event_remain < 0) {
			if(-event_remain > vclocks[v]) {
				update_event(vclocks[v]);
			} else {
				update_event(-event_remain);
			}
		}
		event_remain += vclocks[v];
		cpu_remain += vclocks[v] << power;
		
		while(event_remain > 0) {
			int event_done = event_remain;
			if(cpu_remain > 0) {
				// run one opecode on primary cpu
				int cpu_done_tmp;
				if(dcount_cpu == 1) {
					cpu_done_tmp = d_cpu[0].device->run(-1);
				} else {
					// sync to sub cpus
					if(cpu_done == 0) {
						cpu_done = d_cpu[0].device->run(-1);
					}
					cpu_done_tmp = (cpu_done < 4) ? cpu_done : 4;
					cpu_done -= cpu_done_tmp;
					
					for(int i = 1; i < dcount_cpu; i++) {
						// run sub cpus
						d_cpu[i].accum_clocks += d_cpu[i].update_clocks * cpu_done_tmp;
						int sub_clock = d_cpu[i].accum_clocks >> 10;
						if(sub_clock) {
							d_cpu[i].accum_clocks -= sub_clock << 10;
							d_cpu[i].device->run(sub_clock);
						}
					}
				}
				cpu_remain -= cpu_done_tmp;
				cpu_accum += cpu_done_tmp;
				event_done = cpu_accum >> power;
				cpu_accum -= event_done << power;
			}
			if(event_done > 0) {
				if(event_done > event_remain) {
					update_event(event_remain);
				} else {
					update_event(event_done);
				}
				event_remain -= event_done;
			}
		}
	}
#endif // SDL
}

void EVENT::update_event(int clock)
{
#ifdef SDL
	event_t *event_handle;

	// add event_clocks
	event_clocks += clock;
	clock <<= 10;

	// add passed_clock to all valid events
	event_handle = first_fire_event;
	while (event_handle != NULL) {
		event_handle->passed_clock += (uint32)clock;
		event_handle = event_handle->next;
	}

	if (first_fire_event->passed_clock < first_fire_event->remain_clock) {
		// have remain
		return;
	}

	// event loop
	event_handle = first_fire_event;
	while (event_handle != NULL) {
		// check update_done
		if (event_handle->update_done == true) {
			event_handle = event_handle->next;
			continue;
		}

		// update_done
		event_handle->update_done = true;

		// check remain
		if (event_handle->passed_clock < event_handle->remain_clock) {
			// have remain
			event_handle = event_handle->next;
			continue;
		}

		// find first fire event
		if ((event_handle->prev != NULL) && (event_handle->prev->active == true)) {
			first_fire_event = event_handle->prev;
			while ((first_fire_event->prev != NULL) && (event_handle->prev->active == true)) {
				first_fire_event = first_fire_event->prev;
			}
		}
		else {
			first_fire_event = event_handle->next;
			if (first_fire_event != NULL) {
				first_fire_event->prev = NULL;
			}
		}

		// separate this event
		if (event_handle->prev != NULL) {
			event_handle->prev->next = event_handle->next;
		}
		if (event_handle->next != NULL) {
			event_handle->next->prev = event_handle->prev;
		}

		// loop ?
		if (event_handle->loop_enable == true) {
			// loop
			event_handle->remain_clock -= event_handle->passed_clock;
			event_handle->remain_clock += event_handle->loop_clock;
			event_handle->passed_clock = 0;

			// added callback
			while (event_handle->remain_clock > 0x80000000) {
				// callback n-1 times
				event_handle->device->event_callback(event_handle->event_id, 0);
				event_handle->remain_clock += event_handle->loop_clock;
			}

			insert_event(event_handle);

			// callback
			event_handle->device->event_callback(event_handle->event_id, 0);
		}
		else {
			// one shot
			event_handle->active = false;
			event_handle->next = first_free_event;
			event_handle->prev = NULL;
			first_free_event = event_handle;

			// callback
			event_handle->device->event_callback(event_handle->event_id, 0);
		}

		// restart loop from first_fire_event
		event_handle = first_fire_event;
	}

	// clear update_done for next update
	event_handle = first_fire_event;
	while (event_handle != NULL) {
		event_handle->update_done = false;
		event_handle = event_handle->next;
	}
#else
	uint64 event_clocks_tmp = event_clocks + clock;
	
	while(first_fire_event != NULL && first_fire_event->expired_clock <= event_clocks_tmp) {
		event_t *event_handle = first_fire_event;
		uint64 expired_clock = event_handle->expired_clock;
		
		first_fire_event = event_handle->next;
		if(first_fire_event != NULL) {
			first_fire_event->prev = NULL;
		}
		if(event_handle->loop_clock != 0) {
			event_handle->accum_clocks += event_handle->loop_clock;
			uint64 clock_tmp = event_handle->accum_clocks >> 10;
			event_handle->accum_clocks -= clock_tmp << 10;
			event_handle->expired_clock += clock_tmp;
			insert_event(event_handle);
		} else {
			event_handle->active = false;
			event_handle->next = first_free_event;
			first_free_event = event_handle;
		}
		event_clocks = expired_clock;
		event_handle->device->event_callback(event_handle->event_id, 0);
	}
	event_clocks = event_clocks_tmp;
#endif // SDL
}

uint32 EVENT::current_clock()
{
#ifdef SDL
	Z80 *z80;

	if (regist_event_ctrl == false) {
		z80 = (Z80*)d_cpu[0].device;
		return event_clocks + z80->get_current_icount();
	}
	else {
		return event_clocks;
	}
#else
	return (uint32)(event_clocks & 0xffffffff);
#endif // SDL
}

uint32 EVENT::passed_clock(uint32 prev)
{
	uint32 current = current_clock();
	return (current > prev) ? current - prev : current + (0xffffffff - prev) + 1;
}

double EVENT::passed_usec(uint32 prev)
{
	return 1000000.0 * passed_clock(prev) / d_cpu[0].cpu_clocks;
}

uint32 EVENT::get_cpu_pc(int index)
{
	return d_cpu[index].device->get_pc();
}

void EVENT::register_event(DEVICE* device, int event_id, double usec, bool loop, int* register_id)
{
#ifdef SDL
	Z80 *z80;

	if (regist_event_ctrl == false) {
		// regist during execution
		if (cpu_done == 0) {
			// running main cpu or skip
			z80 = (Z80*)d_cpu[0].device;
			if (z80->get_current_icount() > 0) {
				update_event(z80->get_current_icount());
			}

			// abort running both cpus and continue
			abort_main_cpu();
			abort_sub_cpu();
			regist_event_ctrl = true;
		}
		else {
			// running sub cpu
			update_event(cpu_done);

			// abort running sub cpu and continue
			abort_sub_cpu();
			regist_event_ctrl = true;
		}
	}
#endif // SDL

#ifdef _DEBUG_LOG
	if(!initialize_done && !loop) {
		emu->out_debug_log(_T("EVENT: non-loop event is registered before initialize is done\n"));
	}
#endif
	
	// register event
	if(first_free_event == NULL) {
#ifdef _DEBUG_LOG
		emu->out_debug_log(_T("EVENT: too many events !!!\n"));
#endif
		if(register_id != NULL) {
			*register_id = -1;
		}
		return;
	}
	event_t *event_handle = first_free_event;
	first_free_event = first_free_event->next;
	
	if(register_id != NULL) {
		*register_id = event_handle->index;
	}
	event_handle->active = true;
	event_handle->device = device;
	event_handle->event_id = event_id;
#ifdef SDL
	// unit: d_cpu[0].cpu_clocks / 1024
	usec = ((1024.0 * usec * d_cpu[0].cpu_clocks) / 1000000.0) + 0.5;
	event_handle->remain_clock = (uint32)usec;
	event_handle->loop_clock = event_handle->remain_clock;
	event_handle->passed_clock = 0;
	event_handle->loop_enable = loop;
	event_handle->update_done = false;
#else
	uint64 clock;
	if(loop) {
		event_handle->loop_clock = (uint64)(1024.0 * (double)d_cpu[0].cpu_clocks / 1000000.0 * usec + 0.5);
		event_handle->accum_clocks = event_handle->loop_clock;
		clock = event_handle->accum_clocks >> 10;
		event_handle->accum_clocks -= clock << 10;
	} else {
		clock = (uint64)((double)d_cpu[0].cpu_clocks / 1000000.0 * usec + 0.5);
		event_handle->loop_clock = 0;
		event_handle->accum_clocks = 0;
	}
	event_handle->expired_clock = event_clocks + clock;
#endif // SDL
	
	insert_event(event_handle);
}

#ifdef SDL
void EVENT::register_event_by_clock(DEVICE* device, int event_id, uint32 clock, bool loop, int* register_id)
#else
void EVENT::register_event_by_clock(DEVICE* device, int event_id, uint64 clock, bool loop, int* register_id)
#endif // SDL
{
#ifdef SDL
	Z80 *z80;

	if (regist_event_ctrl == false) {
		// regist during execution
		if (cpu_done == 0) {
			// running main cpu
			z80 = (Z80*)d_cpu[0].device;
			if (z80->get_current_icount() > 0) {
				update_event(z80->get_current_icount());
			}

			// abort running main cpu and continue
			abort_main_cpu();
			regist_event_ctrl = true;
		}
		else {
			// running sub cpu
			update_event(cpu_done);

			// abort running sub cpu and continue
			abort_sub_cpu();
			regist_event_ctrl = true;
		}
	}
#endif // SDL

#ifdef _DEBUG_LOG
	if(!initialize_done && !loop) {
		emu->out_debug_log(_T("EVENT: non-loop event is registered before initialize is done\n"));
	}
#endif
	
	// register event
	if(first_free_event == NULL) {
#ifdef _DEBUG_LOG
		emu->out_debug_log(_T("EVENT: too many events !!!\n"));
#endif
		if(register_id != NULL) {
			*register_id = -1;
		}
		return;
	}
	event_t *event_handle = first_free_event;
	first_free_event = first_free_event->next;
	
	if(register_id != NULL) {
		*register_id = event_handle->index;
	}
	event_handle->active = true;
	event_handle->device = device;
	event_handle->event_id = event_id;
#ifdef SDL
	// unit: d_cpu[0].cpu_clocks / 1024
	event_handle->remain_clock = (clock << 10);
	event_handle->loop_clock = event_handle->remain_clock;
	event_handle->passed_clock = 0;
	event_handle->loop_enable = loop;
	event_handle->update_done = false;
#else
	event_handle->expired_clock = event_clocks + clock;
	event_handle->loop_clock = loop ? (clock << 10) : 0;
	event_handle->accum_clocks = 0;
#endif // SDL
	
	insert_event(event_handle);
}

void EVENT::insert_event(event_t *event_handle)
{
	if(first_fire_event == NULL) {
		first_fire_event = event_handle;
		event_handle->prev = event_handle->next = NULL;
	} else {
		for(event_t *insert_pos = first_fire_event; insert_pos != NULL; insert_pos = insert_pos->next) {
#ifdef SDL
			if ((insert_pos->remain_clock > insert_pos->passed_clock) &&
				((insert_pos->remain_clock - insert_pos->passed_clock) > event_handle->remain_clock)) {
#else
			if(insert_pos->expired_clock > event_handle->expired_clock) {
#endif // SDL
				if(insert_pos->prev != NULL) {
					// insert
					insert_pos->prev->next = event_handle;
					event_handle->prev = insert_pos->prev;
					event_handle->next = insert_pos;
					insert_pos->prev = event_handle;
					break;
				} else {
					// add to head
					first_fire_event = event_handle;
					event_handle->prev = NULL;
					event_handle->next = insert_pos;
					insert_pos->prev = event_handle;
					break;
				}
			} else if(insert_pos->next == NULL) {
				// add to tail
				insert_pos->next = event_handle;
				event_handle->prev = insert_pos;
				event_handle->next = NULL;
				break;
			}
		}
	}
}

void EVENT::cancel_event(DEVICE* device, int register_id)
{
	// cancel registered event
	if(0 <= register_id && register_id < MAX_EVENT) {
		event_t *event_handle = &event[register_id];
		if(device != NULL && device != event_handle->device) {
			emu->out_debug_log("EVENT: event cannot be canceled by non ownew device (id=%d) !!!\n", device->this_device_id);
			return;
		}
		if(event_handle->active) {
			if(event_handle->prev != NULL) {
				event_handle->prev->next = event_handle->next;
			} else {
				first_fire_event = event_handle->next;
			}
			if(event_handle->next != NULL) {
				event_handle->next->prev = event_handle->prev;
			}
			event_handle->active = false;
			event_handle->next = first_free_event;
			first_free_event = event_handle;
#ifdef SDL
			event_handle->prev = NULL;
#endif // SDL
		}
	}
}

void EVENT::register_frame_event(DEVICE* dev)
{
	if(frame_event_count < MAX_EVENT) {
		frame_event[frame_event_count++] = dev;
	} else {
#ifdef _DEBUG_LOG
		emu->out_debug_log(_T("EVENT: too many frame events !!!\n"));
#endif
	}
}

void EVENT::register_vline_event(DEVICE* dev)
{
	if(vline_event_count < MAX_EVENT) {
		vline_event[vline_event_count++] = dev;
	} else {
#ifdef _DEBUG_LOG
		emu->out_debug_log(_T("EVENT: too many vline events !!!\n"));
#endif
	}
}

#ifdef SDL
void EVENT::adjust_event(int register_id, double usec)
{
	usec = ((1024.0 * usec * d_cpu[0].cpu_clocks) / 1000000.0) + 0.5;
	if (event[register_id].loop_clock != usec) {
		event[register_id].remain_clock = usec;
		event[register_id].loop_clock = usec;
		event[register_id].passed_clock = 0;
	}
}
#endif // SDL

void EVENT::event_callback(int event_id, int err)
{
//	if(event_id == EVENT_MIX) {
		// mix sound
		if(prev_skip && dont_skip_frames == 0 && !sound_changed) {
			buffer_ptr = 0;
		}
		if(sound_tmp_samples - buffer_ptr > 0) {
			mix_sound(1);
		}
//	}
}

#ifdef SDL
void EVENT::mix_sound_block()
{
	double samples_double;
	int samples;

	if(prev_skip && dont_skip_frames == 0 && !sound_changed) {
		// none
		buffer_ptr = 0;
		mix_ptr = 0;
	}
	else {
		// what sample does need from mix_clock ?
		samples_double = (double)passed_clock(mix_clock);
		samples_double *= (double)mix_rate;
		samples_double /= (double)d_cpu[0].cpu_clocks;

		// multiple (0x1000:std)
		samples = (int)samples_double;
		samples *= sample_multi;
		samples >>= 12;

		if ((buffer_ptr - mix_ptr) < samples) {
			// add samples
			samples -= (buffer_ptr - mix_ptr);
			if ((sound_tmp_samples - buffer_ptr) < samples) {
				samples = sound_tmp_samples - buffer_ptr;
			}
			mix_sound(samples);
		}
	}
}
#endif // SDL

void EVENT::mix_sound(int samples)
{
	if(samples > 0) {
		int32* buffer = sound_tmp + buffer_ptr * 2;
		memset(buffer, 0, samples * sizeof(int32) * 2);
		for(int i = 0; i < dcount_sound; i++) {
			d_sound[i]->mix(buffer, samples);
		}
		if(!sound_changed) {
			for(int i = 0; i < samples * 2; i += 2) {
				if(buffer[i] != sound_tmp[0] || buffer[i + 1] != sound_tmp[1]) {
					sound_changed = true;
					break;
				}
			}
		}
		buffer_ptr += samples;
	} else {
		// notify to sound devices
		for(int i = 0; i < dcount_sound; i++) {
			d_sound[i]->mix(sound_tmp + buffer_ptr * 2, 0);
		}
	}
}

uint16* EVENT::create_sound(int* extra_frames)
{
	if(prev_skip && dont_skip_frames == 0 && !sound_changed) {
		memset(sound_buffer, 0, sound_samples * sizeof(uint16) * 2);
		*extra_frames = 0;
		return sound_buffer;
	}
	int frames = 0;
	
	// drive extra frames to fill the sound buffer
	while(sound_samples > buffer_ptr) {
		drive();
		frames++;
	}
#ifdef LOW_PASS_FILTER
	// low-pass filter
	for(int i = 0; i < sound_samples - 1; i++) {
		sound_tmp[i * 2    ] = (sound_tmp[i * 2    ] + sound_tmp[i * 2 + 2]) / 2; // L
		sound_tmp[i * 2 + 1] = (sound_tmp[i * 2 + 1] + sound_tmp[i * 2 + 3]) / 2; // R
	}
#endif
	// copy to buffer
	for(int i = 0; i < sound_samples * 2; i++) {
		int dat = sound_tmp[i];
		uint16 highlow = (uint16)(dat & 0x0000ffff);
		
		if((dat > 0) && (highlow >= 0x8000)) {
			sound_buffer[i] = 0x7fff;
			continue;
		}
		if((dat < 0) && (highlow < 0x8000)) {
			sound_buffer[i] = 0x8000;
			continue;
		}
		sound_buffer[i] = highlow;
	}
	if(buffer_ptr > sound_samples) {
		buffer_ptr -= sound_samples;
		memcpy(sound_tmp, sound_tmp + sound_samples * 2, buffer_ptr * sizeof(int32) * 2);
	} else {
		buffer_ptr = 0;
	}
	*extra_frames = frames;
	return sound_buffer;
}

#ifdef SDL
int32* EVENT::create_sound32(int* extra_frames)
{
	if(prev_skip && dont_skip_frames == 0 && !sound_changed) {
		memset(sound_tmp, 0, sound_samples * 2 * sizeof(int32));
		*extra_frames = 0;
		buffer_ptr = 0;
		return sound_tmp;
	}
	int frames = 0;
	
	// drive extra frames to fill the sound buffer
	while(sound_samples >= buffer_ptr) {
		drive();
		frames++;
	}

#ifdef LOW_PASS_FILTER
	// low-pass filter
	for(int i = 0; i < sound_samples - 1; i++) {
		sound_tmp[i * 2    ] = (sound_tmp[i * 2    ] + sound_tmp[i * 2 + 2]) / 2; // L
		sound_tmp[i * 2 + 1] = (sound_tmp[i * 2 + 1] + sound_tmp[i * 2 + 3]) / 2; // R
	}
#endif

	*extra_frames = frames;
	return sound_tmp;
}

void EVENT::create_sound32_after(int samples)
{
	if (buffer_ptr > samples) {
		buffer_ptr -= samples;
		memcpy(sound_tmp, sound_tmp + samples * 2, buffer_ptr * sizeof(int32) * 2);
	}
	else {
		buffer_ptr = 0;
	}

	mix_clock = current_clock();
	mix_ptr = buffer_ptr;
}
#endif // SDL

int EVENT::sound_buffer_ptr()
{
	return buffer_ptr;
}

void EVENT::request_skip_frames()
{
	next_skip = true;
}

bool EVENT::now_skip()
{
	bool value = next_skip;
	
	if(sound_changed || (prev_skip && !next_skip)) {
		dont_skip_frames = (int)frames_per_sec;
	}
	if(dont_skip_frames > 0) {
		value = false;
		dont_skip_frames--;
	}
	prev_skip = next_skip;
	next_skip = false;
	sound_changed = false;
	
	return value;
}

void EVENT::update_config()
{
	if(power != config.cpu_power) {
		power = config.cpu_power;
		cpu_accum = 0;
	}
}

#define STATE_VERSION_100		2
								// version 1.00 - version 1.10
#define STATE_VERSION_120		3
								// version 1.20 -

void EVENT::save_state(FILEIO* state_fio)
{
	state_fio->FputUint32(STATE_VERSION_120);
	state_fio->FputInt32(this_device_id);
	
	state_fio->FputInt32(dcount_cpu);
	for(int i = 0; i < dcount_cpu; i++) {
		state_fio->FputUint32(d_cpu[i].cpu_clocks);
		state_fio->FputUint32(d_cpu[i].update_clocks);
		state_fio->FputUint32(d_cpu[i].accum_clocks);
	}
	state_fio->Fwrite(vclocks, sizeof(vclocks), 1);
	state_fio->FputInt32(event_remain);
	state_fio->FputInt32(cpu_remain);
	state_fio->FputInt32(cpu_accum);
	state_fio->FputInt32(cpu_done);
#ifdef SDL
	state_fio->FputUint32(event_clocks);
	state_fio->FputUint32(mix_clock);
#else
	state_fio->FputUint64(event_clocks);
#endif // SDL
	for(int i = 0; i < MAX_EVENT; i++) {
		state_fio->FputInt32(event[i].device != NULL ? event[i].device->this_device_id : -1);
		state_fio->FputInt32(event[i].event_id);
#ifdef SDL
		state_fio->FputUint32(event[i].remain_clock);
		state_fio->FputUint32(event[i].loop_clock);
		state_fio->FputUint32(event[i].passed_clock);
		state_fio->FputBool(event[i].loop_enable);
#else
		state_fio->FputUint64(event[i].expired_clock);
		state_fio->FputUint64(event[i].loop_clock);
		state_fio->FputUint64(event[i].accum_clocks);
#endif // SDL
		state_fio->FputBool(event[i].active);
		state_fio->FputInt32(event[i].next != NULL ? event[i].next->index : -1);
		state_fio->FputInt32(event[i].prev != NULL ? event[i].prev->index : -1);
	}
	state_fio->FputInt32(first_free_event != NULL ? first_free_event->index : -1);
	state_fio->FputInt32(first_fire_event != NULL ? first_fire_event->index : -1);
	state_fio->FputDouble(frames_per_sec);
	state_fio->FputDouble(next_frames_per_sec);
	state_fio->FputInt32(lines_per_frame);
	state_fio->FputInt32(next_lines_per_frame);
#ifdef SDL
	state_fio->FputBool(single_exec);

	// version 1.20
	state_fio->FputBool(regist_event_ctrl);
	state_fio->FputInt32(vblank_clocks);
	state_fio->FputUint32(single_exec_clock);
#endif // SDL
}

bool EVENT::load_state(FILEIO* state_fio)
{
#ifdef SDL
	uint32 version;

	version = state_fio->FgetUint32();
	if ((version < STATE_VERSION_100) || (version > STATE_VERSION_120)) {
		return false;
	}
#else
	if(state_fio->FgetUint32() != STATE_VERSION_100) {
		return false;
	}
#endif // SDL
	if(state_fio->FgetInt32() != this_device_id) {
		return false;
	}
	if(state_fio->FgetInt32() != dcount_cpu) {
		return false;
	}
	for(int i = 0; i < dcount_cpu; i++) {
		d_cpu[i].cpu_clocks = state_fio->FgetUint32();
		d_cpu[i].update_clocks = state_fio->FgetUint32();
		d_cpu[i].accum_clocks = state_fio->FgetUint32();
	}
	state_fio->Fread(vclocks, sizeof(vclocks), 1);
	event_remain = state_fio->FgetInt32();
	cpu_remain = state_fio->FgetInt32();
	cpu_accum = state_fio->FgetInt32();
	cpu_done = state_fio->FgetInt32();
#ifdef SDL
	event_clocks = state_fio->FgetUint32();
	mix_clock = state_fio->FgetUint32();
#else
	event_clocks = state_fio->FgetUint64();
#endif // SDL
	for(int i = 0; i < MAX_EVENT; i++) {
		event[i].device = vm->get_device(state_fio->FgetInt32());
		event[i].event_id = state_fio->FgetInt32();
#ifdef SDL
		event[i].remain_clock = state_fio->FgetUint32();
		event[i].loop_clock = state_fio->FgetUint32();
		event[i].passed_clock = state_fio->FgetUint32();
		event[i].loop_enable = state_fio->FgetBool();
#else
		event[i].expired_clock = state_fio->FgetUint64();
		event[i].loop_clock = state_fio->FgetUint64();
		event[i].accum_clocks = state_fio->FgetUint64();
#endif // SDL
		event[i].active = state_fio->FgetBool();
		event[i].next = (event_t *)get_event(state_fio->FgetInt32());
		event[i].prev = (event_t *)get_event(state_fio->FgetInt32());
	}
	first_free_event = (event_t *)get_event(state_fio->FgetInt32());
	first_fire_event = (event_t *)get_event(state_fio->FgetInt32());
	frames_per_sec = state_fio->FgetDouble();
	next_frames_per_sec = state_fio->FgetDouble();
	lines_per_frame = state_fio->FgetInt32();
	next_lines_per_frame = state_fio->FgetInt32();
#ifdef SDL
	single_exec = state_fio->FgetBool();

	// version 1.20
	if (version >= STATE_VERSION_120) {
		regist_event_ctrl = state_fio->FgetBool();
		vblank_clocks = state_fio->FgetInt32();
		single_exec_clock = state_fio->FgetUint32();
	}
	else {
		// version 1.10 -> version 1.20 conversion
		event_t *event_handle;

		// keep regist_event_ctrl current value
		vblank_clocks = 0;
		cpu_remain = event_remain;
		single_exec_clock = event_clocks;

		// add first_fire_event->passed_clock to all valid events
		if (first_fire_event != NULL) {
			event_handle = first_fire_event->next;
			while (event_handle != NULL) {
				event_handle->passed_clock += first_fire_event->passed_clock;
				if (event_handle->passed_clock >= event_handle->remain_clock) {
					event_handle->passed_clock = event_handle->remain_clock - 1;
				}
				event_handle = event_handle->next;
			}
		}
	}

	// force update timing
	for(DEVICE* device = vm->first_device; device; device = device->next_device) {
		if(device->event_manager_id() == this_device_id) {
			device->update_timing(d_cpu[0].cpu_clocks, frames_per_sec, lines_per_frame);
		}
	}
#endif // SDL
	
	// post process
	if(sound_buffer) {
		memset(sound_buffer, 0, sound_samples * sizeof(uint16) * 2);
	}
	if(sound_tmp) {
		memset(sound_tmp, 0, sound_tmp_samples * sizeof(int32) * 2);
	}
	buffer_ptr = 0;
#ifdef SDL
	mix_ptr = 0;
#endif // SDL
	return true;
}

void* EVENT::get_event(int index)
{
	if(index >= 0 && index < MAX_EVENT) {
		return &event[index];
	}
	return NULL;
}
