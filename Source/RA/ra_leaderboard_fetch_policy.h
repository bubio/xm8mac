#ifndef XM8_RA_LEADERBOARD_FETCH_POLICY_H
#define XM8_RA_LEADERBOARD_FETCH_POLICY_H

#include <cstdint>

namespace Xm8Ra {

inline bool ShouldCheckLeaderboardEntriesAfterSelection(
	uint32_t old_id, uint32_t new_id)
{
	return new_id != 0 && new_id != old_id;
}

inline bool ShouldFetchLeaderboardEntries(uint32_t selected_id,
	bool has_cached_scoreboard, uint32_t observed_id,
	bool observed_request_known)
{
	if (selected_id == 0 || has_cached_scoreboard) return false;
	return observed_id != selected_id || !observed_request_known;
}

} // namespace Xm8Ra

#endif
