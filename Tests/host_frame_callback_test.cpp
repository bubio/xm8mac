#include "host_frame_callback.h"

#include <cstdlib>
#include <iostream>

namespace {

void CountFrame(void *userdata)
{
	int *count = static_cast<int *>(userdata);
	(*count)++;
}

} // namespace

int main()
{
	HostFrameCallback callback;
	int first_count = 0;
	for (int i = 0; i < 3; i++) {
		callback.notify();
	}
	if (first_count != 0) {
		std::cerr << "unset callback was invoked\n";
		return EXIT_FAILURE;
	}

	callback.set(CountFrame, &first_count);
	for (int i = 0; i < 3; i++) {
		callback.notify();
	}
	if (first_count != 3) {
		std::cerr << "callback was not invoked once per frame\n";
		return EXIT_FAILURE;
	}

	int second_count = 0;
	callback.set(CountFrame, &second_count);
	callback.notify();
	if (first_count != 3 || second_count != 1) {
		std::cerr << "callback rebind retained stale userdata\n";
		return EXIT_FAILURE;
	}

	callback.set(nullptr, nullptr);
	callback.notify();
	if (second_count != 1) {
		std::cerr << "cleared callback was invoked\n";
		return EXIT_FAILURE;
	}

	std::cout << "Host frame callback tests passed\n";
	return EXIT_SUCCESS;
}
