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
	using Xm8Ra::CanApplySequentialRaMediaBatch;
	using Xm8Ra::ClassifyRaDiskAction;
	using Xm8Ra::PlanRaMediaMount;
	using Xm8Ra::RaDiskAction;
	using Xm8Ra::RaDiskPolicyContext;
	using Xm8Ra::RaDiskRole;
	using Xm8Ra::RaDiskState;
	using Xm8Ra::RaDiskTransitionInput;
	using Xm8Ra::RaDiskTrigger;
	using Xm8Ra::RaDrive2MountAction;
	const std::string old_hash(32, 'a');
	const std::string new_hash(32, 'b');

	RaDiskPolicyContext context;
	context.ra_enabled = true;
	context.role = RaDiskRole::Anchor;
	context.active_game_loaded = true;
	context.active_library_game_id = 7;
	context.active_hash = old_hash;
	context.target_library_game_id = 7;
	context.target_hash = new_hash;
	Check(ClassifyRaDiskAction(context) == RaDiskAction::ChangeAnchorMedia,
		"same-game media starts RA change");
	context.anchor_change_pending = true;
	Check(ClassifyRaDiskAction(context) == RaDiskAction::RejectBusy,
		"second media change is rejected while pending");
	context.anchor_change_pending = false;
	context.target_library_game_id = 8;
	Check(ClassifyRaDiskAction(context) == RaDiskAction::RestartAnchorLaunch,
		"different-game anchor starts a new session");
	context.target_library_game_id = 7;
	context.target_hash = old_hash;
	Check(ClassifyRaDiskAction(context) == RaDiskAction::MountNormal,
		"same RA hash does not invoke media change");
	context.active_game_loaded = false;
	context.target_hash = new_hash;
	Check(ClassifyRaDiskAction(context) == RaDiskAction::BeginAnchorLaunch,
		"initial anchor begins game identification");

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
		"non-media-change pair still plans the auxiliary bank");
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

	Check(CanApplySequentialRaMediaBatch(false, 2, false),
		"multi-drive batch is allowed before an online session");
	Check(!CanApplySequentialRaMediaBatch(true, 2, false),
		"online session rejects two independent drive changes");
	Check(!CanApplySequentialRaMediaBatch(true, 1, true),
		"online session rejects changing one drive while closing the other");
	Check(CanApplySequentialRaMediaBatch(true, 1, false),
		"online session allows one independent drive change");

	Check(Xm8Ra::ShouldOpenDroppedD88AsPair(false),
		"raw D88 drop is one paired transaction even with one bank");
	Check(!Xm8Ra::ShouldOpenDroppedD88AsPair(true),
		"playlist drop retains its explicit drive assignments");

	struct TransitionCase {
		const char *name;
		RaDiskState state;
		RaDiskTrigger trigger;
		bool same_container;
		bool same_game;
		bool same_hash;
		bool verified;
		bool network;
		bool hardcore_selected;
		RaDiskAction action;
		RaDiskState next;
	};
	const TransitionCase transition_cases[] = {
		{"normal ignores RA", RaDiskState::Normal,
			RaDiskTrigger::MountAuxiliary, false, false, false, false, false,
			false, RaDiskAction::MountNormal, RaDiskState::Normal},
		{"offline mounts auxiliary", RaDiskState::Offline,
			RaDiskTrigger::MountAuxiliary, false, false, false, false, false,
			true, RaDiskAction::MountAuxiliary, RaDiskState::Offline},
		{"ready starts anchor", RaDiskState::Ready,
			RaDiskTrigger::MountAnchor, false, false, false, false, false,
			true, RaDiskAction::BeginAnchorLaunch, RaDiskState::Starting},
		{"starting queues hardcore auxiliary", RaDiskState::Starting,
			RaDiskTrigger::MountAuxiliary, false, true, false, false, true,
			true, RaDiskAction::VerifyAuxiliary, RaDiskState::Pending},
		{"starting offline does not wait for auxiliary verification",
			RaDiskState::Starting, RaDiskTrigger::MountAuxiliary, false, true,
			false, false, false, true,
			RaDiskAction::EnterOfflineAndMount, RaDiskState::Offline},
		{"casual mounts same game auxiliary", RaDiskState::CasualActive,
			RaDiskTrigger::MountAuxiliary, false, true, false, false, true,
			false, RaDiskAction::MountAuxiliary, RaDiskState::CasualActive},
		{"casual rejects other game auxiliary", RaDiskState::CasualActive,
			RaDiskTrigger::MountAuxiliary, false, false, false, false, true,
			false, RaDiskAction::RejectDifferentGame,
			RaDiskState::CasualActive},
		{"hardcore mounts verified auxiliary", RaDiskState::HardcoreActive,
			RaDiskTrigger::MountAuxiliary, false, true, false, true, false,
			true, RaDiskAction::MountAuxiliary, RaDiskState::HardcoreActive},
		{"hardcore verifies unknown auxiliary", RaDiskState::HardcoreActive,
			RaDiskTrigger::MountAuxiliary, false, true, false, false, true,
			true, RaDiskAction::VerifyAuxiliary, RaDiskState::Pending},
		{"hardcore offline fallback mounts", RaDiskState::HardcoreActive,
			RaDiskTrigger::MountAuxiliary, false, true, false, false, false,
			true, RaDiskAction::EnterOfflineAndMount, RaDiskState::Offline},
		{"same container anchor remains active", RaDiskState::HardcoreActive,
			RaDiskTrigger::ChangeAnchorBank, true, true, false, false, true,
			true, RaDiskAction::MountNormal, RaDiskState::HardcoreActive},
		{"same game anchor starts media change", RaDiskState::HardcoreActive,
			RaDiskTrigger::MountAnchor, false, true, false, false, true,
			true, RaDiskAction::ChangeAnchorMedia, RaDiskState::Pending},
		{"different anchor restarts", RaDiskState::HardcoreActive,
			RaDiskTrigger::MountAnchor, false, false, false, false, true,
			true, RaDiskAction::RestartAnchorLaunch, RaDiskState::Starting},
		{"pending rejects mutation", RaDiskState::Pending,
			RaDiskTrigger::ChangeAuxiliaryBank, false, true, false, false, true,
			true, RaDiskAction::RejectBusy, RaDiskState::Pending},
	};
	for (const TransitionCase& item : transition_cases) {
		RaDiskTransitionInput input;
		input.state = item.state;
		input.trigger = item.trigger;
		input.same_media_container = item.same_container;
		input.same_local_game = item.same_game;
		input.same_hash = item.same_hash;
		input.auxiliary_hash_verified = item.verified;
		input.network_available = item.network;
		input.hardcore_selected = item.hardcore_selected;
		const Xm8Ra::RaDiskTransition transition =
			Xm8Ra::TransitionRaDisk(input);
		Check(transition.action == item.action, item.name);
		Check(transition.next_state == item.next, item.name);
	}

	if (failures != 0) {
		return EXIT_FAILURE;
	}
	std::cout << "RA media change policy tests passed\n";
	return EXIT_SUCCESS;
}
