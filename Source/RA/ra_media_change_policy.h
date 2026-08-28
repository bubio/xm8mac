#ifndef XM8_RA_MEDIA_CHANGE_POLICY_H
#define XM8_RA_MEDIA_CHANGE_POLICY_H

#include <cstddef>
#include <cstdint>
#include <string>

namespace Xm8Ra {

enum class RaMediaChangeAction {
	NoChange,
	BeginSameGameChange,
	RejectPending,
	RejectDifferentGame,
};

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

// While the Drive 1 game is still being loaded by rcheevos, do not let a
// later mount silently replace it. Drive 2 may only be populated with media
// already known to belong to the same local game.
inline bool CanMountWhileGameLoadPending(int drive,
	int64_t pending_library_game_id, const std::string& pending_hash,
	int64_t target_library_game_id, const std::string& target_hash)
{
	if (drive == 0) {
		return !pending_hash.empty() && target_hash == pending_hash;
	}
	return drive == 1 && pending_library_game_id > 0 &&
		target_library_game_id == pending_library_game_id;
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
// same D88 is handled as one deferred mount elsewhere, but a sequence of
// independent drive changes cannot be made atomic around asynchronous RA
// approval. Reject such batches before either the VM or RA state is changed.
inline bool CanApplySequentialRaMediaBatch(bool online_session,
	size_t target_count, bool closes_drive2)
{
	return !online_session || (target_count <= 1 && !closes_drive2);
}

inline RaMediaChangeAction ClassifyMediaChange(bool game_loaded,
	bool change_pending, int64_t active_library_game_id,
	const std::string& active_hash, int64_t target_library_game_id,
	const std::string& target_hash)
{
	if (!game_loaded || active_hash == target_hash) {
		return RaMediaChangeAction::NoChange;
	}
	if (change_pending) {
		return RaMediaChangeAction::RejectPending;
	}
	if (active_library_game_id <= 0 ||
		target_library_game_id != active_library_game_id) {
		return RaMediaChangeAction::RejectDifferentGame;
	}
	return RaMediaChangeAction::BeginSameGameChange;
}

} // namespace Xm8Ra

#endif
