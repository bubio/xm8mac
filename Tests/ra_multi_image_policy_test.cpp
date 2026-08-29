#include "ra_media_change_policy.h"

#include <cstdlib>
#include <iostream>
#include <string>

int main()
{
	bool ok = true;
	auto check = [&ok](bool condition, const char *message) {
		if (!condition) {
			std::cerr << "FAIL: " << message << '\n';
			ok = false;
		}
	};

	const Xm8Ra::RaMediaMountPlan pair =
		Xm8Ra::PlanRaMediaMount(false, true, 2);
	check(!pair.wait_for_ra_approval,
		"fresh multi-image launch does not wait for Drive 2 validation");
	check(pair.drive2_action_after_approval ==
		Xm8Ra::RaDrive2MountAction::OpenBank1,
		"multi-image launch maps bank 1 to Drive 2");

	Xm8Ra::RaDiskPolicyContext bank_switch;
	bank_switch.ra_enabled = true;
	bank_switch.role = Xm8Ra::RaDiskRole::Anchor;
	bank_switch.active_game_loaded = true;
	bank_switch.same_media_container = true;
	bank_switch.active_hash = std::string(32, 'a');
	bank_switch.target_hash = std::string(32, 'b');
	check(Xm8Ra::ClassifyRaDiskAction(bank_switch) ==
		Xm8Ra::RaDiskAction::MountNormal,
		"bank switch in one D88 does not change RA active media");

	return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
