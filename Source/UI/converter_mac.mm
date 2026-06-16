#import <Foundation/Foundation.h>

#include <cstring>

#include "converter_mac.h"

int NormalizeUtf8ToNfcMac(const char *src, char *dst, size_t capacity)
{
	if (src == nullptr || dst == nullptr || capacity == 0) {
		return -1;
	}

	@autoreleasepool {
		NSString *string = [NSString stringWithUTF8String:src];
		if (string == nil) {
			dst[0] = '\0';
			return -1;
		}

		NSString *normalized =
			[string precomposedStringWithCanonicalMapping];
		const char *utf8 = [normalized UTF8String];
		if (utf8 == nullptr) {
			dst[0] = '\0';
			return -1;
		}

		const size_t length = std::strlen(utf8);
		if (length >= capacity) {
			dst[0] = '\0';
			return -1;
		}

		std::memcpy(dst, utf8, length + 1);
	}

	return 0;
}
