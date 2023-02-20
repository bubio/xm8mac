//
// eXcellent Multi-platform emulator type 8 - 'XM8'
// based on ePC-8801MA
//
// Author (ePC-8801MA) : Takeda.Toshiya
// Author (XM8) : Tanaka.Yasushi
//
// [ disk manager ]
//

#ifdef SDL

#ifndef DISKMGR_H
#define DISKMGR_H

//
// defines
//
#define ACCESS_NONE				0
										// no access
#define ACCESS_2D				1
										// access (2D)
#define ACCESS_2HD				2
										// access (2HD)
#define ACCESS_MAX				3
										// access (for initialize)

//
// disk manager
//
class DiskManager
{
public:
	DiskManager();
										// constructor
	virtual ~DiskManager();
										// destructor
	bool Init(VM *v, int drv);
										// initialize
	void Deinit();
										// deinitialize
	void SetVM(VM *v);
										// re-set vm

	// operation
	bool Open(const char *filename, int bank);
										// open
	bool Open(int bank);
										// reopen
	void Close();
										// close
	bool IsOpen();
										// check drive ready
	bool IsNext();
										// check next ready
	bool IsProtect();
										// check write protect
	const char* GetName(int bank = -1);
										// get name of bank
	int GetAccess();
										// get access status
	const char* GetDir();
										// get disk dir
	const char* GetFileName();
										// get disk file name
	int GetBank();
										// get current bank
	int GetBanks();
										// get number of banks
	bool SetBank(int bank);
										// change disk bank
	void ProcessMgr();
										// change disk bank (timer)
	void Load(FILEIO *fio);
										// load state
	void Save(FILEIO *fio);
										// save state

private:
	bool Analyze();
										// analyze d88 header
	int drive;
										// drive number (0 or 1)
	VM *vm;
										// virtual machine
	UPD765A *upd765a;
										// fdc
	char path[_MAX_PATH * 3];
										// disk path
	char dir[_MAX_PATH * 3];
										// disk dir
	char state_path[_MAX_PATH * 3];
										// state path
	bool ready;
										// open flag
	bool readonly;
										// write protect flag
	char *name_list;
										// disk name list
	Uint8 *wp_list;
										// write protect list
	int num_of_banks;
										// number of banks
	int current_bank;
										// current bank
	Uint32 signal;
										// access signal
	int next_bank;
										// next bank
	int next_timer;
										// next timer
	char nullstr[1];
										// null string
};

#endif // DISKMGR_H

#endif // SDL
