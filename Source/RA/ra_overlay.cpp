//
// eXcellent Multi-platform emulator type 8 - 'XM8'
// based on ePC-8801MA
//
// [ RetroAchievements overlay state ]
//

#include "ra_overlay.h"

#include <algorithm>
#include <cstddef>

namespace Xm8Ra {

namespace {

const size_t kMaxUsernameBytes = 256;
const size_t kMaxPasswordBytes = 1024;

bool IsAllowedLoginByte(unsigned char ch)
{
	return ch >= 0x20 && ch <= 0x7e;
}

} // namespace

void RaOverlay::Clear()
{
	notice_text_.clear();
	notice_until_ms_ = 0;
	snapshot_ = RaOverlaySnapshot();
	CloseScreen();
}

void RaOverlay::AddNotice(const std::string& text, uint32_t now_ms,
	uint32_t duration_ms)
{
	if (text.empty()) {
		return;
	}
	notice_text_ = text;
	notice_until_ms_ = now_ms + duration_ms;
}

bool RaOverlay::HasVisibleNotice(uint32_t now_ms) const
{
	return !notice_text_.empty() &&
		static_cast<int32_t>(now_ms - notice_until_ms_) < 0;
}

std::string RaOverlay::VisibleNotice(uint32_t now_ms) const
{
	return HasVisibleNotice(now_ms) ? notice_text_ : std::string();
}

void RaOverlay::SetSnapshot(const RaOverlaySnapshot& snapshot)
{
	snapshot_ = snapshot;
}

const RaOverlaySnapshot& RaOverlay::Snapshot() const
{
	return snapshot_;
}

void RaOverlay::OpenLogin(const std::string& username)
{
	WipeLoginPassword();
	screen_ = RaOverlayScreen::Login;
	login_field_ = username.empty() ? RaOverlayLoginField::Username :
		RaOverlayLoginField::Password;
	login_username_ = username.substr(0, kMaxUsernameBytes);
	login_status_.clear();
	login_submit_pending_ = false;
}

bool RaOverlay::IsBlocking() const
{
	return screen_ != RaOverlayScreen::None;
}

RaOverlayScreen RaOverlay::Screen() const
{
	return screen_;
}

RaOverlayLoginSnapshot RaOverlay::LoginSnapshot() const
{
	RaOverlayLoginSnapshot snapshot;
	snapshot.active = screen_ == RaOverlayScreen::Login;
	snapshot.field = login_field_;
	snapshot.username = login_username_;
	snapshot.masked_password.assign(login_password_.size(), '*');
	snapshot.status_message = login_status_;
	snapshot.can_submit = CanSubmitLogin();
	return snapshot;
}

RaOverlayAction RaOverlay::OnTextInput(const char *text)
{
	if (screen_ != RaOverlayScreen::Login || login_submit_pending_) {
		return RaOverlayAction::None;
	}
	AppendLoginText(text);
	return RaOverlayAction::None;
}

RaOverlayAction RaOverlay::OnControlKey(RaOverlayKey key)
{
	if (screen_ != RaOverlayScreen::Login) {
		return RaOverlayAction::None;
	}
	if (login_submit_pending_ && key != RaOverlayKey::Escape) {
		return RaOverlayAction::None;
	}

	switch (key) {
	case RaOverlayKey::Tab:
	case RaOverlayKey::Up:
	case RaOverlayKey::Down:
		ToggleLoginField();
		break;
	case RaOverlayKey::Backspace:
		BackspaceLoginField();
		break;
	case RaOverlayKey::Enter:
		if (CanSubmitLogin()) {
			login_status_ = "Login pending";
			login_submit_pending_ = true;
			return RaOverlayAction::SubmitLogin;
		}
		if (login_field_ == RaOverlayLoginField::Username &&
			!login_username_.empty()) {
			login_field_ = RaOverlayLoginField::Password;
		}
		else {
			login_status_ = "Enter username and password";
		}
		break;
	case RaOverlayKey::Escape:
		CloseScreen();
		return RaOverlayAction::Close;
	}
	return RaOverlayAction::None;
}

bool RaOverlay::ConsumeSubmittedLogin(std::string *username,
	std::string *password)
{
	if (!login_submit_pending_ || username == nullptr || password == nullptr) {
		return false;
	}
	*username = login_username_;
	*password = login_password_;
	WipeLoginPassword();
	login_submit_pending_ = false;
	return true;
}

void RaOverlay::SetLoginStatus(const std::string& message)
{
	login_status_ = message;
}

void RaOverlay::CloseScreen()
{
	WipeLoginPassword();
	screen_ = RaOverlayScreen::None;
	login_field_ = RaOverlayLoginField::Username;
	login_username_.clear();
	login_status_.clear();
	login_submit_pending_ = false;
}

void RaOverlay::ToggleLoginField()
{
	login_field_ = login_field_ == RaOverlayLoginField::Username ?
		RaOverlayLoginField::Password : RaOverlayLoginField::Username;
}

void RaOverlay::WipeLoginPassword()
{
	std::fill(login_password_.begin(), login_password_.end(), '\0');
	login_password_.clear();
}

bool RaOverlay::AppendLoginText(const char *text)
{
	if (text == nullptr) {
		return false;
	}

	std::string *field = login_field_ == RaOverlayLoginField::Username ?
		&login_username_ : &login_password_;
	const size_t limit = login_field_ == RaOverlayLoginField::Username ?
		kMaxUsernameBytes : kMaxPasswordBytes;

	bool changed = false;
	for (const unsigned char *p =
		reinterpret_cast<const unsigned char *>(text); *p != '\0'; ++p) {
		if (!IsAllowedLoginByte(*p)) {
			continue;
		}
		if (field->size() >= limit) {
			break;
		}
		field->push_back(static_cast<char>(*p));
		changed = true;
	}
	if (changed) {
		login_status_.clear();
	}
	return changed;
}

bool RaOverlay::BackspaceLoginField()
{
	std::string *field = login_field_ == RaOverlayLoginField::Username ?
		&login_username_ : &login_password_;
	if (field->empty()) {
		return false;
	}
	field->pop_back();
	login_status_.clear();
	return true;
}

bool RaOverlay::CanSubmitLogin() const
{
	return !login_username_.empty() && !login_password_.empty() &&
		!login_submit_pending_;
}

} // namespace Xm8Ra
