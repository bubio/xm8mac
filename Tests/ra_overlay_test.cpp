#include "ra_overlay.h"

#include <cassert>
#include <string>

int main()
{
	Xm8Ra::RaOverlay overlay;

	assert(!overlay.HasVisibleNotice(0));
	assert(overlay.VisibleNotice(0).empty());

	overlay.AddNotice("RA: test", 100, 50);
	assert(overlay.HasVisibleNotice(100));
	assert(overlay.VisibleNotice(120) == "RA: test");
	assert(!overlay.HasVisibleNotice(150));
	assert(overlay.VisibleNotice(150).empty());

	overlay.AddNotice("", 200, 50);
	assert(!overlay.HasVisibleNotice(200));

	Xm8Ra::RaOverlaySnapshot snapshot;
	snapshot.mode_enabled = true;
	snapshot.hardcore_enabled = true;
	snapshot.status_text = "RA: logged in";
	snapshot.user_name = "tester";
	snapshot.game_title = "Sample";
	snapshot.rich_presence = "Playing";
	overlay.SetSnapshot(snapshot);

	assert(overlay.Snapshot().mode_enabled);
	assert(overlay.Snapshot().hardcore_enabled);
	assert(overlay.Snapshot().status_text == "RA: logged in");
	assert(overlay.Snapshot().user_name == "tester");
	assert(overlay.Snapshot().game_title == "Sample");
	assert(overlay.Snapshot().rich_presence == "Playing");

	Xm8Ra::RaOverlayAchievementListSnapshot achievements;
	achievements.game_loaded = true;
	achievements.has_achievements = true;
	achievements.game_title = "Test Game";
	Xm8Ra::RaOverlayAchievementItem achievement;
	achievement.id = 42;
	achievement.points = 5;
	achievement.unlocked = 1;
	achievement.title = "First";
	achievement.description = "Description";
	achievement.bucket_label = "Unlocked";
	achievements.achievements.push_back(achievement);
	for (int i = 0; i < 6; ++i) {
		Xm8Ra::RaOverlayAchievementItem extra;
		extra.id = static_cast<uint32_t>(100 + i);
		extra.points = static_cast<uint32_t>(i + 1);
		extra.title = "Extra";
		extra.description = "Extra description";
		achievements.achievements.push_back(extra);
	}
	overlay.OpenAchievements(achievements);
	assert(overlay.IsBlocking());
	assert(overlay.Screen() == Xm8Ra::RaOverlayScreen::Achievements);
	assert(overlay.AchievementListSnapshot().active);
	assert(overlay.AchievementListSnapshot().game_title == "Test Game");
	assert(overlay.AchievementListSnapshot().achievements.size() == 7);
	assert(overlay.AchievementListSnapshot().achievements[0].title == "First");
	assert(overlay.AchievementListSnapshot().selected_index == 0);
	assert(overlay.OnControlKey(Xm8Ra::RaOverlayKey::Down) ==
		Xm8Ra::RaOverlayAction::None);
	assert(overlay.AchievementListSnapshot().selected_index == 1);
	for (int i = 0; i < 5; ++i) {
		overlay.OnControlKey(Xm8Ra::RaOverlayKey::Down);
	}
	assert(overlay.AchievementListSnapshot().selected_index == 6);
	assert(overlay.AchievementListSnapshot().first_visible_index == 2);
	assert(overlay.OnAchievementPointer(90, 126, false) ==
		Xm8Ra::RaOverlayAction::None);
	assert(overlay.AchievementListSnapshot().selected_index == 2);
	assert(overlay.OnAchievementPointer(10, 10, false) ==
		Xm8Ra::RaOverlayAction::None);
	assert(overlay.IsBlocking());
	assert(overlay.OnControlKey(Xm8Ra::RaOverlayKey::Enter) ==
		Xm8Ra::RaOverlayAction::None);
	assert(overlay.IsBlocking());
	assert(overlay.OnAchievementPointer(10, 10, true) ==
		Xm8Ra::RaOverlayAction::Close);
	assert(!overlay.IsBlocking());

	overlay.OpenAchievements(achievements);
	assert(overlay.OnControlKey(Xm8Ra::RaOverlayKey::Escape) ==
		Xm8Ra::RaOverlayAction::Close);
	assert(!overlay.IsBlocking());

	overlay.OpenLogin();
	assert(overlay.IsBlocking());
	assert(overlay.Screen() == Xm8Ra::RaOverlayScreen::Login);
	assert(overlay.LoginSnapshot().field ==
		Xm8Ra::RaOverlayLoginField::Username);
	assert(!overlay.LoginSnapshot().can_submit);

	overlay.OnTextInput("player");
	assert(overlay.LoginSnapshot().username == "player");
	assert(overlay.OnControlKey(Xm8Ra::RaOverlayKey::Enter) ==
		Xm8Ra::RaOverlayAction::None);
	assert(overlay.LoginSnapshot().field ==
		Xm8Ra::RaOverlayLoginField::Password);
	overlay.OnTextInput("secret");
	assert(overlay.LoginSnapshot().masked_password == "******");
	assert(overlay.LoginSnapshot().can_submit);

	assert(overlay.OnControlKey(Xm8Ra::RaOverlayKey::Backspace) ==
		Xm8Ra::RaOverlayAction::None);
	assert(overlay.LoginSnapshot().masked_password == "*****");
	overlay.OnTextInput("t");
	assert(overlay.OnControlKey(Xm8Ra::RaOverlayKey::Enter) ==
		Xm8Ra::RaOverlayAction::SubmitLogin);
	assert(overlay.OnControlKey(Xm8Ra::RaOverlayKey::Enter) ==
		Xm8Ra::RaOverlayAction::None);
	assert(overlay.OnControlKey(Xm8Ra::RaOverlayKey::Escape) ==
		Xm8Ra::RaOverlayAction::None);
	assert(overlay.OnLoginTarget(Xm8Ra::RaOverlayLoginTarget::Cancel,
		true) == Xm8Ra::RaOverlayAction::None);
	assert(overlay.IsBlocking());

	std::string username;
	std::string password;
	assert(overlay.ConsumeSubmittedLogin(&username, &password));
	assert(username == "player");
	assert(password == "secret");
	assert(overlay.LoginSnapshot().masked_password.empty());
	assert(overlay.LoginSnapshot().username == "player");
	overlay.SetLoginStatus("Invalid username or password");
	assert(overlay.LoginSnapshot().status_message ==
		"Invalid username or password");
	assert(overlay.LoginSnapshot().username == "player");
	assert(overlay.LoginSnapshot().masked_password.empty());

	overlay.OpenLogin("saved-user");
	assert(overlay.LoginSnapshot().username == "saved-user");
	assert(overlay.LoginSnapshot().field ==
		Xm8Ra::RaOverlayLoginField::Password);
	assert(overlay.OnControlKey(Xm8Ra::RaOverlayKey::Escape) ==
		Xm8Ra::RaOverlayAction::Close);
	assert(!overlay.IsBlocking());

	overlay.OpenLogin();
	assert(overlay.OnLoginPointer(240, 136, true) ==
		Xm8Ra::RaOverlayAction::None);
	assert(overlay.LoginSnapshot().focus ==
		Xm8Ra::RaOverlayLoginTarget::Username);
	overlay.OnTextInput("paduser");
	assert(overlay.OnLoginPointer(240, 176, true) ==
		Xm8Ra::RaOverlayAction::None);
	assert(overlay.LoginSnapshot().focus ==
		Xm8Ra::RaOverlayLoginTarget::Password);
	overlay.OnTextInput("padpass");
	Xm8Ra::RaOverlayLoginTarget target =
		Xm8Ra::RaOverlayLoginTarget::Username;
	assert(overlay.LoginTargetAt(260, 224, &target));
	assert(target == Xm8Ra::RaOverlayLoginTarget::Login);
	assert(overlay.LoginTargetAt(390, 224, &target));
	assert(target == Xm8Ra::RaOverlayLoginTarget::Cancel);
	assert(!overlay.LoginTargetAt(10, 10, &target));
	assert(overlay.OnLoginPointer(260, 224, false) ==
		Xm8Ra::RaOverlayAction::None);
	assert(overlay.LoginSnapshot().focus ==
		Xm8Ra::RaOverlayLoginTarget::Login);
	assert(overlay.OnLoginPointer(260, 224, true) ==
		Xm8Ra::RaOverlayAction::SubmitLogin);
	assert(overlay.ConsumeSubmittedLogin(&username, &password));
	assert(username == "paduser");
	assert(password == "padpass");
	overlay.SetLoginStatus("retry");

	overlay.OpenLogin();
	overlay.OnControlKey(Xm8Ra::RaOverlayKey::Down);
	assert(overlay.LoginSnapshot().focus ==
		Xm8Ra::RaOverlayLoginTarget::Password);
	overlay.OnControlKey(Xm8Ra::RaOverlayKey::Down);
	assert(overlay.LoginSnapshot().focus ==
		Xm8Ra::RaOverlayLoginTarget::Login);
	assert(overlay.OnControlKey(Xm8Ra::RaOverlayKey::Enter) ==
		Xm8Ra::RaOverlayAction::None);
	assert(overlay.LoginSnapshot().status_message ==
		"Enter username and password");
	overlay.OnControlKey(Xm8Ra::RaOverlayKey::Right);
	assert(overlay.LoginSnapshot().focus ==
		Xm8Ra::RaOverlayLoginTarget::Cancel);
	assert(overlay.OnControlKey(Xm8Ra::RaOverlayKey::Enter) ==
		Xm8Ra::RaOverlayAction::Close);
	assert(!overlay.IsBlocking());

	overlay.Clear();
	assert(!overlay.HasVisibleNotice(201));
	assert(!overlay.Snapshot().mode_enabled);
	assert(overlay.Snapshot().status_text.empty());
	return 0;
}
