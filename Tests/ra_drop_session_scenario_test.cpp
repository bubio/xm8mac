#include "ra_disk_transaction.h"
#include "ra_media_change_policy.h"

#include <cstdlib>
#include <iostream>

namespace {
void Check(bool value, const char *message)
{
	if (!value) { std::cerr << "FAIL: " << message << '\n'; std::exit(EXIT_FAILURE); }
}
}

int main()
{
	using namespace Xm8Ra;
	// RA ON + two-drive D&D: Drive 1 starts the anchor and Drive 2 becomes
	// part of that request, rather than a competing Starting-state request.
	RaDiskPolicyContext drive1;
	drive1.ra_enabled = true;
	drive1.session_state = RaSessionState::Ready;
	drive1.role = RaDiskRole::Anchor;
	Check(ClassifyRaDiskAction(drive1) == RaDiskAction::BeginAnchorLaunch,
		"RA ON drop starts Drive 1 anchor");
	Check(CanAttachDrive2ToAnchorLaunch(true, true, false),
		"Drive 2 attaches to the same drop transaction");
	Check(!CanAttachDrive2ToAnchorLaunch(true, true, true),
		"a second transaction is still rejected");

	// RA ON -> mount -> RA OFF -> replacement must become an ordinary local
	// mount, independent of the previous RA session.
	RaDiskPolicyContext after_mode_off;
	after_mode_off.ra_enabled = false;
	after_mode_off.session_state = RaSessionState::Ready;
	after_mode_off.role = RaDiskRole::Anchor;
	Check(ClassifyRaDiskAction(after_mode_off) == RaDiskAction::MountLocal,
		"RA OFF replacement does not retain an RA transaction");
	return EXIT_SUCCESS;
}
