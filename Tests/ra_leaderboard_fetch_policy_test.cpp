#include "ra_leaderboard_fetch_policy.h"

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
	using Xm8Ra::ShouldCheckLeaderboardEntriesAfterSelection;
	using Xm8Ra::ShouldFetchLeaderboardEntries;

	Check(ShouldCheckLeaderboardEntriesAfterSelection(10, 11),
		"moving to another leaderboard checks entries");
	Check(!ShouldCheckLeaderboardEntriesAfterSelection(10, 10),
		"stable selection does not refetch every frame");
	Check(!ShouldCheckLeaderboardEntriesAfterSelection(10, 0),
		"empty selection does not fetch");

	Check(ShouldFetchLeaderboardEntries(11, false, 10, true),
		"newly selected leaderboard starts a fetch");
	Check(ShouldFetchLeaderboardEntries(11, false, 11, false),
		"selected leaderboard retries when no request state exists");
	Check(!ShouldFetchLeaderboardEntries(11, false, 11, true),
		"pending, loaded, or failed request is not duplicated");
	Check(!ShouldFetchLeaderboardEntries(11, true, 10, false),
		"event scoreboard cache avoids redundant fetch");
	Check(!ShouldFetchLeaderboardEntries(0, false, 0, false),
		"invalid leaderboard ID is ignored");

	if (failures != 0) return EXIT_FAILURE;
	std::cout << "RA leaderboard fetch policy tests passed\n";
	return EXIT_SUCCESS;
}
