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
const size_t kMenuListVisibleRows = 7;
const size_t kMaxVisibleNotices = 3;
const size_t kMaxQueuedNotices = 64;
const int kIdentificationIdentified = 1;

const int kMenuListRowX = 80;
const int kMenuListRowY = 80;
const int kMenuListRowW = 480;
const int kMenuListRowH = 40;
const int kMenuListRowPitch = 40;

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
	ClearNotices();
	snapshot_ = RaOverlaySnapshot();
	CloseScreen();
}

void RaOverlay::ClearNotices()
{
	notices_.clear();
	next_notice_sequence_ = 1;
	notices_paused_ = false;
}

void RaOverlay::AddNotice(const std::string& text, uint32_t now_ms,
	uint32_t duration_ms, RaNoticePriority priority)
{
	if (text.empty() || duration_ms == 0) {
		return;
	}
	if (!notices_paused_) {
		RebalanceNotices(now_ms);
	}
	QueuedNotice notice;
	notice.text = text;
	notice.priority = priority;
	notice.remaining_ms = duration_ms;
	notice.sequence = next_notice_sequence_++;
	notices_.push_back(notice);
	if (!notices_paused_) {
		RebalanceNotices(now_ms);
	}
	if (notices_.size() > kMaxQueuedNotices) {
		auto drop = std::min_element(notices_.begin(), notices_.end(),
			[](const QueuedNotice& left, const QueuedNotice& right) {
				if (left.priority != right.priority) {
					return left.priority < right.priority;
				}
				return left.sequence > right.sequence;
			});
		if (drop != notices_.end()) {
			notices_.erase(drop);
		}
	}
}

void RaOverlay::SetNoticesPaused(bool paused, uint32_t now_ms)
{
	if (paused == notices_paused_) {
		return;
	}
	if (paused) {
		RebalanceNotices(now_ms);
		for (QueuedNotice& notice : notices_) {
			if (notice.active) {
				notice.remaining_ms = notice.expires_at_ms - now_ms;
				notice.active = false;
			}
		}
		notices_paused_ = true;
	}
	else {
		notices_paused_ = false;
		RebalanceNotices(now_ms);
	}
}

bool RaOverlay::HasVisibleNotice(uint32_t now_ms)
{
	return !VisibleNotices(now_ms).empty();
}

std::string RaOverlay::VisibleNotice(uint32_t now_ms)
{
	const std::vector<RaVisibleNotice> visible = VisibleNotices(now_ms);
	return visible.empty() ? std::string() : visible.front().text;
}

std::vector<RaVisibleNotice> RaOverlay::VisibleNotices(uint32_t now_ms)
{
	std::vector<RaVisibleNotice> visible;
	if (notices_paused_) {
		return visible;
	}
	RebalanceNotices(now_ms);
	for (const QueuedNotice& notice : notices_) {
		if (notice.active) {
			visible.push_back({notice.text, notice.priority});
		}
	}
	std::stable_sort(visible.begin(), visible.end(),
		[](const RaVisibleNotice& left, const RaVisibleNotice& right) {
			return left.priority > right.priority;
		});
	return visible;
}

size_t RaOverlay::NoticeQueueSize() const
{
	return notices_.size();
}

void RaOverlay::RebalanceNotices(uint32_t now_ms)
{
	if (notices_paused_) {
		return;
	}
	for (QueuedNotice& notice : notices_) {
		if (notice.active) {
			const int32_t remaining = static_cast<int32_t>(
				notice.expires_at_ms - now_ms);
			notice.remaining_ms = remaining > 0 ?
				static_cast<uint32_t>(remaining) : 0;
			notice.active = false;
		}
	}
	notices_.erase(std::remove_if(notices_.begin(), notices_.end(),
		[](const QueuedNotice& notice) { return notice.remaining_ms == 0; }),
		notices_.end());

	std::vector<QueuedNotice *> ranked;
	ranked.reserve(notices_.size());
	for (QueuedNotice& notice : notices_) {
		ranked.push_back(&notice);
	}
	std::sort(ranked.begin(), ranked.end(),
		[](const QueuedNotice *left, const QueuedNotice *right) {
			if (left->priority != right->priority) {
				return left->priority > right->priority;
			}
			return left->sequence < right->sequence;
		});
	const size_t active_count = std::min(kMaxVisibleNotices, ranked.size());
	for (size_t index = 0; index < active_count; ++index) {
		ranked[index]->active = true;
		ranked[index]->expires_at_ms = now_ms + ranked[index]->remaining_ms;
	}
}

void RaOverlay::SetSnapshot(const RaOverlaySnapshot& snapshot)
{
	snapshot_ = snapshot;
}

const RaOverlaySnapshot& RaOverlay::Snapshot() const
{
	return snapshot_;
}

void RaOverlay::OpenLibrary(const RaOverlayLibraryListSnapshot& snapshot)
{
	WipeLoginPassword();
	library_ = snapshot;
	library_.active = true;
	library_.selected_index = 0;
	library_.first_visible_index = 0;
	achievements_ = RaOverlayAchievementListSnapshot();
	leaderboards_ = RaOverlayLeaderboardListSnapshot();
	screen_ = RaOverlayScreen::Library;
	achievement_detail_scroll_ = 0;
	++achievement_selection_revision_;
	NormalizeListSelection();
	login_status_.clear();
	login_submit_pending_ = false;
}

void RaOverlay::OpenAchievements(
	const RaOverlayAchievementListSnapshot& snapshot)
{
	WipeLoginPassword();
	library_ = RaOverlayLibraryListSnapshot();
	achievements_ = snapshot;
	achievements_.active = true;
	achievements_.selected_index = 0;
	achievements_.first_visible_index = 0;
	leaderboards_ = RaOverlayLeaderboardListSnapshot();
	screen_ = RaOverlayScreen::Achievements;
	achievement_detail_scroll_ = 0;
	++achievement_selection_revision_;
	NormalizeListSelection();
	login_status_.clear();
	login_submit_pending_ = false;
}

void RaOverlay::OpenLeaderboards(
	const RaOverlayLeaderboardListSnapshot& snapshot)
{
	WipeLoginPassword();
	library_ = RaOverlayLibraryListSnapshot();
	leaderboards_ = snapshot;
	leaderboards_.active = true;
	leaderboards_.selected_index = 0;
	leaderboards_.first_visible_index = 0;
	achievements_ = RaOverlayAchievementListSnapshot();
	screen_ = RaOverlayScreen::Leaderboards;
	achievement_detail_scroll_ = 0;
	NormalizeListSelection();
	login_status_.clear();
	login_submit_pending_ = false;
}

void RaOverlay::UpdateLeaderboards(
	const RaOverlayLeaderboardListSnapshot& snapshot)
{
	if (screen_ != RaOverlayScreen::Leaderboards) {
		return;
	}

	const size_t selected_index = leaderboards_.selected_index;
	const size_t first_visible_index = leaderboards_.first_visible_index;
	leaderboards_ = snapshot;
	leaderboards_.active = true;
	leaderboards_.selected_index = selected_index;
	leaderboards_.first_visible_index = first_visible_index;
	NormalizeListSelection();
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

RaOverlayLibraryListSnapshot RaOverlay::LibraryListSnapshot() const
{
	RaOverlayLibraryListSnapshot snapshot = library_;
	snapshot.active = screen_ == RaOverlayScreen::Library;
	snapshot.selection_revision = achievement_selection_revision_;
	return snapshot;
}

RaOverlayAchievementListSnapshot RaOverlay::AchievementListSnapshot() const
{
	RaOverlayAchievementListSnapshot snapshot = achievements_;
	snapshot.active = screen_ == RaOverlayScreen::Achievements;
	snapshot.selection_revision = achievement_selection_revision_;
	return snapshot;
}

RaOverlayAchievementDetailSnapshot RaOverlay::AchievementDetailSnapshot() const
{
	RaOverlayAchievementDetailSnapshot snapshot;
	snapshot.active = screen_ == RaOverlayScreen::AchievementDetail;
	snapshot.selected_index = achievements_.selected_index;
	snapshot.item_count = achievements_.achievements.size();
	snapshot.scroll_offset = achievement_detail_scroll_;
	snapshot.selection_revision = achievement_selection_revision_;
	snapshot.game_title = achievements_.game_title;
	if (achievements_.selected_index < achievements_.achievements.size()) {
		snapshot.achievement =
			achievements_.achievements[achievements_.selected_index];
	}
	return snapshot;
}

RaOverlayLeaderboardListSnapshot RaOverlay::LeaderboardListSnapshot() const
{
	RaOverlayLeaderboardListSnapshot snapshot = leaderboards_;
	snapshot.active = screen_ == RaOverlayScreen::Leaderboards;
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
	if (screen_ == RaOverlayScreen::GameDetail) {
		switch (key) {
		case RaOverlayKey::Escape:
			screen_ = RaOverlayScreen::Library;
			break;
		case RaOverlayKey::Enter:
			if (CanLaunchSelectedLibraryGame()) {
				return RaOverlayAction::OpenLibraryGame;
			}
			if (CanResolveSelectedLibraryConflict()) {
				return RaOverlayAction::ResolveLibraryConflict;
			}
			break;
		case RaOverlayKey::Up:
		case RaOverlayKey::Down:
		case RaOverlayKey::Left:
		case RaOverlayKey::Right:
		case RaOverlayKey::Tab:
		case RaOverlayKey::Backspace:
		case RaOverlayKey::PageUp:
		case RaOverlayKey::PageDown:
			break;
		}
		return RaOverlayAction::None;
	}

	if (screen_ == RaOverlayScreen::AchievementDetail) {
		switch (key) {
		case RaOverlayKey::Escape:
			CloseAchievementDetail();
			break;
		case RaOverlayKey::Backspace:
			break;
		case RaOverlayKey::Up:
		case RaOverlayKey::Left:
			MoveAchievementDetailScroll(-1);
			break;
		case RaOverlayKey::Down:
		case RaOverlayKey::Right:
		case RaOverlayKey::Tab:
			MoveAchievementDetailScroll(1);
			break;
		case RaOverlayKey::PageUp:
			MoveAchievementDetailScroll(-static_cast<int>(kMenuListVisibleRows));
			break;
		case RaOverlayKey::PageDown:
			MoveAchievementDetailScroll(static_cast<int>(kMenuListVisibleRows));
			break;
		case RaOverlayKey::Enter:
			break;
		}
		return RaOverlayAction::None;
	}

	if (screen_ == RaOverlayScreen::Library ||
		screen_ == RaOverlayScreen::Achievements ||
		screen_ == RaOverlayScreen::Leaderboards) {
		switch (key) {
		case RaOverlayKey::Escape:
			CloseScreen();
			return RaOverlayAction::Close;
		case RaOverlayKey::Up:
			MoveListSelection(-1);
			break;
		case RaOverlayKey::Down:
			MoveListSelection(1);
			break;
		case RaOverlayKey::Tab:
			MoveListSelection(1);
			break;
		case RaOverlayKey::Right:
			MoveListSelection(1);
			break;
		case RaOverlayKey::Left:
			MoveListSelection(-1);
			break;
		case RaOverlayKey::Enter:
			if (screen_ == RaOverlayScreen::Library &&
				ListItemCount() > 0) {
				screen_ = RaOverlayScreen::GameDetail;
			}
			else if (screen_ == RaOverlayScreen::Achievements) {
				OpenAchievementDetail();
			}
			break;
		case RaOverlayKey::Backspace:
			break;
		case RaOverlayKey::PageUp:
			MoveListSelection(-static_cast<int>(kMenuListVisibleRows));
			break;
		case RaOverlayKey::PageDown:
			MoveListSelection(static_cast<int>(kMenuListVisibleRows));
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
	case RaOverlayKey::PageUp:
		MoveLoginFocus(-1);
		break;
	case RaOverlayKey::PageDown:
		MoveLoginFocus(1);
		break;
	}
	return RaOverlayAction::None;
}

RaOverlayAction RaOverlay::OnAchievementPointer(int x, int y, bool activate)
{
	return OnListPointer(x, y, activate);
}

RaOverlayAction RaOverlay::OnListPointer(int x, int y, bool activate)
{
	if (screen_ != RaOverlayScreen::Achievements) {
		if (screen_ != RaOverlayScreen::Library &&
			screen_ != RaOverlayScreen::Leaderboards) {
			return RaOverlayAction::None;
		}
	}

	size_t index = 0;
	if (ListTargetAt(x, y, &index)) {
		if (screen_ == RaOverlayScreen::Library) {
			library_.selected_index = index;
			if (activate) {
				NormalizeListSelection();
				screen_ = RaOverlayScreen::GameDetail;
				return RaOverlayAction::None;
			}
		}
		else if (screen_ == RaOverlayScreen::Achievements) {
			if (achievements_.selected_index != index) {
				achievements_.selected_index = index;
				achievement_detail_scroll_ = 0;
				++achievement_selection_revision_;
			}
			if (activate) {
				NormalizeListSelection();
				OpenAchievementDetail();
				return RaOverlayAction::None;
			}
		}
		else {
			leaderboards_.selected_index = index;
		}
		NormalizeListSelection();
		return RaOverlayAction::None;
	}
	if (ListItemCount() == 0 &&
		HitRect(x, y, kMenuListRowX, kMenuListRowY,
			kMenuListRowW, kMenuListRowH)) {
		return RaOverlayAction::None;
	}
	if (activate) {
		CloseScreen();
		return RaOverlayAction::Close;
	}
	return RaOverlayAction::None;
}

RaOverlayAction RaOverlay::OnListScroll(int delta)
{
	if (screen_ == RaOverlayScreen::GameDetail) {
		return RaOverlayAction::None;
	}
	if (screen_ == RaOverlayScreen::AchievementDetail) {
		MoveAchievementDetailScroll(delta);
		return RaOverlayAction::None;
	}
	if (screen_ != RaOverlayScreen::Achievements &&
		screen_ != RaOverlayScreen::Library &&
		screen_ != RaOverlayScreen::Leaderboards) {
		return RaOverlayAction::None;
	}
	MoveListSelection(delta);
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
	library_ = RaOverlayLibraryListSnapshot();
	achievements_ = RaOverlayAchievementListSnapshot();
	leaderboards_ = RaOverlayLeaderboardListSnapshot();
	achievement_detail_scroll_ = 0;
	login_field_ = RaOverlayLoginField::Username;
	login_focus_ = RaOverlayLoginTarget::Username;
	login_username_.clear();
	login_status_.clear();
	login_submit_pending_ = false;
}

void RaOverlay::MoveListSelection(int delta)
{
	const size_t count = ListItemCount();
	if (count == 0) {
		return;
	}

	size_t *selected = screen_ == RaOverlayScreen::Library ?
		&library_.selected_index : (screen_ == RaOverlayScreen::Achievements ?
			&achievements_.selected_index : &leaderboards_.selected_index);
	const size_t previous = *selected;
	const size_t last = count - 1;
	if (delta < 0) {
		const size_t amount = static_cast<size_t>(-delta);
		*selected = amount > *selected ? 0 : *selected - amount;
	}
	else {
		*selected += static_cast<size_t>(delta);
		if (*selected > last) {
			*selected = last;
		}
	}
	if ((screen_ == RaOverlayScreen::Library ||
		screen_ == RaOverlayScreen::Achievements) && *selected != previous) {
		achievement_detail_scroll_ = 0;
		++achievement_selection_revision_;
	}
	NormalizeListSelection();
}

void RaOverlay::OpenAchievementDetail()
{
	if (screen_ != RaOverlayScreen::Achievements ||
		achievements_.selected_index >= achievements_.achievements.size()) {
		return;
	}
	achievement_detail_scroll_ = 0;
	screen_ = RaOverlayScreen::AchievementDetail;
}

void RaOverlay::CloseAchievementDetail()
{
	if (screen_ == RaOverlayScreen::AchievementDetail) {
		screen_ = RaOverlayScreen::Achievements;
	}
}

void RaOverlay::MoveAchievementDetailScroll(int delta)
{
	if (screen_ != RaOverlayScreen::AchievementDetail) {
		return;
	}
	achievement_detail_scroll_ += delta;
	if (achievement_detail_scroll_ < 0) {
		achievement_detail_scroll_ = 0;
	}
}

bool RaOverlay::ListTargetAt(int x, int y, size_t *index) const
{
	if (screen_ != RaOverlayScreen::Library &&
		screen_ != RaOverlayScreen::Achievements &&
		screen_ != RaOverlayScreen::Leaderboards) {
		return false;
	}
	const size_t count = ListItemCount();
	const size_t first_visible = screen_ == RaOverlayScreen::Library ?
		library_.first_visible_index : (screen_ == RaOverlayScreen::Achievements ?
			achievements_.first_visible_index :
			leaderboards_.first_visible_index);
	if (count == 0) {
		return false;
	}

	for (size_t row = 0; row < kMenuListVisibleRows; ++row) {
		const size_t candidate = first_visible + row;
		if (candidate >= count) {
			break;
		}
		const int row_y = kMenuListRowY +
			static_cast<int>(row) * kMenuListRowPitch;
		if (HitRect(x, y, kMenuListRowX, row_y, kMenuListRowW,
			kMenuListRowH)) {
			if (index != nullptr) {
				*index = candidate;
			}
			return true;
		}
	}
	return false;
}

void RaOverlay::NormalizeListSelection()
{
	size_t *selected = screen_ == RaOverlayScreen::Leaderboards ?
		&leaderboards_.selected_index : (screen_ == RaOverlayScreen::Library ?
			&library_.selected_index : &achievements_.selected_index);
	size_t *first_visible = screen_ == RaOverlayScreen::Leaderboards ?
		&leaderboards_.first_visible_index : (screen_ == RaOverlayScreen::Library ?
			&library_.first_visible_index : &achievements_.first_visible_index);
	const size_t count = screen_ == RaOverlayScreen::Leaderboards ?
		leaderboards_.leaderboards.size() : (screen_ == RaOverlayScreen::Library ?
			library_.games.size() : achievements_.achievements.size());

	if (count == 0) {
		*selected = 0;
		*first_visible = 0;
		return;
	}

	const size_t last = count - 1;
	if (*selected > last) {
		*selected = last;
	}
	if (*first_visible > *selected) {
		*first_visible = *selected;
	}
	if (*selected >= *first_visible + kMenuListVisibleRows) {
		*first_visible = *selected - kMenuListVisibleRows + 1;
	}
	if (*first_visible > last) {
		*first_visible = last;
	}
}

size_t RaOverlay::ListItemCount() const
{
	if (screen_ == RaOverlayScreen::Library ||
		screen_ == RaOverlayScreen::GameDetail) {
		return library_.games.size();
	}
	if (screen_ == RaOverlayScreen::Leaderboards) {
		return leaderboards_.leaderboards.size();
	}
	return achievements_.achievements.size();
}

bool RaOverlay::SelectedLibraryGameId(int64_t *game_id) const
{
	if ((screen_ != RaOverlayScreen::Library &&
		screen_ != RaOverlayScreen::GameDetail) ||
		library_.selected_index >= library_.games.size() ||
		game_id == nullptr) {
		return false;
	}
	*game_id = library_.games[library_.selected_index].game_id;
	return true;
}

bool RaOverlay::CanLaunchSelectedLibraryGame() const
{
	return (screen_ == RaOverlayScreen::Library ||
		screen_ == RaOverlayScreen::GameDetail) &&
		library_.selected_index < library_.games.size() &&
		library_.games[library_.selected_index].identification_state ==
			kIdentificationIdentified &&
		library_.games[library_.selected_index].ra_game_id > 0;
}

bool RaOverlay::CanResolveSelectedLibraryConflict() const
{
	return (screen_ == RaOverlayScreen::Library ||
		screen_ == RaOverlayScreen::GameDetail) &&
		library_.selected_index < library_.games.size() &&
		library_.games[library_.selected_index].identification_state == 4 &&
		(library_.games[library_.selected_index].conflict_kind ==
			RaOverlayConflictKind::Merge ||
		 library_.games[library_.selected_index].conflict_kind ==
			RaOverlayConflictKind::Split);
}

bool RaOverlay::OpenSelectedLibraryGameDetail()
{
	if (screen_ != RaOverlayScreen::Library ||
		library_.selected_index >= library_.games.size()) {
		return false;
	}
	screen_ = RaOverlayScreen::GameDetail;
	return true;
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
