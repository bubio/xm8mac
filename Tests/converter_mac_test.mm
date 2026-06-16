#import <Foundation/Foundation.h>

#include <cstring>
#include <iostream>

#include "converter_mac.h"

namespace {

int failures = 0;

void Check(bool condition, const char *message)
{
	if (!condition) {
		std::cerr << "FAIL: " << message << '\n';
		failures++;
	}
}

}

int main()
{
	@autoreleasepool {
		char normalized[32];
		Check(NormalizeUtf8ToNfcMac("ka", normalized,
			sizeof(normalized)) == 0, "normalize ascii succeeds");
		Check(std::strcmp(normalized, "ka") == 0,
			"ascii remains unchanged");

		const char *nfd = "\xe3\x81\x8b\xe3\x82\x99";
		const char *nfc = "\xe3\x81\x8c";
		Check(NormalizeUtf8ToNfcMac(nfd, normalized,
			sizeof(normalized)) == 0, "normalize nfd succeeds");
		Check(std::strcmp(normalized, nfc) == 0,
			"nfd kana becomes nfc kana");

		char too_small[3] = {'x', 'y', 'z'};
		Check(NormalizeUtf8ToNfcMac(nfd, too_small,
			sizeof(too_small)) == -1, "small buffer fails");
		Check(too_small[0] == '\0',
			"small buffer is cleared after failure");

		Check(NormalizeUtf8ToNfcMac(nullptr, normalized,
			sizeof(normalized)) == -1, "null source fails");
		Check(NormalizeUtf8ToNfcMac("ka", nullptr,
			sizeof(normalized)) == -1, "null destination fails");
		Check(NormalizeUtf8ToNfcMac("ka", normalized, 0) == -1,
			"zero capacity fails");

		const char invalid[] = {static_cast<char>(0xff), '\0'};
		Check(NormalizeUtf8ToNfcMac(invalid, normalized,
			sizeof(normalized)) == -1, "invalid utf-8 fails");
	}

	return failures == 0 ? 0 : 1;
}
