#include "ra_connectivity.h"

#include <ifaddrs.h>
#include <net/if.h>
#include <sys/socket.h>

#include <chrono>
#include <memory>

namespace Xm8Ra {

namespace {

class RaLinuxConnectivityMonitor final : public RaConnectivityMonitor {
public:
	RaReachabilityState Poll() override
	{
		const auto now = std::chrono::steady_clock::now();
		if (has_polled_ && now < next_poll_) {
			return cached_;
		}
		has_polled_ = true;
		next_poll_ = now + std::chrono::milliseconds(500);

		ifaddrs *interfaces = nullptr;
		if (getifaddrs(&interfaces) != 0) {
			cached_ = RaReachabilityState::Unknown;
			return cached_;
		}

		cached_ = RaReachabilityState::Unreachable;
		for (const ifaddrs *item = interfaces; item != nullptr;
			item = item->ifa_next) {
			if (item->ifa_addr == nullptr ||
				(item->ifa_flags & IFF_UP) == 0 ||
				(item->ifa_flags & IFF_LOOPBACK) != 0) {
				continue;
			}
			const int family = item->ifa_addr->sa_family;
			if (family == AF_INET || family == AF_INET6) {
				cached_ = RaReachabilityState::Reachable;
				break;
			}
		}
		freeifaddrs(interfaces);
		return cached_;
	}

private:
	RaReachabilityState cached_ = RaReachabilityState::Unknown;
	std::chrono::steady_clock::time_point next_poll_;
	bool has_polled_ = false;
};

} // namespace

std::unique_ptr<RaConnectivityMonitor> CreateLinuxRaConnectivityMonitor()
{
	return std::unique_ptr<RaConnectivityMonitor>(
		new RaLinuxConnectivityMonitor());
}

} // namespace Xm8Ra
