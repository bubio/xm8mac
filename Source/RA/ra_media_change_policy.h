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
	RejectBusy,
};

struct RaDiskPolicyContext {
	bool ra_enabled = false;
	RaDiskLoginState login_state = RaDiskLoginState::LoggedOut;
	bool hardcore_selected = false;
	RaDiskRole role = RaDiskRole::Anchor;
	bool anchor_load_pending = false;
	bool anchor_change_pending = false;
	bool active_game_loaded = false;
	bool same_media_container = false;
	int64_t active_library_game_id = 0;
	std::string active_hash;
	int64_t target_library_game_id = 0;
	std::string target_hash;
};

// Login and selected play mode affect how an anchor launch is carried out,
// never whether an auxiliary disk may be mounted. Only Drive 1 owns the RA
// active media identity.
inline RaDiskAction ClassifyRaDiskAction(const RaDiskPolicyContext& context)
{
	if (!context.ra_enabled) return RaDiskAction::MountNormal;
	if (context.role == RaDiskRole::Auxiliary) {
		return RaDiskAction::MountAuxiliary;
	}
	if (context.anchor_load_pending || context.anchor_change_pending) {
		return RaDiskAction::RejectBusy;
	}
	if (!context.active_game_loaded) {
		return RaDiskAction::BeginAnchorLaunch;
	}
	if (context.same_media_container ||
		(!context.active_hash.empty() &&
		 context.active_hash == context.target_hash)) {
		return RaDiskAction::MountNormal;
	}
	if (context.active_library_game_id > 0 &&
		context.target_library_game_id == context.active_library_game_id) {
		return RaDiskAction::ChangeAnchorMedia;
	}
	return RaDiskAction::RestartAnchorLaunch;
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
