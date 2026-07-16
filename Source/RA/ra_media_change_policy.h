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

inline RaMediaChangeAction ClassifyMediaChange(int drive,
	bool game_loaded, bool change_pending, bool same_working_media,
	int64_t active_library_game_id,
	const std::string& active_hash, int64_t target_library_game_id,
	const std::string& target_hash)
{
	if (drive != 0 || !game_loaded || same_working_media ||
		active_hash == target_hash) {
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
