#include "ra_menu_status.h"

#include <cstdlib>
#include <iostream>

namespace {

int failures = 0;

void Check(bool condition, const char *message)
{
	if (!condition) {
		std::cerr << "FAIL: " << message << '\n';
		failures++;
	}
}

} // namespace

int main()
{
	Xm8Ra::RaMenuStatus status;
	Check(status.State() == Xm8Ra::RaMenuStatusState::Unavailable,
		"default status is unavailable");

	status.Set(Xm8Ra::RaMenuStatusState::UnknownGame);
	Check(status.State() == Xm8Ra::RaMenuStatusState::UnknownGame,
		"unknown game is retained after the game service unloads");
	status.EnterOfflineSession();
	Check(status.State() == Xm8Ra::RaMenuStatusState::UnknownGame,
		"offline cleanup preserves unknown game");

	status.Set(Xm8Ra::RaMenuStatusState::PendingGame, "12345678");
	Check(status.State() == Xm8Ra::RaMenuStatusState::PendingGame,
		"next game launch replaces unknown game");
	Check(status.Detail() == "12345678", "pending hash is retained");

	status.Set(Xm8Ra::RaMenuStatusState::Disabled);
	Check(status.State() == Xm8Ra::RaMenuStatusState::Disabled,
		"mode off replaces an active display state");

	status.Set(Xm8Ra::RaMenuStatusState::ActiveGame, "Example Game");
	Check(status.State() == Xm8Ra::RaMenuStatusState::ActiveGame,
		"identified game becomes active");
	Check(status.Detail() == "Example Game", "active game title is retained");

	status.SetConnectivity(true);
	Check(status.State() == Xm8Ra::RaMenuStatusState::Disconnected,
		"disconnect updates the status row");
	Check(status.Detail() == "Example Game",
		"disconnect preserves the active game title");
	status.SetConnectivity(false);
	Check(status.State() == Xm8Ra::RaMenuStatusState::ActiveGame,
		"reconnect restores the active game status");

	status.Set(Xm8Ra::RaMenuStatusState::LoggedIn, "Player");
	status.EnterOfflineSession();
	Check(status.State() == Xm8Ra::RaMenuStatusState::OfflineSession,
		"ordinary offline sessions are shown as offline");

	if (failures != 0) {
		return EXIT_FAILURE;
	}
	std::cout << "RA menu status tests passed\n";
	return EXIT_SUCCESS;
}
