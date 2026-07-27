#include "ra_media_change_policy.h"

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
	using Xm8Ra::ClassifyMediaChange;
	using Xm8Ra::PlanRaMediaMount;
	using Xm8Ra::RaDrive2MountAction;
	using Xm8Ra::RaMediaChangeAction;
	const std::string old_hash(32, 'a');
	const std::string new_hash(32, 'b');

	Check(ClassifyMediaChange(0, true, false, false, 7, old_hash, 7, new_hash) ==
		RaMediaChangeAction::BeginSameGameChange,
		"same-game Drive 1 media starts RA change");
	Check(ClassifyMediaChange(0, true, true, false, 7, old_hash, 7, new_hash) ==
		RaMediaChangeAction::RejectPending,
		"second media change is rejected while pending");
	Check(ClassifyMediaChange(0, true, false, false, 7, old_hash, 8, new_hash) ==
		RaMediaChangeAction::RejectDifferentGame,
		"different-game media is rejected");
	Check(ClassifyMediaChange(0, true, false, false, 0, old_hash, 7, new_hash) ==
		RaMediaChangeAction::RejectDifferentGame,
		"media change requires known active library ownership");
	Check(ClassifyMediaChange(1, true, false, false, 7, old_hash, 8, new_hash) ==
		RaMediaChangeAction::NoChange,
		"Drive 2 never changes active RA media");
	Check(ClassifyMediaChange(0, true, false, false, 7, old_hash, 7, old_hash) ==
		RaMediaChangeAction::NoChange,
		"same RA hash does not invoke media change");
	Check(ClassifyMediaChange(0, true, false, true, 7, old_hash, 7, new_hash) ==
		RaMediaChangeAction::NoChange,
		"bank switching in one working D88 does not change RA media");
	Check(ClassifyMediaChange(0, false, false, false, 0, "", 7, new_hash) ==
		RaMediaChangeAction::NoChange,
		"initial media load is not classified as media change");

	const Xm8Ra::RaMediaMountPlan pending_pair =
		PlanRaMediaMount(true, true, 2);
	Check(pending_pair.wait_for_ra_approval,
		"same-game pair waits for RA before either drive changes");
	Check(pending_pair.drive2_action_after_approval ==
		RaDrive2MountAction::OpenBank1,
		"two-bank pair opens Drive 2 only after approval");
	const Xm8Ra::RaMediaMountPlan pending_single_bank =
		PlanRaMediaMount(true, true, 1);
	Check(pending_single_bank.wait_for_ra_approval &&
		pending_single_bank.drive2_action_after_approval ==
			RaDrive2MountAction::Close,
		"single-bank pair defers Drive 2 close until approval");
	const Xm8Ra::RaMediaMountPlan direct_pair =
		PlanRaMediaMount(false, true, 2);
	Check(!direct_pair.wait_for_ra_approval &&
		direct_pair.drive2_action_after_approval ==
			RaDrive2MountAction::OpenBank1,
		"non-media-change pair applies both drives directly");
	const Xm8Ra::RaMediaMountPlan single_drive =
		PlanRaMediaMount(true, false, 2);
	Check(single_drive.wait_for_ra_approval &&
		single_drive.drive2_action_after_approval ==
			RaDrive2MountAction::Unchanged,
		"single-drive media change never mutates Drive 2");
	Check(Xm8Ra::RaMediaRollbackRestoredAllDrives(false, true, false),
		"single-drive rollback ignores Drive 2");
	Check(!Xm8Ra::RaMediaRollbackRestoredAllDrives(true, true, false),
		"paired rollback fails when Drive 2 was not restored");
	Check(Xm8Ra::RaMediaRollbackRestoredAllDrives(true, true, true),
		"paired rollback requires both drives");

	if (failures != 0) {
		return EXIT_FAILURE;
	}
	std::cout << "RA media change policy tests passed\n";
	return EXIT_SUCCESS;
}
