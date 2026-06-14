#ifdef SDL

#include "os.h"
#include "common.h"
#include "fileio.h"
#include "d88probe.h"

bool ProbeD88Image(const char *filename, int *banks, size_t *name_length)
{
	FILEIO fio;
	Uint8 header[0x2b0];
	Uint32 offset;
	Uint32 add;
	size_t len;
	int count;

	if (filename == NULL || banks == NULL ||
		strlen(filename) >= (_MAX_PATH * 3)) {
		return false;
	}
	if (fio.Fopen((_TCHAR*)filename, FILEIO_READ_BINARY) == false) {
		return false;
	}

	count = 0;
	offset = 0;
	len = 0;
	for (;;) {
		if (fio.Fread(header, 1, sizeof(header)) != sizeof(header)) {
			break;
		}

		if ((header[0x23] != 0x00) ||
			(header[0x22] != 0x00) ||
			(header[0x21] != 0x02) ||
			((header[0x20] & 0x0f) != 0x00)) {
			if (header[0x21] != 0x00) {
				break;
			}
		}

		add = (Uint32)header[0x1f];
		add <<= 8;
		add |= (Uint32)header[0x1e];
		add <<= 8;
		add |= (Uint32)header[0x1d];
		add <<= 8;
		add |= (Uint32)header[0x1c];
		if (add < sizeof(header) || offset > 0xffffffffU - add) {
			break;
		}

		header[0x10] = 0x00;
		len += strlen((const char*)header) + 1;
		count++;
		offset += add;
		fio.Fseek((long)offset, FILEIO_SEEK_SET);
	}

	fio.Fclose();
	if (count == 0) {
		return false;
	}
	*banks = count;
	if (name_length != NULL) {
		*name_length = len;
	}
	return true;
}

#endif // SDL
