#ifndef XM8_RA_CONNECTIVITY_H
#define XM8_RA_CONNECTIVITY_H

#include "ra_session_state.h"

#include <memory>

namespace Xm8Ra {

enum class RaReachabilityState {
	Unknown,
	Reachable,
	Unreachable,
};

struct RaReachabilityTransition {
	bool has_signal = false;
	RaSessionSignal signal = RaSessionSignal::Disconnected;
};

class RaConnectivityTracker {
public:
	RaReachabilityTransition Observe(RaReachabilityState state)
	{
		RaReachabilityTransition result;
		if (state == RaReachabilityState::Unknown) {
			return result;
		}
		if (state_ != RaReachabilityState::Unknown && state_ != state) {
			result.has_signal = true;
			result.signal = state == RaReachabilityState::Reachable ?
				RaSessionSignal::Reconnected : RaSessionSignal::Disconnected;
		}
		state_ = state;
		return result;
	}

	RaReachabilityState State() const { return state_; }

private:
	RaReachabilityState state_ = RaReachabilityState::Unknown;
};

class RaConnectivityMonitor {
public:
	virtual ~RaConnectivityMonitor() = default;
	virtual RaReachabilityState Poll() = 0;
};

std::unique_ptr<RaConnectivityMonitor> CreatePlatformRaConnectivityMonitor();

} // namespace Xm8Ra

#endif
