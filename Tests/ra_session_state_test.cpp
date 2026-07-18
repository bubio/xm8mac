#include "ra_session_state.h"
#include "ra_connectivity.h"

#include <cstdlib>
#include <iostream>

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
	using Xm8Ra::IsRaSessionEvaluating;
	using Xm8Ra::IsRaSessionOffline;
	using Xm8Ra::RaSessionSignal;
	using Xm8Ra::RaSessionState;
	using Xm8Ra::TransitionRaSession;
	using Xm8Ra::RaReachabilityState;

	RaSessionState state = RaSessionState::Ready;
	state = TransitionRaSession(state, RaSessionSignal::BeginLaunch);
	Check(state == RaSessionState::Starting, "launch enters Starting");
	state = TransitionRaSession(state, RaSessionSignal::LaunchFailed);
	Check(IsRaSessionOffline(state), "launch failure enters Offline");
	Check(!IsRaSessionEvaluating(state), "Offline never evaluates frames");
	state = TransitionRaSession(state, RaSessionSignal::LaunchSucceeded);
	Check(IsRaSessionOffline(state), "late launch success cannot revive Offline");
	state = TransitionRaSession(state, RaSessionSignal::Reconnected);
	Check(IsRaSessionOffline(state), "reconnect cannot revive Offline");
	state = TransitionRaSession(state, RaSessionSignal::StopGame);
	Check(state == RaSessionState::Ready, "stopping game clears Offline");

	state = TransitionRaSession(state, RaSessionSignal::BeginLaunch);
	state = TransitionRaSession(state, RaSessionSignal::LaunchSucceeded);
	Check(state == RaSessionState::Active, "successful launch enters Active");
	Check(IsRaSessionEvaluating(state), "Active evaluates frames");
	state = TransitionRaSession(state, RaSessionSignal::Disconnected);
	Check(state == RaSessionState::ActiveDisconnected,
		"connection loss preserves active session");
	Check(IsRaSessionEvaluating(state),
		"ActiveDisconnected continues frame evaluation");
	state = TransitionRaSession(state, RaSessionSignal::Disconnected);
	Check(state == RaSessionState::ActiveDisconnected,
		"duplicate disconnect signal does not change state");
	state = TransitionRaSession(state, RaSessionSignal::Reconnected);
	Check(state == RaSessionState::Active, "reconnect restores Active");
	state = TransitionRaSession(state, RaSessionSignal::Reconnected);
	Check(state == RaSessionState::Active,
		"duplicate reconnect signal does not change state");
	state = TransitionRaSession(state, RaSessionSignal::SessionInvalidated);
	Check(IsRaSessionOffline(state), "invalid active session enters Offline");

	Xm8Ra::RaConnectivityTracker connectivity;
	auto reachability = connectivity.Observe(RaReachabilityState::Reachable);
	Check(!reachability.has_signal,
		"initial reachability establishes baseline without notification");
	reachability = connectivity.Observe(RaReachabilityState::Reachable);
	Check(!reachability.has_signal,
		"duplicate reachable observation is suppressed");
	reachability = connectivity.Observe(RaReachabilityState::Unreachable);
	Check(reachability.has_signal &&
		reachability.signal == RaSessionSignal::Disconnected,
		"reachable to unreachable emits disconnect once");
	reachability = connectivity.Observe(RaReachabilityState::Unknown);
	Check(!reachability.has_signal &&
		connectivity.State() == RaReachabilityState::Unreachable,
		"unknown observation preserves last reliable state");
	reachability = connectivity.Observe(RaReachabilityState::Unreachable);
	Check(!reachability.has_signal,
		"duplicate unreachable observation is suppressed");
	reachability = connectivity.Observe(RaReachabilityState::Reachable);
	Check(reachability.has_signal &&
		reachability.signal == RaSessionSignal::Reconnected,
		"unreachable to reachable emits reconnect once");

	if (failures != 0) {
		return EXIT_FAILURE;
	}
	std::cout << "RA session state tests passed\n";
	return EXIT_SUCCESS;
}
