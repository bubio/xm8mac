#ifndef XM8_RA_MEDIA_CHANGE_POLICY_H
#define XM8_RA_MEDIA_CHANGE_POLICY_H

#include <cstddef>
#include <cstdint>
#include <string>

namespace Xm8Ra {

enum class RaDiskRole {
	Anchor,
	Auxiliary,
};

enum class RaDiskLoginState {
	LoggedOut,
	LoginPending,
	LoggedIn,
	LoginFailed,
};

enum class RaDiskAction {
	MountNormal,
	MountAuxiliary,
	BeginAnchorLaunch,
	ChangeAnchorMedia,
	RestartAnchorLaunch,
	VerifyAuxiliary,
	EnterOfflineAndMount,
	RejectBusy,
	RejectDifferentGame,
};

// The complete disk-operation state seen by the pure transition policy.
// App owns the side effects, but it must not invent a result outside this
// table. Pending covers both anchor media change and auxiliary verification.
enum class RaDiskState {
	Normal,
	Offline,
	Ready,
	Starting,
	CasualActive,
	HardcoreActive,
	Pending,
};

enum class RaDiskTrigger {
	MountAnchor,
	MountAuxiliary,
	ChangeAnchorBank,
	ChangeAuxiliaryBank,
	EjectAnchor,
	EjectAuxiliary,
};

struct RaDiskTransitionInput {
	RaDiskState state = RaDiskState::Normal;
	RaDiskTrigger trigger = RaDiskTrigger::MountAnchor;
	bool same_media_container = false;
	bool same_local_game = false;
	bool same_hash = false;
	bool auxiliary_hash_verified = false;
	bool network_available = false;
	bool hardcore_selected = false;
};

struct RaDiskTransition {
	RaDiskAction action = RaDiskAction::MountNormal;
	RaDiskState next_state = RaDiskState::Normal;
};

inline bool IsAuxiliaryTrigger(RaDiskTrigger trigger)
{
	return trigger == RaDiskTrigger::MountAuxiliary ||
		trigger == RaDiskTrigger::ChangeAuxiliaryBank ||
		trigger == RaDiskTrigger::EjectAuxiliary;
}

inline bool IsEjectTrigger(RaDiskTrigger trigger)
{
	return trigger == RaDiskTrigger::EjectAnchor ||
		trigger == RaDiskTrigger::EjectAuxiliary;
}

// Normative state/trigger transition table. Local D88 probing and RA-library
// registration occur before this decision whenever RA is enabled. Normal is
// deliberately independent of every RA field.
inline RaDiskTransition TransitionRaDisk(const RaDiskTransitionInput& input)
{
	if (input.state == RaDiskState::Normal) {
		return {RaDiskAction::MountNormal, RaDiskState::Normal};
	}
	if (input.state == RaDiskState::Offline) {
		return {IsAuxiliaryTrigger(input.trigger) ?
			RaDiskAction::MountAuxiliary : RaDiskAction::MountNormal,
			RaDiskState::Offline};
	}
	if (input.state == RaDiskState::Pending) {
		return {RaDiskAction::RejectBusy, RaDiskState::Pending};
	}
	if (input.state == RaDiskState::Starting) {
		if (IsEjectTrigger(input.trigger)) {
			return {RaDiskAction::RejectBusy, RaDiskState::Starting};
		}
		if (IsAuxiliaryTrigger(input.trigger) && input.same_local_game) {
			if (!input.hardcore_selected || input.auxiliary_hash_verified) {
				return {RaDiskAction::MountAuxiliary, RaDiskState::Starting};
			}
			return input.network_available ?
				RaDiskTransition{RaDiskAction::VerifyAuxiliary,
					RaDiskState::Pending} :
				RaDiskTransition{RaDiskAction::EnterOfflineAndMount,
					RaDiskState::Offline};
		}
		return {RaDiskAction::RejectBusy, RaDiskState::Starting};
	}

	if (IsEjectTrigger(input.trigger)) {
		return {IsAuxiliaryTrigger(input.trigger) ?
			RaDiskAction::MountAuxiliary : RaDiskAction::MountNormal,
			input.state};
	}

	if (IsAuxiliaryTrigger(input.trigger)) {
		if (input.state == RaDiskState::Ready) {
			return {RaDiskAction::MountAuxiliary, RaDiskState::Ready};
		}
		if (input.state == RaDiskState::CasualActive) {
			return {input.same_local_game ? RaDiskAction::MountAuxiliary :
				RaDiskAction::RejectDifferentGame, input.state};
		}
		if (input.auxiliary_hash_verified) {
			return {RaDiskAction::MountAuxiliary, input.state};
		}
		if (input.network_available) {
			return {RaDiskAction::VerifyAuxiliary, RaDiskState::Pending};
		}
		return {RaDiskAction::EnterOfflineAndMount, RaDiskState::Offline};
	}

	if (input.state == RaDiskState::Ready) {
		return {RaDiskAction::BeginAnchorLaunch, RaDiskState::Starting};
	}
	if (input.same_media_container || input.same_hash) {
		return {RaDiskAction::MountNormal, input.state};
	}
	if (input.same_local_game) {
		return {RaDiskAction::ChangeAnchorMedia, RaDiskState::Pending};
	}
	return {RaDiskAction::RestartAnchorLaunch, RaDiskState::Starting};
}

struct RaDiskPolicyContext {
	bool ra_enabled = false;
	RaDiskLoginState login_state = RaDiskLoginState::LoggedOut;
	bool hardcore_selected = false;
	RaDiskRole role = RaDiskRole::Anchor;
	bool anchor_load_pending = false;
	bool anchor_change_pending = false;
	bool active_game_loaded = false;
	bool session_offline = false;
	bool auxiliary_validation_pending = false;
	bool auxiliary_hash_verified = false;
	bool network_available = false;
	bool same_media_container = false;
	int64_t active_library_game_id = 0;
	std::string active_hash;
	int64_t target_library_game_id = 0;
	std::string target_hash;
};

// Only Drive 1 owns the RA active-media identity. Drive 2 may require a
// same-game hash verification in Hardcore, but that verification never calls
// the active-media change operation.
inline RaDiskAction ClassifyRaDiskAction(const RaDiskPolicyContext& context)
{
	RaDiskState state = RaDiskState::Normal;
	if (context.ra_enabled) {
		if (context.session_offline) state = RaDiskState::Offline;
		else if (context.anchor_change_pending ||
			context.auxiliary_validation_pending) state = RaDiskState::Pending;
		else if (context.anchor_load_pending) state = RaDiskState::Starting;
		else if (!context.active_game_loaded) state = RaDiskState::Ready;
		else state = context.hardcore_selected ? RaDiskState::HardcoreActive :
			RaDiskState::CasualActive;
	}
	RaDiskTransitionInput input;
	input.state = state;
	input.trigger = context.role == RaDiskRole::Auxiliary ?
		RaDiskTrigger::MountAuxiliary : RaDiskTrigger::MountAnchor;
	input.same_media_container = context.same_media_container;
	input.same_local_game = context.active_library_game_id > 0 &&
		context.target_library_game_id == context.active_library_game_id;
	input.same_hash = !context.active_hash.empty() &&
		context.active_hash == context.target_hash;
	input.auxiliary_hash_verified = context.auxiliary_hash_verified;
	input.network_available = context.network_available;
	input.hardcore_selected = context.hardcore_selected;
	return TransitionRaDisk(input).action;
}

enum class RaDrive2MountAction {
	Unchanged,
	Close,
	OpenBank1,
};

struct RaMediaMountPlan {
	bool wait_for_ra_approval = false;
	RaDrive2MountAction drive2_action_after_approval =
		RaDrive2MountAction::Unchanged;
};

inline RaMediaMountPlan PlanRaMediaMount(bool same_game_change,
	bool open_pair, int target_banks)
{
	RaMediaMountPlan plan;
	plan.wait_for_ra_approval = same_game_change;
	if (open_pair) {
		plan.drive2_action_after_approval = target_banks > 1 ?
			RaDrive2MountAction::OpenBank1 : RaDrive2MountAction::Close;
	}
	return plan;
}

inline bool CanEjectRaMedia(int drive, bool game_load_pending,
	bool media_change_pending)
{
	return drive == 1 || (!game_load_pending && !media_change_pending);
}

// Launch-profile persistence is metadata for an auxiliary disk. A missing or
// incomplete anchor profile must never undo a successful Drive 2 VM mount.
inline bool MustPersistRaLaunchProfileForMount(RaDiskRole role)
{
	return role == RaDiskRole::Anchor;
}

// A raw D88 drop replaces the legacy Drive 1/Drive 2 pair even when the
// image has only one bank (in which case Drive 2 is closed). Treat it as one
// transaction so that the close also waits for RA approval.
inline bool ShouldOpenDroppedD88AsPair(bool is_playlist)
{
	return !is_playlist;
}

inline bool RaMediaRollbackRestoredAllDrives(bool open_pair,
	bool drive1_restored, bool drive2_restored)
{
	return drive1_restored && (!open_pair || drive2_restored);
}

// rcheevos exposes one current media hash. A paired open of banks from the
// same D88 is one anchor transaction, but a sequence of independent Drive 1
// changes cannot be made atomic around asynchronous RA approval. Reject such
// batches before either the VM or RA state is changed.
inline bool CanApplySequentialRaMediaBatch(bool online_session,
	size_t target_count, bool closes_drive2)
{
	return !online_session || (target_count <= 1 && !closes_drive2);
}

} // namespace Xm8Ra

#endif
