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

#ifndef DISKSUB_H
#define DISKSUB_H

//
// disk subsystem
//
class DiskSub : public DEVICE
{
public:
	DiskSub(VM *vm, EMU *emu);
										// constructor
	virtual ~DiskSub();
										// destructor
	void initialize();
										// initialize
	void release();
										// deinitialize
	void reset();
										// reset

	// memory
	void write_data8(uint32 addr, uint32 data);
										// write memory
	uint32 read_data8(uint32 addr);
										// read memory
	uint32 fetch_op(uint32 addr, int *wait);
										// read memory (fetch)

	// i/o
	void write_io8(uint32 addr, uint32 data);
										// port out
	uint32 read_io8(uint32 addr);
										// port in

	// interrupt
	uint32 intr_ack();
										// interrupt ack

	// unique
	void set_context_cpu(Z80 *cpu);
										// set cpu
	void set_context_fdc(UPD765A *fdc);
										// set fdc
	void set_context_pio(I8255 *pio);
										// set pio

	// state
	void save_state(FILEIO *fio);
										// save state
	bool load_state(FILEIO *fio);
										// load state

private:
	void portf4_out(uint32 data);
										// port f4
	Z80 *z80;
										// Z80 CPU
	UPD765A *upd765a;
										// uPD765A FDC
	I8255 *i8255;
										// i8255 PIO
	uint8 memory[0x10000];
										// memory
	static const uint8 init_table[0x60];
										// memory initialize table
	static const uint8 init_pattern[0x10];
										// memory initialize pattern
};

#endif // DISKSUB_H

#endif // SDL
