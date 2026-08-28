#ifndef XM8_RA_MEDIA_CHANGE_POLICY_H
#define XM8_RA_MEDIA_CHANGE_POLICY_H

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

inline bool RaMediaRollbackRestoredAllDrives(bool open_pair,
	bool drive1_restored, bool drive2_restored)
{
	return drive1_restored && (!open_pair || drive2_restored);
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
