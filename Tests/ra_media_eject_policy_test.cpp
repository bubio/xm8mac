#include "ra_media_change_policy.h"

#include <cstdlib>
#include <iostream>

int main()
{
	bool ok = true;
	auto check = [&ok](bool condition, const char *message) {
		if (!condition) {
			std::cerr << "FAIL: " << message << '\n';
			ok = false;
		}
	};

	check(Xm8Ra::CanEjectRaMedia(0, false, false),
		"idle anchor may be ejected");
	check(!Xm8Ra::CanEjectRaMedia(0, true, false),
		"anchor eject is blocked during game identification");
	check(!Xm8Ra::CanEjectRaMedia(0, false, true),
		"anchor eject is blocked during media change");
	check(Xm8Ra::CanEjectRaMedia(1, true, true),
		"auxiliary eject never depends on RA anchor operations");

	return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
