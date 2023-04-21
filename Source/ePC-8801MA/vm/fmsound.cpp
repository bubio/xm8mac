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

#include "os.h"
#include "common.h"
#include "device.h"
#include "emu.h"
#include "fmsound.h"

//
// defines
//
#define EVENT_FM_TIMER			0
										// timer id
#define OPN_BUSY_USEC			20
										// write busy us (OPN)
#define OPNA_BUSY_USEC			10
										// write busy us (OPNA)

//
// FmSound()
// constructor
//
FMSound::FMSound(VM *vm, EMU *emu) : DEVICE(vm, emu)
{
	// object
	opna = NULL;
	opn = NULL;

	// fm sound chip
	chip_ym2608 = false;
	chip_enable = false;
	chip_clock = 0;

	// main cpu
	cpu_clock = 0;
	cpu_busy_clock = 0;

	// outputs irq
	init_output_signals(&outputs_irq);
}

//
// ~FMSound()
// destructor
//
FMSound::~FMSound()
{
	release();
}

//
// select()
// select chip and enable mixing
//
void FMSound::select(bool ym2608, bool enable)
{
	chip_ym2608 = ym2608;
	chip_enable = enable;
}

//
// initialize()
// initialize
//
void FMSound::initialize()
{
	if (chip_ym2608 == true) {
		opna = new FM::OPNA;
	}
	else {
		opn = new FM::OPN;
	}

	// timer event
	timer_event_id = -1;
}

//
// release()
// deinitialize
//
void FMSound::release()
{
	if (opna != NULL) {
		delete opna;
		opna = NULL;
	}

	if (opn != NULL) {
		delete opn;
		opn = NULL;
	}
}

//
// reset()
// reset
//
void FMSound::reset()
{
	int loop;

	// fmgen
	if (chip_ym2608 == true) {
		opna->Reset();
	}
	else {
		opn->Reset();
	}

	// clock
	timer_clock = 0;
	timer_clock_valid = false;
	write_clock = 0;

	// busy
	busy = false;

	// irq
	irq_prev = false;

	// BLOCK / F-Number 2
	fnum2[0] = 0x00;
	fnum2[1] = 0x00;

	// write channel
	write_ch[0] = 0x00;
	write_ch[1] = 0x00;

	// write data
	write_data[0] = 0x00;
	write_data[1] = 0x00;

	// key on/off
	memset(keyon, 0, sizeof(keyon));

	// register map
	memset(reg, 0, sizeof(reg));

	// register 0x29 (for Misty Blue)
	if (chip_ym2608 == true) {
		reg[0][0x29] = 0x03;
		opna->SetReg(0x29, 0x03);
	}

	// FM TL
	for (loop=0x40; loop<0x50; loop++) {
		if ((loop & 3) != 3) {
			if (chip_ym2608 == true) {
				reg[0][loop] = 0x7f;
				opna->SetReg(loop, 0x7f);
				reg[1][loop] = 0x7f;
				opna->SetReg(0x100 | loop, 0x7f);
			}
			else {
				reg[0][loop] = 0x7f;
				opn->SetReg(loop, 0x7f);
			}
		}
	}

	// FM key off
	for (loop=0; loop<8; loop++) {
		if ((loop & 3) != 3) {
			if (chip_ym2608 == true) {
				reg[0][loop] = (uint8)loop;
				opna->SetReg(0x28, loop);
			}
			else {
				reg[0][loop & 3] = loop & 3;
				opn->SetReg(0x28, loop & 3);
			}
		}
	}
}

//
// write_io8()
// port out
//
void FMSound::write_io8(uint addr, uint32 data)
{
	switch (addr & 3) {
	// OPN channel port
	case 0:
		write_ch[0] = (uint8)data;
		write_data[0] = (uint8)data;
		break;

	// OPN data port
	case 1:
		// mask reg 0x29 if OPN
		if (chip_ym2608 == false) {
			if (write_ch[0] == 0x29) {
				data &= ~0x80;
			}
		}

		// save data
		write_data[0] = (uint8)data;

		// F-Num2 ?
		if ((write_ch[0] >= 0xa4) && (write_ch[0] <= 0xa6)) {
			// common F-Num2
			fnum2[0] = (uint8)data;
		}
		else {
			// timer control (1)
			if (write_ch[0] == 0x27) {
				update_timer();
			}

			if ((write_ch[0] >= 0xa0) && (write_ch[0] <= 0xa2)) {
				// set F-Num2 at this time (for Kenja no Yuigon)
				if (reg[0][write_ch[0] + 4] != fnum2[0]) {
					mix_sound_block();
					reg[0][write_ch[0] + 4] = fnum2[0];
					if (chip_ym2608 == true) {
						opna->SetReg((uint)(write_ch[0] + 4), (uint)fnum2[0]);
					}
					else {
						opn->SetReg((uint)(write_ch[0] + 4), (uint)fnum2[0]);
					}
				}
			}

			// mix sound
			if ((write_ch[0] == 8) || (write_ch[0] == 9) || (write_ch[0] == 10) || (write_ch[0] == 0x28)) {
				mix_sound_block();
			}
			else {
				if (reg[0][write_ch[0]] != data) {
					mix_sound_block();
				}
			}

			// set data to fmgen
			if (chip_ym2608 == true) {
				opna->SetReg((uint)write_ch[0], (uint)data);
			}
			else {
				opn->SetReg((uint)write_ch[0], (uint)data);
			}

			// busy flag
			write_clock = current_clock();
			busy = true;

			// timer control (2)
			if (write_ch[0] == 0x27) {
				timer_control();
				update_interrupt();
			}

			// key on/off
			if (write_ch[0] == 0x28) {
				if ((reg[0][0x29] & 0x80) != 0) {
					// FM1-6
					keyon[data & 0x07] = (uint8)(data & 0xf0);
				}
				else {
					// FM1-3
					keyon[data & 0x03] = (uint8)(data & 0xf0);
				}
			}

			// save register data
			reg[0][write_ch[0]] = (uint8)data;
		}
		break;

	// OPNA extended channel port
	case 2:
		if (chip_ym2608 == false) {
			break;
		}

		write_ch[1] = (uint8)data;
		write_data[1] = (uint8)data;
		break;

	// OPNA extended data port
	case 3:
		if (chip_ym2608 == false) {
			break;
		}

		// save data
		write_data[1] = (uint8)data;

		// F-Num2 ?
		if ((write_ch[1] >= 0xa4) && (write_ch[1] <= 0xa6)) {
			// common F-Num2
			fnum2[1] = (uint8)data;
		}
		else {
			if ((write_ch[1] >= 0xa0) && (write_ch[1] <= 0xa2)) {
				// set F-Num2 at this time
				if (reg[1][write_ch[1] + 4] != fnum2[1]) {
					mix_sound_block();
					reg[1][write_ch[1] + 4] = fnum2[1];
					opna->SetReg((uint)(0x100 | (write_ch[1] + 4)), (uint)fnum2[1]);
				}
			}

			// mix sound
			if (write_ch[1] <= 0x10) {
				mix_sound_block();
			}
			else {
				if (reg[1][write_ch[1]] != data) {
					mix_sound_block();
				}
			}

			// set data to fmgen
			opna->SetReg((uint)(0x100 | write_ch[1]), (uint)data);

			// busy flag
			write_clock = current_clock();
			busy = true;

			// update interrupt if ADPCM
			if (write_ch[1] <= 0x10) {
				update_interrupt();
			}

			// save register data
			reg[1][write_ch[1]] = (uint8)data;
		}
		break;
	}
}

//
// read_io8()
// port in
//
uint32 FMSound::read_io8(uint32 addr)
{
	uint32 status;

	// initialize
	status = 0xff;

	switch (addr & 3) {
	// OPN status port
	case 0:
		// from fmgen
		if (chip_ym2608 == true) {
			status = (uint32)opna->ReadStatus();
		}
		else {
			status = (uint32)opn->ReadStatus();
		}

		// busy timeout
		if (busy == true) {
			if (passed_clock(write_clock) > cpu_busy_clock) {
				busy = false;
			}
		}

		// busy flag
		if (busy == true) {
			status |= 0x80;
		}
		else {
			status &= ~0x80;
		}
		break;

	// OPN data port
	case 1:
		if ((write_ch[0] < 0x10) || (write_ch[0] == 0xff)) {
			// SSG part or chip detect register
			if (chip_ym2608 == true) {
				status = (uint32)opna->GetReg((uint)write_ch[0]);
			}
			else {
				status = (uint32)opn->GetReg((uint)write_ch[0]);
			}
		}
		else {
			// other registers
			if (chip_ym2608 == true) {
				status = (uint32)write_data[0];
			}
			else {
				status = 0;
			}
		}
		break;

	// OPNA extended status port
	case 2:
		if (chip_ym2608 == false) {
			break;
		}

		// from fmgen
		status = (uint32)opna->ReadStatusEx();

		// busy timeout
		if (busy == true) {
			if (passed_clock(write_clock) > cpu_busy_clock) {
				busy = false;
			}
		}

		// busy flag
		if (busy == true) {
			status |= 0x80;
		}
		else {
			status &= ~0x80;
		}
		break;

	// OPNA extended data port
	case 3:
		if (chip_ym2608 == false) {
			break;
		}

		if (write_ch[1] == 0x08) {
			// ADPCM data
			status = (uint32)opna->GetReg(0x0108);
		}
		else {
			// other registers
			status = (uint32)write_data[1];
		}
		break;
	}

	return status;
}

//
// set_context_irq()
// set signal context
//
void FMSound::set_context_irq(DEVICE* device, int id, uint32 mask)
{
	register_output_signal(&outputs_irq, device, id, mask);
}

//
// mix()
// mix sound
//
void FMSound::mix(int32 *buffer, int count)
{
	if ((chip_enable == true) && (count > 0)) {
		if (chip_ym2608 == true) {
			opna->Mix(buffer, count);

			// for ADPCM EOS
			update_interrupt();
		}
		else {
			opn->Mix(buffer, count);
		}
	}
}

//
// update_timing()
// update timing
//
void FMSound::update_timing(int new_clocks, double new_frames_per_sec, int new_lines_per_frame)
{
	// save cpu clock
	cpu_clock = new_clocks;

	// get busy wait clock
	if (chip_ym2608 == true) {
		cpu_busy_clock = (uint32)((cpu_clock * OPNA_BUSY_USEC) / 1000000);
	}
	else {
		cpu_busy_clock = (uint32)((cpu_clock * OPN_BUSY_USEC) / 1000000);
	}
}

//
// timer_control()
// timer control
//
void FMSound::timer_control()
{
	uint32 next_clock;

	if (timer_clock_valid == false) {
		// first time
		timer_clock_valid = true;
		timer_clock = current_clock();
	}

	// get next event us
	if (chip_ym2608 == true) {
		if (chip_clock == cpu_clock) {
			// CPU 8MHz
			next_clock = (uint32)(opna->GetNextEvent() * 2);
		}
		else {
			// CPU 4MHz
			// next_clock = (uint32)(opna->GetNextEvent() * 2) / (OPNA 8MHz / CPU 4MHz)
			next_clock = (uint32)opna->GetNextEvent();
		}
	}
	else {
		if (chip_clock == cpu_clock) {
			// CPU 4MHz
			next_clock = (uint32)opn->GetNextEvent();
		}
		else {
			// CPU 8MHz
			next_clock = (uint32)(opn->GetNextEvent() * 2);
		}
	}

	if (next_clock > 0) {
		// cancel event
		if (timer_event_id >= 0) {
			cancel_event(this, timer_event_id);
			timer_event_id = -1;
		}

		// register event
		register_event_by_clock(this, EVENT_FM_TIMER, next_clock, false, &timer_event_id);
	}
	else {
		// cancel event
		if (timer_event_id >= 0) {
			cancel_event(this, timer_event_id);
			timer_event_id = -1;
		}

		// reset
		timer_clock_valid = false;
	}
}

//
// event_callback()
// timer event
//
void FMSound::event_callback(int event_id, int error)
{
	// mix sound (for CSM)
	if ((reg[0][0x27] & 0xc0) == 0x80) {
		mix_sound_block();
	}

	// update timer
	update_timer();

	// update interrupt
	update_interrupt();

	// ext timer
	timer_control();
}

//
// update_count()
// update timer clock
//
void FMSound::update_timer()
{
	uint32 passed;

	if (timer_clock_valid == true) {
		// get passed clock for OPN or OPNA
		passed = passed_clock(timer_clock);
		if (chip_ym2608 == true) {
			if (cpu_clock == chip_clock) {
				// CPU 8MHz
				passed /= 2;
			}
			opna->Count((int32)passed);
		}
		else {
			if (chip_clock < cpu_clock) {
				// CPU 8MHz
				passed /= 2;
			}
			opn->Count((int32)passed);
		}

		// save clock
		timer_clock = current_clock();
	}
}

//
// update_interrupt()
// interrupt control
//
void FMSound::update_interrupt()
{
	bool irq;

	// get interrupt request from fmgen
	if (chip_ym2608 == true) {
		irq = opna->ReadIRQ();
	}
	else {
		irq = opn->ReadIRQ();
	}

	// off -> on
	if ((irq_prev == false) && (irq == true)) {
		write_signals(&outputs_irq, 0xffffffff);
	}

	// on -> off
	if ((irq_prev == true) && (irq == false)) {
		write_signals(&outputs_irq, 0);
	}

	// save curernt status
	irq_prev = irq;
}

//
// is_ym2608()
// detect chip
//
bool FMSound::is_ym2608()
{
	return chip_ym2608;
}

//
// is_enable()
// detect enable
//
bool FMSound::is_enable()
{
	return chip_enable;
}

//
// init_fmgen()
// initialize fmgen
//
void FMSound::init(int rate, int clock, int samples, int vol_fm, int vol_ssg)
{
	// save chip clock
	chip_clock = clock;

	if (chip_ym2608 == true) {
		opna->Init((uint)clock, (uint)rate, false, (const char*)emu->application_path());
		opna->SetVolumeFM(vol_fm);
		opna->SetVolumePSG(vol_ssg);
	}
	else {
		opn->Init((uint)clock, (uint)rate, false, NULL);
		opn->SetVolumeFM(vol_fm);
		opn->SetVolumePSG(vol_ssg);
	}
}

//
// change_rate()
// change sample rate
//
void FMSound::change_rate(int rate)
{
	if (chip_ym2608 == true) {
		opna->SetRate((uint)chip_clock, (uint)rate, false);
	}
	else {
		opn->SetRate((uint)chip_clock, (uint)rate, false);
	}
}

#define STATE_VERSION_100		2
										// version 1.00 - version 1.10
#define STATE_VERSION_120		3
										// version 1.20 -

//
// save_state()
// save state
//
void FMSound::save_state(FILEIO *fio)
{
	fio->FputUint32(STATE_VERSION_120);
	fio->FputInt32(this_device_id);

	// ym2608 and enable
	fio->FputBool(chip_ym2608);
	fio->FputBool(chip_enable);

	// chip clock
	fio->FputInt32(chip_clock);

	// fmgen
	if (chip_ym2608 == true) {
		opna->SaveState((void*)fio);
	}
	else {
		opn->SaveState((void*)fio);
	}

	// timer
	fio->FputInt32(timer_event_id);
	fio->FputUint32(timer_clock);
	fio->FputBool(timer_clock_valid);

	// busy
	fio->FputUint32(write_clock);
	fio->FputBool(busy);

	// cpu clock
	fio->FputInt32(cpu_clock);
	fio->FputUint32(cpu_busy_clock);

	// irq
	fio->FputBool(irq_prev);

	// ch, data, fnum2, keyon, reg
	fio->Fwrite(write_ch, 1, sizeof(write_ch));
	fio->Fwrite(write_data, 1, sizeof(write_data));
	fio->Fwrite(fnum2, 1, sizeof(fnum2));
	fio->Fwrite(keyon, 1, sizeof(keyon));
	fio->Fwrite(reg, 1, sizeof(reg));
}

//
// load_state()
// load state
//
bool FMSound::load_state(FILEIO *fio)
{
	uint32 version;
	int32 device_id;
	int port;

	// version and device id
	version = fio->FgetUint32();
	device_id = fio->FgetInt32();
	if (device_id != this_device_id) {
		return false;
	}

	// for backward compatiblity
	if (version == STATE_VERSION_100) {
		// version 1.00 - version 1.10 (YM2203 class)
		if (chip_ym2608 == true) {
			if (opna->LoadState((void*)fio) == false) {
				return false;
			}
		}
		else {
			if (opn->LoadState((void*)fio) == false) {
				return false;
			}
		}

		write_ch[0] = fio->FgetUint8();

		// mode
		fio->FgetUint8();

		write_ch[1] = fio->FgetUint8();
		write_data[1] = fio->FgetUint8();

		// YM2203 general i/o port
		for (port=0; port<2; port++) {
			fio->FgetUint8();
			fio->FgetUint8();
			fio->FgetBool();
		}

		chip_clock = fio->FgetInt32();
		irq_prev = fio->FgetBool();

		// mute
		fio->FgetBool();

		// clock_prev
		fio->FgetUint32();

		// clock_accum
		fio->FgetUint32();

		// clock_const
		fio->FgetUint32();

		write_clock = fio->FgetUint32();
		busy = fio->FgetBool();
		reg[0][0x27] = fio->FgetUint8();

		// setup timer
		timer_clock_valid = false;
		timer_control();

		return true;
	}

	// version 1.20
	if (version == STATE_VERSION_120) {
		// version 1.20 -

		// ym2608 and enable
		if (fio->FgetBool() != chip_ym2608) {
			return false;
		}
		chip_enable = fio->FgetBool();

		// chip clock
		chip_clock= fio->FgetInt32();

		// fmgen
		if (chip_ym2608 == true) {
			if (opna->LoadState((void*)fio) == false) {
				return false;
			}
		}
		else {
			if (opn->LoadState((void*)fio) == false) {
				return false;
			}
		}

		// timer
		timer_event_id = fio->FgetInt32();
		timer_clock = fio->FgetUint32();
		timer_clock_valid = fio->FgetBool();

		// busy
		write_clock = fio->FgetUint32();
		busy = fio->FgetBool();

		// cpu clock
		cpu_clock = fio->FgetInt32();
		cpu_busy_clock = fio->FgetUint32();

		// irq
		irq_prev = fio->FgetBool();

		// ch, data, fnum2, keyon, reg
		fio->Fread(write_ch, 1, sizeof(write_ch));
		fio->Fread(write_data, 1, sizeof(write_data));
		fio->Fread(fnum2, 1, sizeof(fnum2));
		fio->Fread(keyon, 1, sizeof(keyon));
		fio->Fread(reg, 1, sizeof(reg));

		return true;
	}

	return false;
}

#endif // SDL
