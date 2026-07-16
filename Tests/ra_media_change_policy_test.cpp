#include "ra_media_change_policy.h"

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
	using Xm8Ra::ClassifyMediaChange;
	using Xm8Ra::RaMediaChangeAction;
	const std::string old_hash(32, 'a');
	const std::string new_hash(32, 'b');

	Check(ClassifyMediaChange(0, true, false, false, 7, old_hash, 7, new_hash) ==
		RaMediaChangeAction::BeginSameGameChange,
		"same-game Drive 1 media starts RA change");
	Check(ClassifyMediaChange(0, true, true, false, 7, old_hash, 7, new_hash) ==
		RaMediaChangeAction::RejectPending,
		"second media change is rejected while pending");
	Check(ClassifyMediaChange(0, true, false, false, 7, old_hash, 8, new_hash) ==
		RaMediaChangeAction::RejectDifferentGame,
		"different-game media is rejected");
	Check(ClassifyMediaChange(0, true, false, false, 0, old_hash, 7, new_hash) ==
		RaMediaChangeAction::RejectDifferentGame,
		"media change requires known active library ownership");
	Check(ClassifyMediaChange(1, true, false, false, 7, old_hash, 8, new_hash) ==
		RaMediaChangeAction::NoChange,
		"Drive 2 never changes active RA media");
	Check(ClassifyMediaChange(0, true, false, false, 7, old_hash, 7, old_hash) ==
		RaMediaChangeAction::NoChange,
		"same RA hash does not invoke media change");
	Check(ClassifyMediaChange(0, true, false, true, 7, old_hash, 7, new_hash) ==
		RaMediaChangeAction::NoChange,
		"bank switching in one working D88 does not change RA media");
	Check(ClassifyMediaChange(0, false, false, false, 0, "", 7, new_hash) ==
		RaMediaChangeAction::NoChange,
		"initial media load is not classified as media change");

	if (failures != 0) {
		return EXIT_FAILURE;
	}
	std::cout << "RA media change policy tests passed\n";
	return EXIT_SUCCESS;
}
