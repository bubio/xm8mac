#ifndef PC88_RA_MEMORY_H
#define PC88_RA_MEMORY_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline size_t ReadPc88RaInspectionMemory(const uint8_t *ram,
	const uint8_t *tvram, uint32_t addr, uint8_t *buffer, size_t count)
{
	if (ram == NULL || tvram == NULL || buffer == NULL) {
		return 0;
	}

	size_t copied = 0;
	while (copied < count) {
		if (copied > 0xffffffffu - addr) {
			break;
		}

		const uint32_t current = addr + (uint32_t)copied;
		const uint8_t *source = NULL;
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

#ifdef __cplusplus
}
#endif

#endif
