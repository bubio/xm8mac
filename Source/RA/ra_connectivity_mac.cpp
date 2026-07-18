#include "ra_connectivity.h"

#include <SystemConfiguration/SystemConfiguration.h>

#include <chrono>
#include <cstring>
#include <netinet/in.h>

namespace Xm8Ra {

namespace {

class RaMacConnectivityMonitor final : public RaConnectivityMonitor {
public:
	RaMacConnectivityMonitor()
	{
		sockaddr_in address;
		std::memset(&address, 0, sizeof(address));
		address.sin_len = sizeof(address);
		address.sin_family = AF_INET;
		reachability_ = SCNetworkReachabilityCreateWithAddress(nullptr,
			reinterpret_cast<const sockaddr *>(&address));
	}

	~RaMacConnectivityMonitor() override
	{
		if (reachability_ != nullptr) {
			CFRelease(reachability_);
		}
	}

	RaReachabilityState Poll() override
	{
		const auto now = std::chrono::steady_clock::now();
		if (has_polled_ && now < next_poll_) {
			return cached_;
		}
		has_polled_ = true;
		next_poll_ = now + std::chrono::milliseconds(500);
		if (reachability_ == nullptr) {
			cached_ = RaReachabilityState::Unknown;
			return cached_;
		}

		SCNetworkReachabilityFlags flags = 0;
		if (!SCNetworkReachabilityGetFlags(reachability_, &flags)) {
			cached_ = RaReachabilityState::Unknown;
			return cached_;
		}
		const bool reachable =
			(flags & kSCNetworkReachabilityFlagsReachable) != 0;
		const bool connection_required =
			(flags & kSCNetworkReachabilityFlagsConnectionRequired) != 0;
		cached_ = reachable && !connection_required ?
			RaReachabilityState::Reachable :
			RaReachabilityState::Unreachable;
		return cached_;
	}

private:
	SCNetworkReachabilityRef reachability_ = nullptr;
	RaReachabilityState cached_ = RaReachabilityState::Unknown;
	std::chrono::steady_clock::time_point next_poll_;
	bool has_polled_ = false;
};

} // namespace

std::unique_ptr<RaConnectivityMonitor> CreateMacRaConnectivityMonitor()
{
	return std::unique_ptr<RaConnectivityMonitor>(
		new RaMacConnectivityMonitor());
}

} // namespace Xm8Ra
