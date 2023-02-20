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

#include "os.h"
#include "common.h"
#include "fileio.h"
#include "disk.h"
#include "upd765a.h"
#include "vm.h"
#include "diskmgr.h"

//
// defines
//
#define DISK_NEXT_FRAME			32
										// frames to set next bank

//
// DiskManager()
// constructor
//
DiskManager::DiskManager()
{
	// object
	vm = NULL;
	upd765a = NULL;

	// others
	drive = 0;
	ready = false;
	readonly = false;
	name_list = NULL;
	wp_list = NULL;
	signal = 0;
	path[0] = '\0';
	dir[0] = '\0';
	state_path[0] = '\0';
	next_bank = 0;
	next_timer = 0;
	nullstr[0] = '\0';
}

//
// ~DiskManager()
// destructor
//
DiskManager::~DiskManager()
{
	Deinit();
}

//
// Init()
// initialize
//
bool DiskManager::Init(VM *v, int drv)
{
	// get and save object
	vm = v;
	upd765a = (UPD765A*)vm->get_device(11);

	// record drive
	drive = drv;

	// not ready
	ready = false;

	return true;
}

//
// Deinit()
// deinitialize
//
void DiskManager::Deinit()
{
	// close current disk
	Close();
}

//
// SetVM()
// re-set vm
//
void DiskManager::SetVM(VM *v)
{
	// get and save object
	vm = v;
	upd765a = (UPD765A*)vm->get_device(11);
}

//
// Open()
// open disk
//
bool DiskManager::Open(const char *filename, int bank)
{
	char *ptr;
	char *last;

	// save path
	if (strlen(filename) >= sizeof(path)) {
		return false;
	}
	strcpy(path, filename);

	// save directory
	strcpy(dir, path);
	ptr = dir;
	last = dir;

	// search last '\\' or '/'
	while (*ptr != '\0') {
		if ((*ptr == '\\') || (*ptr == '/')) {
			last = ptr;
		}
		ptr++;
	}

	// end mark
	last[1] = '\0';

	return Open(bank);
}

//
// Open()
// reopen disk
//
bool DiskManager::Open(int bank)
{
	// close
	Close();

	// analyze
	if (Analyze() == false) {
		return false;
	}

	// set bank
	if (bank >= num_of_banks) {
		current_bank = num_of_banks - 1;
	}
	else {
		current_bank = bank;
	}

	// open
	vm->open_disk(drive, (_TCHAR*)path, current_bank);

	// ready
	ready = true;

	return true;
}

//
// Close()
// close disk
//
void DiskManager::Close()
{
	// close
	if (ready == true) {
		vm->close_disk(drive);
		ready = false;
	}

	// delete name list
	if (name_list != NULL) {
		SDL_free(name_list);
		name_list = NULL;
	}

	// delete write protect list
	if (wp_list != NULL) {
		SDL_free(wp_list);
		wp_list = NULL;
	}

	// parameters
	readonly = false;
	num_of_banks = 0;
	current_bank = 0;
	next_timer = 0;
}

//
// IsOpen()
// check drive ready
//
bool DiskManager::IsOpen()
{
	return ready;
}

//
// IsNext()
// check next ready
//
bool DiskManager::IsNext()
{
	if ((ready == true) && (next_timer > 0)) {
		return true;
	}
	else {
		return false;
	}
}

//
// IsProtect()
// check write protect
//
bool DiskManager::IsProtect()
{
	// open ?
	if (ready == true) {
		// next timer
		if (next_timer > 0) {
			return false;
		}

		// file
		if (readonly == true) {
			return true;
		}

		// bank
		if (wp_list[current_bank] != 0x00) {
			return true;
		}
	}

	return false;
}

//
// GetName()
// get name of current bank
//
const char* DiskManager::GetName(int bank)
{
	const char *ptr;
	int loop;
	size_t len;

	// default parameter -> current_bank
	if (bank < 0) {
		if (next_timer > 0) {
			bank = next_bank;
		}
		else {
			bank = current_bank;
		}
	}

	// open ?
	if (ready == false) {
		return nullstr;
	}

	ptr = name_list;
	for (loop=0; loop<bank; loop++) {
		len = strlen(ptr);
		ptr += len;
		ptr++;
	}

	return ptr;
}

//
// GetAccess()
// get access status
//
int DiskManager::GetAccess()
{
	uint8 type;
	int busy;

	// get information from fdc
	type = upd765a->get_drive_type(drive);
	busy = upd765a->get_busy_drive();

	// check access
	if (busy != drive) {
		return ACCESS_NONE;
	}

	// check 2D or 2HD
	if (type == DRIVE_TYPE_2HD) {
		return ACCESS_2HD;
	}
	else {
		return ACCESS_2D;
	}
}

//
// GetDir()
// get disk dir
//
const char* DiskManager::GetDir()
{
	return dir;
}

//
// GetFileName()
// get disk file name
//
const char* DiskManager::GetFileName()
{
	size_t len;

	// open ?
	if (ready == false) {
		return nullstr;
	}

	// get length of directory
	len = strlen(dir);
	SDL_assert(len > 0);
	SDL_assert(strlen(path) > len);

	return &path[len];
}

//
// GetBank()
// get current disk bank
//
int DiskManager::GetBank()
{
	return current_bank;
}

//
// GetBanks()
// get number of banks
//
int DiskManager::GetBanks()
{
	return num_of_banks;
}

//
// SetBank
// change disk bank
//
bool DiskManager::SetBank(int bank)
{
	if ((bank >= 0) && (bank < num_of_banks)) {
		// open (dummy to access file)
		vm->close_disk(drive);
		vm->open_disk(drive, (_TCHAR*)path, bank);

		// close
		vm->close_disk(drive);

		// set timer
		next_bank = bank;
		next_timer = DISK_NEXT_FRAME;
		return true;
	}

	return false;
}

//
// ProcessMgr()
// process per frame
//
void DiskManager::ProcessMgr()
{
	if (next_timer > 0) {
		next_timer--;
		if (next_timer == 0) {
			current_bank = next_bank;
			vm->open_disk(drive, (_TCHAR*)path, current_bank);
		}
	}
}

//
// Load()
// load state
//
void DiskManager::Load(FILEIO *fio)
{
	bool rdy;
	int bank;

	// close
	Close();

	// load parameter
	fio->Fread(state_path, 1, sizeof(state_path));
	rdy = fio->FgetBool();
	bank = fio->FgetInt32();
	next_bank = fio->FgetInt32();
	next_timer = fio->FgetInt32();

	// open
	if (rdy == true) {
		Open(state_path, bank);
	}

	// next bank
	if (next_timer > 0) {
		vm->close_disk(drive);
	}
}

//
// Save()
// save state
//
void DiskManager::Save(FILEIO *fio)
{
	fio->Fwrite(path, 1, sizeof(path));
	fio->FputBool(ready);
	fio->FputInt32(current_bank);
	fio->FputInt32(next_bank);
	fio->FputInt32(next_timer);
}

//
// Analyze()
// analyze d88 header
//
bool DiskManager::Analyze()
{
	FILEIO fio;
	Uint8 header[0x2b0];
	Uint32 offset;
	Uint32 add;
	size_t len;
	int bank;
	char *ptr;
	int track;
	Uint32 trkofs;

	// open
	if (fio.Fopen(path, FILEIO_READ_BINARY) == false) {
		return false;
	}

	// version 1.70
	readonly = fio.IsProtected(path);

	// clear
	num_of_banks = 0;
	offset = 0;
	len = 0;

	// bank loop (1)
	for (;;) {
		// read D88 header
		if (fio.Fread(header, 1, sizeof(header)) != sizeof(header)) {
			// EOF
			break;
		}

		// track0 offset in header must be 0x000002x0
		if ((header[0x23] != 0x00) ||
			(header[0x22] != 0x00) ||
			(header[0x21] != 0x02) ||
			((header[0x20] & 0x0f) != 0x00)) {
			// illegal format
			if (header[0x21] != 0x00) {
				num_of_banks = 0;
				break;
			}
		}

		// bank++
		num_of_banks++;

		// add length of disk name
		header[0x10] = 0x00;
		len += strlen((const char*)header);
		len++;

		// size
		add = (Uint32)header[0x1f];
		add <<= 8;
		add |= (Uint32)header[0x1e];
		add <<= 8;
		add |= (Uint32)header[0x1d];
		add <<= 8;
		add |= (Uint32)header[0x1c];

		// check track offset
		for (track=0; track<160; track++) {
			trkofs = (Uint32)(header[0x20 + track * 4 + 3]);
			trkofs <<= 8;
			trkofs |= (Uint32)(header[0x20 + track * 4 + 2]);
			trkofs <<= 8;
			trkofs |= (Uint32)(header[0x20 + track * 4 + 1]);
			trkofs <<= 8;
			trkofs |= (Uint32)(header[0x20 + track * 4 + 0]);

			// track offset must be 0x10 alignment
			if ((trkofs & 0xf) != 0) {
				num_of_banks = 0;
				break;
			}
		}
		if (num_of_banks == 0) {
			break;
		}

		// seek
		offset += add;
		fio.Fseek((long)offset, FILEIO_SEEK_SET);
	}

	// num_of_banks > 0 ?
	if (num_of_banks == 0) {
		fio.Fclose();
		return false;
	}

	// malloc
	name_list = (char*)SDL_malloc(len);
	if (name_list == NULL) {
		fio.Fclose();
		return false;
	}
	wp_list = (Uint8*)SDL_malloc(num_of_banks);
	if (wp_list == NULL) {
		SDL_free(name_list);
		name_list = NULL;
		fio.Fclose();
		return false;
	}

	// bank loop (2)
	offset = 0;
	ptr = name_list;
	for (bank=0; bank<num_of_banks; bank++) {
		// seek
		fio.Fseek((long)offset, FILEIO_SEEK_SET);

		// read D88 header
		fio.Fread(header, 1, sizeof(header));

		// copy name
		header[0x10] = 0x00;
		len = strlen((const char*)header);
		strcpy(ptr, (const char*)header);
		ptr += len;
		ptr++;

		// record write protect flag
		wp_list[bank] = header[0x1a];

		// size
		add = (Uint32)header[0x1f];
		add <<= 8;
		add |= (Uint32)header[0x1e];
		add <<= 8;
		add |= (Uint32)header[0x1d];
		add <<= 8;
		add |= (Uint32)header[0x1c];
		offset += add;
	}

	// close
	fio.Fclose();

	return true;
}

#endif // SDL
