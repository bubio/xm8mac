#include "ra_connectivity.h"

#include <windows.h>
#include <wininet.h>

#include <chrono>
#include <memory>

namespace Xm8Ra {

namespace {

class RaWinConnectivityMonitor final : public RaConnectivityMonitor {
public:
	RaReachabilityState Poll() override
	{
		const auto now = std::chrono::steady_clock::now();
		if (has_polled_ && now < next_poll_) {
			return cached_;
		}
		has_polled_ = true;
		next_poll_ = now + std::chrono::milliseconds(500);

		DWORD flags = 0;
		cached_ = InternetGetConnectedState(&flags, 0) != FALSE ?
			RaReachabilityState::Reachable :
			RaReachabilityState::Unreachable;
		return cached_;
	}

private:
	RaReachabilityState cached_ = RaReachabilityState::Unknown;
	std::chrono::steady_clock::time_point next_poll_;
	bool has_polled_ = false;
};

} // namespace

std::unique_ptr<RaConnectivityMonitor> CreateWinRaConnectivityMonitor()
{
	return std::unique_ptr<RaConnectivityMonitor>(
		new RaWinConnectivityMonitor());
}

} // namespace Xm8Ra
