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

	std::string username;
	std::string password;
	assert(overlay.ConsumeSubmittedLogin(&username, &password));
	assert(username == "player");
	assert(password == "secret");
	assert(overlay.LoginSnapshot().masked_password.empty());

	overlay.OpenLogin("saved-user");
	assert(overlay.LoginSnapshot().username == "saved-user");
	assert(overlay.LoginSnapshot().field ==
		Xm8Ra::RaOverlayLoginField::Password);
	assert(overlay.OnControlKey(Xm8Ra::RaOverlayKey::Escape) ==
		Xm8Ra::RaOverlayAction::Close);
	assert(!overlay.IsBlocking());

	overlay.Clear();
	assert(!overlay.HasVisibleNotice(201));
	assert(!overlay.Snapshot().mode_enabled);
	assert(overlay.Snapshot().status_text.empty());
	return 0;
}
