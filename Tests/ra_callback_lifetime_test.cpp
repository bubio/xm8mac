#include "ra_callback_lifetime.h"

#include <atomic>
#include <cstdlib>
#include <iostream>
#include <thread>

namespace {

int failures = 0;

void Check(bool condition, const char *message)
{
	if (!condition) {
		std::cerr << "FAIL: " << message << '\n';
		failures++;
	}
}

} // namespace

int main()
{
	for (int iteration = 0; iteration < 200; ++iteration) {
		int owner = iteration;
		Xm8Ra::RaCallbackLifetime lifetime(&owner);
		Xm8Ra::RaCallbackLifetime::Lease callback = lifetime.TryAcquire();
		Check(static_cast<bool>(callback), "callback lease acquired");
		Check(callback.Owner<int>() == &owner, "lease retains owner pointer");

		std::atomic<bool> close_started(false);
		std::atomic<bool> close_finished(false);
		std::thread closer([&]() {
			close_started.store(true, std::memory_order_release);
			lifetime.CloseAndWait();
			close_finished.store(true, std::memory_order_release);
		});

		while (!close_started.load(std::memory_order_acquire)) {
			std::this_thread::yield();
		}
		// Wait until CloseAndWait has closed the gate. Temporary leases are
		// immediately released, allowing the closer to acquire the mutex.
		while (lifetime.TryAcquire()) {
			std::this_thread::yield();
		}
		Check(!close_finished.load(std::memory_order_acquire),
			"close waits for callback already in flight");

		callback = Xm8Ra::RaCallbackLifetime::Lease();
		closer.join();
		Check(close_finished.load(std::memory_order_acquire),
			"close completes after callback release");
		Check(!lifetime.TryAcquire(), "closed gate rejects late callback");
	}

	if (failures != 0) return EXIT_FAILURE;
	std::cout << "RA callback lifetime tests passed\n";
	return EXIT_SUCCESS;
}
