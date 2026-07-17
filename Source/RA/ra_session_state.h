#ifndef XM8_RA_SESSION_STATE_H
#define XM8_RA_SESSION_STATE_H

namespace Xm8Ra {

enum class RaSessionState {
	Ready,
	Starting,
	Active,
	ActiveDisconnected,
	Offline,
};

enum class RaSessionSignal {
	BeginLaunch,
	LaunchSucceeded,
	LaunchFailed,
	Disconnected,
	Reconnected,
	SessionInvalidated,
	StopGame,
};

inline RaSessionState TransitionRaSession(RaSessionState state,
	RaSessionSignal signal)
{
	if (signal == RaSessionSignal::StopGame) {
		return RaSessionState::Ready;
	}

	switch (state) {
	case RaSessionState::Ready:
		return signal == RaSessionSignal::BeginLaunch ?
			RaSessionState::Starting : state;
	case RaSessionState::Starting:
		if (signal == RaSessionSignal::LaunchSucceeded) {
			return RaSessionState::Active;
		}
		if (signal == RaSessionSignal::LaunchFailed ||
			signal == RaSessionSignal::SessionInvalidated) {
			return RaSessionState::Offline;
		}
		return state;
	case RaSessionState::Active:
		if (signal == RaSessionSignal::Disconnected) {
			return RaSessionState::ActiveDisconnected;
		}
		if (signal == RaSessionSignal::SessionInvalidated) {
			return RaSessionState::Offline;
		}
		return state;
	case RaSessionState::ActiveDisconnected:
		if (signal == RaSessionSignal::Reconnected) {
			return RaSessionState::Active;
		}
		if (signal == RaSessionSignal::SessionInvalidated) {
			return RaSessionState::Offline;
		}
		return state;
	case RaSessionState::Offline:
		// A network response or login must not revive the current game.
		return state;
	}
	return state;
}

inline bool IsRaSessionOffline(RaSessionState state)
{
	return state == RaSessionState::Offline;
}

inline bool IsRaSessionEvaluating(RaSessionState state)
{
	return state == RaSessionState::Active ||
		state == RaSessionState::ActiveDisconnected;
}

} // namespace Xm8Ra

#endif
