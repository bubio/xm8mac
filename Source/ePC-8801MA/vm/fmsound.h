//
// eXcellent Multi-platform emulator type 8 - 'XM8'
// based on ePC-8801MA
//
// Author (ePC-8801MA) : Takeda.Toshiya
// Author (XM8) : Tanaka.Yasushi
//
// [ fm sound chip (YM2203 / YM2608) ]
//

#ifdef SDL

#ifndef FMSOUND_H
#define FMSOUND_H

#include "opna.h"

//
// fm sound chip
//
class FMSound : public DEVICE
{
public:
	FMSound(VM *vm, EMU *emu);
										// constructor
	virtual ~FMSound();
										// destructor
	void select(bool ym2608, bool enable);
										// select chip and enable mixing
	void initialize();
										// initialize
	void release();
										// deinitialize
	void reset();
										// reset

	// i/o
	void write_io8(uint32 addr, uint32 data);
										// port out
	uint32 read_io8(uint32 addr);
										// port in

	// context
	void set_context_irq(DEVICE* device, int id, uint32 mask);
										// set signal context

	// mix
	void mix(int32 *buffer, int count);
										// mix sound

	// timing
	void update_timing(int new_clocks, double new_frames_per_sec, int new_lines_per_frame);
										// update timing

	// event
	void event_callback(int event_id, int error);
										// timer event

	// detect chip and enable
	bool is_ym2608();
										// detect YM2608
	bool is_enable();
										// detect enable

	// state
	void save_state(FILEIO *fio);
										// save state
	bool load_state(FILEIO *fio);
										// load state

	// fmgen
	void init(int rate, int clock, int samples, int vol_fm, int vol_ssg);
										// initialize fmgen
	void change_rate(int rate);
										// change sample rate

private:
	void update_timer();
										// update timer clock
	void update_interrupt();
										// interrupt control
	void timer_control();
										// timer control
	int timer_event_id;
										// timer event id
	uint32 timer_clock;
										// timer clock
	bool timer_clock_valid;
										// timer clock (valid)
	uint32 write_clock;
										// write clock
	bool busy;
										// busy flag clock
	FM::OPNA *opna;
										// opna instance
	FM::OPN *opn;
										// opn instance
	bool chip_ym2608;
										// chip select
	bool chip_enable;
										// chip enable
	int chip_clock;
										// chip clock
	int cpu_clock;
										// cpu clock
	uint32 cpu_busy_clock;
										// cpu clock (busy)
	bool irq_prev;
										// irq (previous)
	outputs_t outputs_irq;
										// irq signal
	uint8 write_ch[2];
										// channel
	uint8 write_data[2];
										// data
	uint8 fnum2[2];
										// A4-A6 and 1A4-1A6
	uint8 keyon[8];
										// key on/off
	uint8 reg[2][0x100];
										// register data
};

#endif // FMSOUND_H

#endif // SDL
