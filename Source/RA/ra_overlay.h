//
// eXcellent Multi-platform emulator type 8 - 'XM8'
// based on ePC-8801MA
//
// [ RetroAchievements overlay state ]
//

#ifndef XM8_RA_OVERLAY_H
#define XM8_RA_OVERLAY_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Xm8Ra {

enum class RaOverlayScreen {
	None,
	Achievements,
	AchievementDetail,
	Leaderboards,
	Login,
};

enum class RaOverlayLoginField {
	Username,
	Password,
};

enum class RaOverlayLoginTarget {
	Username,
	Password,
	Login,
	Cancel,
};

enum class RaOverlayKey {
	Tab,
	Backspace,
	Enter,
	Escape,
	Up,
	Down,
	Left,
	Right,
};

enum class RaOverlayAction {
	None,
	SubmitLogin,
	Close,
};

struct RaOverlaySnapshot {
	bool mode_enabled = false;
	bool hardcore_enabled = false;
	std::string status_text;
	std::string user_name;
	std::string game_title;
	std::string rich_presence;
};

struct RaOverlayLoginSnapshot {
	bool active = false;
	RaOverlayLoginField field = RaOverlayLoginField::Username;
	RaOverlayLoginTarget focus = RaOverlayLoginTarget::Username;
	std::string username;
	std::string masked_password;
	std::string status_message;
	bool can_submit = false;
};

struct RaOverlayAchievementItem {
	uint32_t id = 0;
	uint32_t points = 0;
	uint8_t unlocked = 0;
	std::string title;
	std::string description;
	std::string measured_progress;
	std::string bucket_label;
};

struct RaOverlayAchievementListSnapshot {
	bool active = false;
	bool game_loaded = false;
	bool has_achievements = false;
	size_t selected_index = 0;
	size_t first_visible_index = 0;
	uint32_t selection_revision = 0;
	std::string game_title;
	std::string status_message;
	std::vector<RaOverlayAchievementItem> achievements;
};

struct RaOverlayAchievementDetailSnapshot {
	bool active = false;
	size_t selected_index = 0;
	size_t item_count = 0;
	int scroll_offset = 0;
	uint32_t selection_revision = 0;
	std::string game_title;
	RaOverlayAchievementItem achievement;
};

struct RaOverlayLeaderboardListSnapshot {
	bool active = false;
	bool game_loaded = false;
	size_t selected_index = 0;
	size_t first_visible_index = 0;
	std::string game_title;
	std::string status_message;
	std::vector<std::string> leaderboards;
};

class RaOverlay {
public:
	void Clear();
	void AddNotice(const std::string& text, uint32_t now_ms,
		uint32_t duration_ms = 5000);
	bool HasVisibleNotice(uint32_t now_ms) const;
	std::string VisibleNotice(uint32_t now_ms) const;

	void SetSnapshot(const RaOverlaySnapshot& snapshot);
	const RaOverlaySnapshot& Snapshot() const;

	void OpenAchievements(const RaOverlayAchievementListSnapshot& snapshot);
	void OpenLeaderboards(const RaOverlayLeaderboardListSnapshot& snapshot);
	void OpenLogin(const std::string& username = std::string());
	bool IsBlocking() const;
	RaOverlayScreen Screen() const;
	RaOverlayAchievementListSnapshot AchievementListSnapshot() const;
	RaOverlayAchievementDetailSnapshot AchievementDetailSnapshot() const;
	RaOverlayLeaderboardListSnapshot LeaderboardListSnapshot() const;
	RaOverlayLoginSnapshot LoginSnapshot() const;
	RaOverlayAction OnTextInput(const char *text);
	RaOverlayAction OnControlKey(RaOverlayKey key);
	RaOverlayAction OnAchievementPointer(int x, int y, bool activate);
	RaOverlayAction OnListPointer(int x, int y, bool activate);
	RaOverlayAction OnListScroll(int delta);
	RaOverlayAction OnLoginTarget(RaOverlayLoginTarget target, bool activate);
	bool LoginTargetAt(int x, int y, RaOverlayLoginTarget *target) const;
	RaOverlayAction OnLoginPointer(int x, int y, bool activate);
	bool ConsumeSubmittedLogin(std::string *username, std::string *password);
	void SetLoginStatus(const std::string& message);
	void CloseScreen();

private:
	void MoveListSelection(int delta);
	void OpenAchievementDetail();
	void CloseAchievementDetail();
	void MoveAchievementDetailScroll(int delta);
	bool ListIndexAt(int x, int y, size_t *index) const;
	void NormalizeListSelection();
	size_t ListItemCount() const;
	void MoveLoginFocus(int delta);
	bool IsLoginTextFocused() const;
	void SetLoginFieldFocus(RaOverlayLoginField field);
	void WipeLoginPassword();
	bool AppendLoginText(const char *text);
	bool BackspaceLoginField();
	RaOverlayAction ActivateLoginFocus();
	bool CanSubmitLogin() const;

	std::string notice_text_;
	uint32_t notice_until_ms_ = 0;
	RaOverlaySnapshot snapshot_;
	RaOverlayAchievementListSnapshot achievements_;
	RaOverlayLeaderboardListSnapshot leaderboards_;
	RaOverlayScreen screen_ = RaOverlayScreen::None;
	uint32_t achievement_selection_revision_ = 0;
	int achievement_detail_scroll_ = 0;
	RaOverlayLoginField login_field_ = RaOverlayLoginField::Username;
	RaOverlayLoginTarget login_focus_ = RaOverlayLoginTarget::Username;
	std::string login_username_;
	std::string login_password_;
	std::string login_status_;
	bool login_submit_pending_ = false;
};

} // namespace Xm8Ra

#endif // XM8_RA_OVERLAY_H
