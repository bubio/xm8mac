#include "ra_connectivity.h"

#include "xm8jni.h"

#include <chrono>
#include <memory>

namespace Xm8Ra {
namespace {
class RaAndroidConnectivityMonitor final : public RaConnectivityMonitor {
public:
	RaReachabilityState Poll() override
	{
		const auto now = std::chrono::steady_clock::now();
		if (has_polled_ && now < next_poll_) return cached_;
		has_polled_ = true;
		next_poll_ = now + std::chrono::milliseconds(500);
		cached_ = Android_RaHasNetwork() ? RaReachabilityState::Reachable :
			RaReachabilityState::Unreachable;
		return cached_;
	}
private:
	RaReachabilityState cached_ = RaReachabilityState::Unknown;
	std::chrono::steady_clock::time_point next_poll_;
	bool has_polled_ = false;
};
}
std::unique_ptr<RaConnectivityMonitor> CreateAndroidRaConnectivityMonitor()
{
	return std::unique_ptr<RaConnectivityMonitor>(new RaAndroidConnectivityMonitor());
}
} // namespace Xm8Ra
