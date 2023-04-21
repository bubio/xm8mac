//
// eXcellent Multi-platform emulator type 8 - 'XM8'
// based on ePC-8801MA
//
// Author (ePC-8801MA) : Takeda.Toshiya
// Author (XM8) : Tanaka.Yasushi
//
// [ disk subsystem ]
//

#ifdef SDL

#include "os.h"
#include "common.h"
#include "device.h"
#include "emu.h"
#include "fileio.h"
#include "disk.h"
#include "z80.h"
#include "upd765a.h"
#include "i8255.h"
#include "disksub.h"

//
// DiskSub()
// constructor
//
DiskSub::DiskSub(VM *vm, EMU *emu) : DEVICE(vm, emu)
{
	z80 = NULL;
	upd765a = NULL;
	i8255 = NULL;
}

//
// ~DiskSub()
// destructor
//
DiskSub::~DiskSub()
{
	release();
}

//
// initialize()
// initiailize
//
void DiskSub::initialize()
{
	FILEIO *fio;
	int high;
	int low;
	uint8 *ptr;
	uint8 eor;

	// clear rom & initialize (JR *)
	memset(&memory[0x0000], 0xff, 0x2000);
	memory[0x0000] = 0x18;
	memory[0x0001] = 0xfe;

	// clear higher memory
	memset(&memory[0x8000], 0xff, 0x8000);

	// inititlize 0x2000-0x7fff
	ptr = &memory[0x2000];
	for (high=0; high<0x60; high++) {
		// get xor data
		switch (init_table[high]) {
		case 0:
			eor = 0xf0;
			break;
		case 1:
			eor = 0x0f;
			break;
		case 2:
			eor = 0xff;
			break;
		default:
			eor = 0x00;
			break;
		}

		// fill memory
		for (low=0; low<0x10; low++) {
			memset(ptr, init_pattern[low] ^ eor, 0x10);
			ptr += 0x10;
		}
	}

	// read ROM (both PC88.ROM and DISK.ROM)
	fio = new FILEIO;
	if (fio->Fopen(emu->bios_path(_T("PC88.ROM")), FILEIO_READ_BINARY) == true) {
		fio->Fseek(0x14000, FILEIO_SEEK_CUR);
		fio->Fread(&memory[0], 1, 0x2000);
		fio->Fclose();
	}
	if (fio->Fopen(emu->bios_path(_T("DISK.ROM")), FILEIO_READ_BINARY) ==  true) {
		fio->Fread(&memory[0], 1, 0x2000);
		fio->Fclose();
	}
	delete fio;
}

//
// release()
// deinitialize
//
void DiskSub::release()
{
}

//
// reset()
// reset
//
void DiskSub::reset()
{
	// reset drive mode
	upd765a->set_drive_type(0, DRIVE_TYPE_2D);
	upd765a->set_drive_type(1, DRIVE_TYPE_2D);
	
	// clear pio output
	i8255->write_io8(1, 0);
	i8255->write_io8(2, 0);
}

//
// set_context_cpu()
// set cpu
//
void DiskSub::set_context_cpu(Z80 *cpu)
{
	z80 = cpu;
}

//
// set_context_fdc()
// set fdc
//
void DiskSub::set_context_fdc(UPD765A *fdc)
{
	upd765a = fdc;
}

//
// set_context_pio()
// set ppi
//
void DiskSub::set_context_pio(I8255 *pio)
{
	i8255 = pio;
}

//
// write_data8()
// write memory
//
void DiskSub::write_data8(uint32 addr, uint32 data)
{
	if ((addr >= 0x4000) && (addr < 0x8000)) {
		memory[addr] = (uint8)data;
	}
}

//
// read_data8()
// read memory
//
uint32 DiskSub::read_data8(uint32 addr)
{
	return memory[addr];
}

//
// fetch_op()
// fetch op
//
uint32 DiskSub::fetch_op(uint32 addr, int *wait)
{
	// no access wait (both ROM and RAM)
	*wait = 0;
	return memory[addr];
}

//
// write_io8()
// port out
//
void DiskSub::write_io8(uint32 addr, uint32 data)
{
	uint32 port_c;

	switch (addr & 0xff) {
	// drive mode
	case 0xf4:
		portf4_out(data);
		break;

	// write pre compensation
	case 0xf7:
		break;

	// drive motor
	case 0xf8:
		// both drives always set force ready signal
		upd765a->write_signal(SIG_UPD765A_FREADY, 1, 1);
		break;

	// FDC
	case 0xfb:
		upd765a->write_io8(addr, data);
		break;

	// PIO (data)
	case 0xfc:
	case 0xfd:
		i8255->write_io8(addr, data);
		break;

	// PIO (ctrl)
	case 0xfe:
		port_c = i8255->read_signal(SIG_I8255_PORT_C);
		i8255->write_io8(2, data);
		if (port_c != i8255->read_signal(SIG_I8255_PORT_C)) {
			// port C has been changed
			request_single_exec();
		}
		break;

	// PIO (indirect ctrl)
	case 0xff:
		port_c = i8255->read_signal(SIG_I8255_PORT_C);
		i8255->write_io8(3, data);
		if (port_c != i8255->read_signal(SIG_I8255_PORT_C)) {
			// port C has been changed
			request_single_exec();
		}
		break;
	}
}

//
// portf4_out()
// port f4
//
void DiskSub::portf4_out(uint32 data)
{
	// MR/MH/MA/MA2/MA... type ROM only
	if (memory[0x7ee] != 0xfe) {
		return;
	}

	// drive 0
	if ((data & 0x01) != 0) {
		upd765a->set_drive_type(0, DRIVE_TYPE_2HD);
	}
	else {
		if ((data & 0x04) != 0) {
			upd765a->set_drive_type(0, DRIVE_TYPE_2DD);
		}
		else {
			upd765a->set_drive_type(0, DRIVE_TYPE_2D);
		}
	}

	// drive 1
	if ((data & 0x02) != 0) {
		upd765a->set_drive_type(1, DRIVE_TYPE_2HD);
	}
	else {
		if ((data & 0x08) != 0) {
			upd765a->set_drive_type(1, DRIVE_TYPE_2DD);
		}
		else {
			upd765a->set_drive_type(1, DRIVE_TYPE_2D);
		}
	}
}

//
// read_io8()
// port in
//
uint32 DiskSub::read_io8(uint32 addr)
{
	switch (addr & 0xff) {
	// FDC TC signal
	case 0xf8:
		upd765a->write_signal(SIG_UPD765A_TC, 1, 1);
		break;

	// FDC
	case 0xfa:
	case 0xfb:
		return upd765a->read_io8(addr);

	// PIO
	case 0xfc:
	case 0xfd:
	case 0xfe:
	case 0xff:
		return i8255->read_io8(addr);

	default:
		break;
	}

	return 0xff;
}

//
// intr_ack()
// interrupt ack
//
uint32 DiskSub::intr_ack()
{
	// return NOP opcode
	return 0x00;
}

#define STATE_VERSION_100		1
										// version 1.00-

//
// save_state()
// save state
//
void DiskSub::save_state(FILEIO *fio)
{
	fio->FputUint32(STATE_VERSION_100);
	fio->FputInt32(this_device_id);

	// RAM data
	fio->Fwrite(&memory[0x4000], 1, 0x4000);
}

//
// load_state()
// load state
//
bool DiskSub::load_state(FILEIO *fio)
{
	if (fio->FgetUint32() != STATE_VERSION_100) {
		return false;
	}

	if (fio->FgetInt32() != this_device_id) {
		return false;
	}

	fio->Fread(&memory[0x4000], 1, 0x4000);

	return true;
}

//
// init_table
//
// 0:F0 0F...
// 1:0F F0...
// 2:FF 00...
// 3:00 FF...
//
const uint8 DiskSub::init_table[0x60] = {
	// 0x2000
	0, 1, 0, 1, 0, 3, 0, 0, 0, 0, 1, 0, 1, 0, 1, 0,

	// 0x3000
	0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 0,

	// 0x4000
	1, 0, 1, 0, 1, 0, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1,

	// 0x5000
	1, 0, 1, 0, 1, 0, 1, 1, 1, 1, 3, 1, 0, 1, 0, 1,

	// 0x6000
	1, 0, 1, 0, 1, 2, 1, 1, 1, 1, 0, 1, 0, 1, 0, 1,

	// 0x7000
	1, 0, 1, 0, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 0, 1
};

//
// init_pattern
//
const uint8 DiskSub::init_pattern[0x10] = {
	0x00,
	0xff,
	0x00,
	0xff,
	0xff,
	0x00,
	0xff,
	0x00,
	0x00,
	0xff,
	0x00,
	0xff,
	0xff,
	0x00,
	0xff,
	0x00
};

#endif // SDL
