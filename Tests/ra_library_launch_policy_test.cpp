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
			context.anchor_load_pending = true;
			context.anchor_change_pending = true;
			context.active_game_loaded = true;
			context.active_library_game_id = 7;
			context.target_library_game_id = 99;
			context.active_hash = std::string(32, 'a');
			context.target_hash = std::string(32, 'b');
			check(Xm8Ra::ClassifyRaDiskAction(context) ==
				Xm8Ra::RaDiskAction::MountAuxiliary,
				"Drive 2 never enters RA validation");
		}
	}

	return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
