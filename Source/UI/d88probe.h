#ifndef D88PROBE_H
#define D88PROBE_H

#include <cstddef>

bool ProbeD88Image(const char *filename, int *banks,
	size_t *name_length = nullptr);

#endif
