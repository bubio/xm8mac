//
// eXcellent Multi-platform emulator type 8 - 'XM8'
// based on ePC-8801MA
//
// Author : Takeda.Toshiya (ePC-8801MA)
// Author : Tanaka.Yasushi (XM8)
//
// [ file i/o for fmgen ]
//

#ifdef SDL

#include "os.h"
#include "common.h"
#include "file.h"

//
// FileIO()
// constructor
//
FileIO::FileIO()
{
	hfile = NULL;
}

//
// ~FileIO()
// destructor
//
FileIO::~FileIO()
{
	Close();
}

//
// Open()
// open file
//
bool FileIO::Open(const _TCHAR* filename, uint flg)
{
	SDL_RWops *ops;

	Close();

	// support readonly mode
	if (flg != readonly) {
		return false;
	}

	// open with 'rb' mode
	ops = SDL_RWFromFile(filename, "rb");
	if (ops == NULL) {
		return false;
	}

	// return
	hfile = ops;
	return true;
}

//
// Close()
// close file
//
void FileIO::Close()
{
	if (hfile != NULL) {
		SDL_RWclose((SDL_RWops*)hfile);
		hfile = NULL;
	}
}

//
// Read()
// read from byte stream
//
int32 FileIO::Read(void* dest, int32 len)
{
	size_t s;

	if (hfile == NULL) {
		SDL_assert(false);
		return -1;
	}

	s = SDL_RWread((SDL_RWops*)hfile, dest, 1, (size_t)len);

	return (int32)s;
}

//
// Seek()
// seek any position
//
bool FileIO::Seek(int32 fpos, SeekMethod method)
{
	Sint64 pos;

	if (hfile == NULL) {
		SDL_assert(false);
		return false;
	}

	switch (method) {
	case begin:
		pos = SDL_RWseek((SDL_RWops*)hfile, (Sint64)fpos, RW_SEEK_SET);
		if (pos >= 0) {
			return true;
		}
		break;

	// from current
	case current:
		pos = SDL_RWseek((SDL_RWops*)hfile, (Sint64)fpos, RW_SEEK_CUR);
		if (pos >= 0) {
			return true;
		}
		break;

	// default
	default:
		break;
	}

	// error
	return false;
}

#endif // SDL
