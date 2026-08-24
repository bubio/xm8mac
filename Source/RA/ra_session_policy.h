#ifndef XM8_RA_SESSION_POLICY_H
#define XM8_RA_SESSION_POLICY_H

#include "ra_session_state.h"

namespace Xm8Ra {

enum class RaPlayMode {
	Casual,
	Hardcore,
};

enum class RaEffectiveMode {
	Normal,
	Casual,
	Hardcore,
	Offline,
};

enum class RaRestrictedOperation {
	LoadState,
	SaveState,
	FullSpeed,
	FastDisk,
	Debugger,
};

struct RaSessionPolicyContext {
	bool ra_enabled = false;
	RaPlayMode selected_mode = RaPlayMode::Casual;
	RaSessionState session_state = RaSessionState::Ready;
};

inline RaEffectiveMode EffectiveRaMode(const RaSessionPolicyContext& context)
{
	if (!context.ra_enabled) {
		return RaEffectiveMode::Normal;
	}
	if (context.session_state == RaSessionState::Offline) {
		return RaEffectiveMode::Offline;
	}
	if (context.session_state == RaSessionState::Starting ||
		context.session_state == RaSessionState::Active ||
		context.session_state == RaSessionState::ActiveDisconnected) {
		return context.selected_mode == RaPlayMode::Hardcore ?
			RaEffectiveMode::Hardcore : RaEffectiveMode::Casual;
	}
	// RA is enabled, but no game session is currently enforcing a play mode.
	return RaEffectiveMode::Normal;
}

inline bool IsRaOperationAllowed(const RaSessionPolicyContext& context,
	RaRestrictedOperation operation)
{
	const RaEffectiveMode mode = EffectiveRaMode(context);
	if (mode == RaEffectiveMode::Hardcore) {
		// Hardcore states may be created for achievement debugging, but never
		// loaded. All other assisted operations remain unavailable.
		return operation == RaRestrictedOperation::SaveState;
	}
	if (mode == RaEffectiveMode::Casual) {
		return operation != RaRestrictedOperation::FastDisk &&
			operation != RaRestrictedOperation::Debugger;
	}
	return true;
}

inline bool IsRaOnlineSession(const RaSessionPolicyContext& context)
{
	const RaEffectiveMode mode = EffectiveRaMode(context);
	return mode == RaEffectiveMode::Casual ||
		mode == RaEffectiveMode::Hardcore;
}

inline bool IsRaHardcoreSession(const RaSessionPolicyContext& context)
{
	return EffectiveRaMode(context) == RaEffectiveMode::Hardcore;
}

inline bool CanLoadHardcoreDebugState(const RaSessionPolicyContext& context)
{
	return EffectiveRaMode(context) == RaEffectiveMode::Casual;
}

} // namespace Xm8Ra

#endif
