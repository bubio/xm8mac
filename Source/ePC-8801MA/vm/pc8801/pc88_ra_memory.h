#ifndef PC88_RA_MEMORY_H
#define PC88_RA_MEMORY_H

#include "../../common.h"

#include <cstddef>
#include <cstring>

inline size_t ReadPc88RaInspectionMemory(const uint8 *ram, const uint8 *tvram,
	uint32 addr, uint8 *buffer, size_t count)
{
	if (ram == NULL || tvram == NULL || buffer == NULL) {
		return 0;
	}

	size_t copied = 0;
	while (copied < count) {
		if (copied > 0xffffffffu - addr) {
			break;
		}

		const uint32 current = addr + static_cast<uint32>(copied);
		const uint8 *source = NULL;
		size_t available = 0;

		if (current < 0x10000) {
			source = &ram[current];
			available = 0x10000 - current;
		}
		else if (current < 0x11000) {
			source = &tvram[current - 0x10000];
			available = 0x11000 - current;
		}
		else {
			break;
		}

		const size_t remaining = count - copied;
		const size_t chunk = available < remaining ? available : remaining;
		memcpy(buffer + copied, source, chunk);
		copied += chunk;
	}
	return copied;
}

#endif
