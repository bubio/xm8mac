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
const size_t kAchievementVisibleRows = 5;

const int kAchievementRowX = 88;
const int kAchievementRowY = 126;
const int kAchievementRowW = 464;
const int kAchievementRowH = 28;
const int kAchievementRowPitch = 32;

const int kLoginUserBoxX = 232;
const int kLoginUserBoxY = 130;
const int kLoginUserBoxW = 272;
const int kLoginUserBoxH = 28;
const int kLoginPassBoxX = 232;
const int kLoginPassBoxY = 170;
const int kLoginPassBoxW = 272;
const int kLoginPassBoxH = 28;
const int kLoginButtonX = 256;
const int kLoginButtonY = 220;
const int kLoginButtonW = 112;
const int kLoginButtonH = 30;
const int kCancelButtonX = 384;
const int kCancelButtonY = 220;
const int kCancelButtonW = 112;
const int kCancelButtonH = 30;

bool IsAllowedLoginByte(unsigned char ch)
{
	return ch >= 0x20 && ch <= 0x7e;
}

bool HitRect(int x, int y, int rx, int ry, int rw, int rh)
{
	return x >= rx && x < rx + rw && y >= ry && y < ry + rh;
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

void RaOverlay::OpenAchievements(
	const RaOverlayAchievementListSnapshot& snapshot)
{
	WipeLoginPassword();
	achievements_ = snapshot;
	achievements_.active = true;
	achievements_.selected_index = 0;
	achievements_.first_visible_index = 0;
	NormalizeAchievementSelection();
	screen_ = RaOverlayScreen::Achievements;
	login_status_.clear();
	login_submit_pending_ = false;
}

void RaOverlay::OpenLogin(const std::string& username)
{
	WipeLoginPassword();
	screen_ = RaOverlayScreen::Login;
	login_field_ = username.empty() ? RaOverlayLoginField::Username :
		RaOverlayLoginField::Password;
	login_focus_ = username.empty() ? RaOverlayLoginTarget::Username :
		RaOverlayLoginTarget::Password;
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

RaOverlayAchievementListSnapshot RaOverlay::AchievementListSnapshot() const
{
	RaOverlayAchievementListSnapshot snapshot = achievements_;
	snapshot.active = screen_ == RaOverlayScreen::Achievements;
	return snapshot;
}

RaOverlayLoginSnapshot RaOverlay::LoginSnapshot() const
{
	RaOverlayLoginSnapshot snapshot;
	snapshot.active = screen_ == RaOverlayScreen::Login;
	snapshot.field = login_field_;
	snapshot.focus = login_focus_;
	snapshot.username = login_username_;
	snapshot.masked_password.assign(login_password_.size(), '*');
	snapshot.status_message = login_status_;
	snapshot.can_submit = CanSubmitLogin();
	return snapshot;
}

RaOverlayAction RaOverlay::OnTextInput(const char *text)
{
	if (screen_ != RaOverlayScreen::Login || login_submit_pending_ ||
		!IsLoginTextFocused()) {
		return RaOverlayAction::None;
	}
	AppendLoginText(text);
	return RaOverlayAction::None;
}

RaOverlayAction RaOverlay::OnControlKey(RaOverlayKey key)
{
	if (screen_ == RaOverlayScreen::Achievements) {
		switch (key) {
		case RaOverlayKey::Escape:
			CloseScreen();
			return RaOverlayAction::Close;
		case RaOverlayKey::Up:
			MoveAchievementSelection(-1);
			break;
		case RaOverlayKey::Down:
			MoveAchievementSelection(1);
			break;
		case RaOverlayKey::Tab:
		case RaOverlayKey::Right:
			MoveAchievementSelection(1);
			break;
		case RaOverlayKey::Left:
			MoveAchievementSelection(-1);
			break;
		case RaOverlayKey::Enter:
		case RaOverlayKey::Backspace:
			break;
		}
		return RaOverlayAction::None;
	}

	if (screen_ != RaOverlayScreen::Login) {
		return RaOverlayAction::None;
	}
	if (login_submit_pending_) {
		return RaOverlayAction::None;
	}

	switch (key) {
	case RaOverlayKey::Tab:
	case RaOverlayKey::Down:
	case RaOverlayKey::Right:
		MoveLoginFocus(1);
		break;
	case RaOverlayKey::Up:
	case RaOverlayKey::Left:
		MoveLoginFocus(-1);
		break;
	case RaOverlayKey::Backspace:
		if (IsLoginTextFocused()) {
			BackspaceLoginField();
		}
		break;
	case RaOverlayKey::Enter:
		return ActivateLoginFocus();
	case RaOverlayKey::Escape:
		CloseScreen();
		return RaOverlayAction::Close;
	}
	return RaOverlayAction::None;
}

RaOverlayAction RaOverlay::OnAchievementPointer(int x, int y, bool activate)
{
	if (screen_ != RaOverlayScreen::Achievements) {
		return RaOverlayAction::None;
	}

	size_t index = 0;
	if (AchievementIndexAt(x, y, &index)) {
		achievements_.selected_index = index;
		NormalizeAchievementSelection();
		return RaOverlayAction::None;
	}
	if (activate) {
		CloseScreen();
		return RaOverlayAction::Close;
	}
	return RaOverlayAction::None;
}

RaOverlayAction RaOverlay::OnLoginTarget(RaOverlayLoginTarget target,
	bool activate)
{
	if (screen_ != RaOverlayScreen::Login) {
		return RaOverlayAction::None;
	}
	if (login_submit_pending_) {
		return RaOverlayAction::None;
	}

	login_focus_ = target;
	if (target == RaOverlayLoginTarget::Username) {
		login_field_ = RaOverlayLoginField::Username;
	}
	else if (target == RaOverlayLoginTarget::Password) {
		login_field_ = RaOverlayLoginField::Password;
	}

	return activate ? ActivateLoginFocus() : RaOverlayAction::None;
}

bool RaOverlay::LoginTargetAt(int x, int y, RaOverlayLoginTarget *target) const
{
	if (HitRect(x, y, kLoginUserBoxX, kLoginUserBoxY,
		kLoginUserBoxW, kLoginUserBoxH)) {
		if (target != nullptr) {
			*target = RaOverlayLoginTarget::Username;
		}
		return true;
	}
	if (HitRect(x, y, kLoginPassBoxX, kLoginPassBoxY,
		kLoginPassBoxW, kLoginPassBoxH)) {
		if (target != nullptr) {
			*target = RaOverlayLoginTarget::Password;
		}
		return true;
	}
	if (HitRect(x, y, kLoginButtonX, kLoginButtonY,
		kLoginButtonW, kLoginButtonH)) {
		if (target != nullptr) {
			*target = RaOverlayLoginTarget::Login;
		}
		return true;
	}
	if (HitRect(x, y, kCancelButtonX, kCancelButtonY,
		kCancelButtonW, kCancelButtonH)) {
		if (target != nullptr) {
			*target = RaOverlayLoginTarget::Cancel;
		}
		return true;
	}
	return false;
}

RaOverlayAction RaOverlay::OnLoginPointer(int x, int y, bool activate)
{
	RaOverlayLoginTarget target;
	if (!LoginTargetAt(x, y, &target)) {
		return RaOverlayAction::None;
	}
	return OnLoginTarget(target, activate);
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
	return true;
}

void RaOverlay::SetLoginStatus(const std::string& message)
{
	login_status_ = message;
	login_submit_pending_ = false;
}

void RaOverlay::CloseScreen()
{
	WipeLoginPassword();
	screen_ = RaOverlayScreen::None;
	achievements_ = RaOverlayAchievementListSnapshot();
	login_field_ = RaOverlayLoginField::Username;
	login_focus_ = RaOverlayLoginTarget::Username;
	login_username_.clear();
	login_status_.clear();
	login_submit_pending_ = false;
}

void RaOverlay::MoveAchievementSelection(int delta)
{
	if (achievements_.achievements.empty()) {
		return;
	}

	const size_t last = achievements_.achievements.size() - 1;
	if (delta < 0) {
		const size_t amount = static_cast<size_t>(-delta);
		achievements_.selected_index = amount > achievements_.selected_index ?
			0 : achievements_.selected_index - amount;
	}
	else {
		achievements_.selected_index += static_cast<size_t>(delta);
		if (achievements_.selected_index > last) {
			achievements_.selected_index = last;
		}
	}
	NormalizeAchievementSelection();
}

bool RaOverlay::AchievementIndexAt(int x, int y, size_t *index) const
{
	if (achievements_.achievements.empty()) {
		return false;
	}

	for (size_t row = 0; row < kAchievementVisibleRows; ++row) {
		const size_t candidate = achievements_.first_visible_index + row;
		if (candidate >= achievements_.achievements.size()) {
			break;
		}
		const int row_y = kAchievementRowY +
			static_cast<int>(row) * kAchievementRowPitch;
		if (HitRect(x, y, kAchievementRowX, row_y, kAchievementRowW,
			kAchievementRowH)) {
			if (index != nullptr) {
				*index = candidate;
			}
			return true;
		}
	}
	return false;
}

void RaOverlay::NormalizeAchievementSelection()
{
	if (achievements_.achievements.empty()) {
		achievements_.selected_index = 0;
		achievements_.first_visible_index = 0;
		return;
	}

	const size_t last = achievements_.achievements.size() - 1;
	if (achievements_.selected_index > last) {
		achievements_.selected_index = last;
	}
	if (achievements_.first_visible_index > achievements_.selected_index) {
		achievements_.first_visible_index = achievements_.selected_index;
	}
	if (achievements_.selected_index >=
		achievements_.first_visible_index + kAchievementVisibleRows) {
		achievements_.first_visible_index =
			achievements_.selected_index - kAchievementVisibleRows + 1;
	}
	if (achievements_.first_visible_index > last) {
		achievements_.first_visible_index = last;
	}
}

void RaOverlay::MoveLoginFocus(int delta)
{
	int focus = static_cast<int>(login_focus_);
	focus = (focus + delta) % 4;
	if (focus < 0) {
		focus += 4;
	}
	OnLoginTarget(static_cast<RaOverlayLoginTarget>(focus), false);
}

bool RaOverlay::IsLoginTextFocused() const
{
	return login_focus_ == RaOverlayLoginTarget::Username ||
		login_focus_ == RaOverlayLoginTarget::Password;
}

void RaOverlay::SetLoginFieldFocus(RaOverlayLoginField field)
{
	login_field_ = field;
	login_focus_ = field == RaOverlayLoginField::Username ?
		RaOverlayLoginTarget::Username : RaOverlayLoginTarget::Password;
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
	if (!IsLoginTextFocused()) {
		return false;
	}
	std::string *field = login_field_ == RaOverlayLoginField::Username ?
		&login_username_ : &login_password_;
	if (field->empty()) {
		return false;
	}
	field->pop_back();
	login_status_.clear();
	return true;
}

RaOverlayAction RaOverlay::ActivateLoginFocus()
{
	switch (login_focus_) {
	case RaOverlayLoginTarget::Username:
		if (!login_username_.empty()) {
			SetLoginFieldFocus(RaOverlayLoginField::Password);
		}
		else {
			login_status_ = "Enter username";
		}
		break;
	case RaOverlayLoginTarget::Password:
		if (CanSubmitLogin()) {
			login_status_ = "Login pending";
			login_submit_pending_ = true;
			return RaOverlayAction::SubmitLogin;
		}
		login_status_ = "Enter username and password";
		break;
	case RaOverlayLoginTarget::Login:
		if (CanSubmitLogin()) {
			login_status_ = "Login pending";
			login_submit_pending_ = true;
			return RaOverlayAction::SubmitLogin;
		}
		login_status_ = "Enter username and password";
		break;
	case RaOverlayLoginTarget::Cancel:
		CloseScreen();
		return RaOverlayAction::Close;
	}
	return RaOverlayAction::None;
}

bool RaOverlay::CanSubmitLogin() const
{
	return !login_username_.empty() && !login_password_.empty() &&
		!login_submit_pending_;
}

} // namespace Xm8Ra
