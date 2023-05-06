//
// eXcellent Multi-platform emulator type 8 - 'XM8'
// based on ePC-8801MA
//
// Author (ePC-8801MA) : Takeda.Toshiya
// Author (XM8) : Tanaka.Yasushi
//
// [ tape manager ]
//

#ifdef SDL

#ifndef TAPEMGR_H
#define TAPEMGR_H

//
// tape manager
//
class TapeManager
{
public:
	TapeManager();
										// constructor
	virtual ~TapeManager();
										// destructor
	bool Init(VM *v);
										// initialize
	void Deinit();
										// deinitialize
	void SetVM(VM *v);
										// re-set vm

	// operation
	bool Play(const char *filename = NULL);
										// play
	bool Rec(const char *filename = NULL);
										// rec
	void Eject();
										// eject
	bool IsPlay();
										// check tape mount (play)
	bool IsRec();
										// check tape mount (rec)
	const char* GetDir();
										// get tape dir
	const char* GetFileName();
										// get tape file name
	void Load(FILEIO *fio);
										// load state
	void Save(FILEIO *fio);
										// save state

private:
	bool Open(const char *filename, bool rec);
										// open and close
	VM *vm;
										// virtual machine
	bool mount_play;
										// mount flag (play)
	bool mount_rec;
										// mount flag (rec)
	char path[_MAX_PATH * 3];
										// tape path
	char dir[_MAX_PATH * 3];
										// tape dir
	char state_path[_MAX_PATH * 3];
										// state path
	char nullstr[1];
										// null string
};

#endif // TAPEMGR_H

#endif // SDL
