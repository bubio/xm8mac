#include "ra_media_change_policy.h"

#include <cstdlib>
#include <iostream>

int main()
{
	bool ok = true;
	auto check = [&ok](bool condition, const char *message) {
		if (!condition) {
			std::cerr << "FAIL: " << message << '\n';
			ok = false;
		}
	};

	const Xm8Ra::RaDiskLoginState login_states[] = {
		Xm8Ra::RaDiskLoginState::LoggedOut,
		Xm8Ra::RaDiskLoginState::LoginPending,
		Xm8Ra::RaDiskLoginState::LoggedIn,
		Xm8Ra::RaDiskLoginState::LoginFailed,
	};
	for (const Xm8Ra::RaDiskLoginState login : login_states) {
		for (const bool hardcore : {false, true}) {
			Xm8Ra::RaDiskPolicyContext context;
			context.ra_enabled = true;
			context.login_state = login;
			context.hardcore_selected = hardcore;
			context.role = Xm8Ra::RaDiskRole::Auxiliary;
			context.anchor_load_pending = false;
			context.anchor_change_pending = false;
			context.active_game_loaded = true;
			context.active_library_game_id = 7;
			context.target_library_game_id = 99;
			context.active_hash = std::string(32, 'a');
			context.target_hash = std::string(32, 'b');
			context.network_available = true;
			const Xm8Ra::RaDiskAction expected = hardcore ?
				Xm8Ra::RaDiskAction::VerifyAuxiliary :
				Xm8Ra::RaDiskAction::RejectDifferentGame;
			check(Xm8Ra::ClassifyRaDiskAction(context) == expected,
				"Drive 2 follows selected-mode validation policy");
		}
	}

	Xm8Ra::RaDiskPolicyContext offline;
	offline.ra_enabled = true;
	offline.session_offline = true;
	offline.hardcore_selected = true;
	offline.role = Xm8Ra::RaDiskRole::Auxiliary;
	check(Xm8Ra::ClassifyRaDiskAction(offline) ==
		Xm8Ra::RaDiskAction::MountAuxiliary,
		"offline Drive 2 mount never waits for RA validation");

	return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
