#include "ra_session_policy.h"

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
	using namespace Xm8Ra;
	RaSessionPolicyContext context;

	Check(EffectiveRaMode(context) == RaEffectiveMode::Normal,
		"RA disabled is Normal");
	context.ra_enabled = true;
	context.selected_mode = RaPlayMode::Hardcore;
	Check(EffectiveRaMode(context) == RaEffectiveMode::Normal,
		"Hardcore preference without a game does not restrict Normal operation");

	context.session_state = RaSessionState::Starting;
	Check(IsRaHardcoreSession(context), "Hardcore applies while starting");
	for (RaRestrictedOperation operation : {
		RaRestrictedOperation::LoadState, RaRestrictedOperation::SaveState,
		RaRestrictedOperation::FullSpeed, RaRestrictedOperation::FastDisk,
		RaRestrictedOperation::Debugger}) {
		Check(!IsRaOperationAllowed(context, operation),
			"Hardcore rejects every restricted operation");
	}

	context.selected_mode = RaPlayMode::Casual;
	Check(IsRaOperationAllowed(context, RaRestrictedOperation::LoadState),
		"Casual allows state loading");
	Check(IsRaOperationAllowed(context, RaRestrictedOperation::FullSpeed),
		"Casual allows full speed");
	Check(!IsRaOperationAllowed(context, RaRestrictedOperation::FastDisk),
		"Casual rejects pseudo fast disk");
	Check(!IsRaOperationAllowed(context, RaRestrictedOperation::Debugger),
		"Casual rejects debugger operations");

	context.selected_mode = RaPlayMode::Hardcore;
	context.session_state = RaSessionState::ActiveDisconnected;
	Check(IsRaHardcoreSession(context),
		"disconnect does not weaken an active Hardcore session");
	context.session_state = RaSessionState::Offline;
	Check(EffectiveRaMode(context) == RaEffectiveMode::Offline,
		"invalidated session is Offline");
	Check(IsRaOperationAllowed(context, RaRestrictedOperation::LoadState),
		"Offline allows its dedicated states");
	Check(IsRaOperationAllowed(context, RaRestrictedOperation::FullSpeed),
		"Offline restores normal speed policy");

	if (failures != 0) return EXIT_FAILURE;
	std::cout << "RA session policy tests passed\n";
	return EXIT_SUCCESS;
}
