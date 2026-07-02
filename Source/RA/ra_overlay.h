//
// eXcellent Multi-platform emulator type 8 - 'XM8'
// based on ePC-8801MA
//
// [ RetroAchievements overlay state ]
//

#ifndef XM8_RA_OVERLAY_H
#define XM8_RA_OVERLAY_H

#include <cstdint>
#include <string>

namespace Xm8Ra {

enum class RaOverlayScreen {
	None,
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

class RaOverlay {
public:
	void Clear();
	void AddNotice(const std::string& text, uint32_t now_ms,
		uint32_t duration_ms = 5000);
	bool HasVisibleNotice(uint32_t now_ms) const;
	std::string VisibleNotice(uint32_t now_ms) const;

	void SetSnapshot(const RaOverlaySnapshot& snapshot);
	const RaOverlaySnapshot& Snapshot() const;

	void OpenLogin(const std::string& username = std::string());
	bool IsBlocking() const;
	RaOverlayScreen Screen() const;
	RaOverlayLoginSnapshot LoginSnapshot() const;
	RaOverlayAction OnTextInput(const char *text);
	RaOverlayAction OnControlKey(RaOverlayKey key);
	RaOverlayAction OnLoginTarget(RaOverlayLoginTarget target, bool activate);
	bool LoginTargetAt(int x, int y, RaOverlayLoginTarget *target) const;
	RaOverlayAction OnLoginPointer(int x, int y, bool activate);
	bool ConsumeSubmittedLogin(std::string *username, std::string *password);
	void SetLoginStatus(const std::string& message);
	void CloseScreen();

private:
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
	RaOverlayScreen screen_ = RaOverlayScreen::None;
	RaOverlayLoginField login_field_ = RaOverlayLoginField::Username;
	RaOverlayLoginTarget login_focus_ = RaOverlayLoginTarget::Username;
	std::string login_username_;
	std::string login_password_;
	std::string login_status_;
	bool login_submit_pending_ = false;
};

} // namespace Xm8Ra

#endif // XM8_RA_OVERLAY_H
