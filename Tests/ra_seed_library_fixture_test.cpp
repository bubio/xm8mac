#include "Fixtures/ra_library_fixture_seed.h"
#include "ra_library.h"
#include "ra_media_store.h"

#include <chrono>
#include <cerrno>
#include <cstdlib>
#include <dirent.h>
#include <iostream>
#include <string>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

namespace {

int failures = 0;

void Check(bool condition, const char *message)
{
	if (!condition) {
		std::cerr << "FAIL: " << message << '\n';
		++failures;
	}
}

std::string JoinPath(const std::string& dir, const char *name)
{
	if (!dir.empty() && dir.back() == '/') {
		return dir + name;
	}
	return dir + "/" + name;
}

bool PathExists(const std::string& path)
{
	struct stat st;
	return stat(path.c_str(), &st) == 0;
}

bool MakeDirectory(const std::string& path)
{
#ifdef _WIN32
	return _mkdir(path.c_str()) == 0 || errno == EEXIST;
#else
	return mkdir(path.c_str(), 0755) == 0 || errno == EEXIST;
#endif
}

void RemoveTree(const std::string& path)
{
	struct stat st;
	if (lstat(path.c_str(), &st) != 0) {
		return;
	}
	if (S_ISDIR(st.st_mode)) {
		DIR *dir = opendir(path.c_str());
		if (dir != nullptr) {
			while (dirent *entry = readdir(dir)) {
				const std::string name = entry->d_name;
				if (name != "." && name != "..") {
					RemoveTree(JoinPath(path, name.c_str()));
				}
			}
			closedir(dir);
		}
		rmdir(path.c_str());
	}
	else {
		unlink(path.c_str());
	}
}

} // namespace

int main()
{
	const auto unique =
		std::chrono::steady_clock::now().time_since_epoch().count();
	const char *temporary = std::getenv(
#ifdef _WIN32
		"TEMP"
#else
		"TMPDIR"
#endif
	);
	const std::string base = std::string(temporary != nullptr ? temporary :
#ifdef _WIN32
		"."
#else
		"/tmp"
#endif
	) + "/xm8-ra-seed-fixture-" + std::to_string(unique);
	const std::string ra_root = JoinPath(base, "ra");

	Check(MakeDirectory(base), "create seed fixture test root");
	RaLibraryFixtureSeed::SeedResult result;
	std::string error;
	Check(RaLibraryFixtureSeed::Seed(ra_root, &result, &error),
		"seed RA library fixture");
	if (!error.empty()) {
		std::cerr << error << '\n';
	}
	Check(result.games.size() == 2, "seed result has two games");
	Check(PathExists(JoinPath(ra_root, "library.sqlite3")),
		"seed creates RA database");
	Check(PathExists(result.source_dir), "seed creates source fixture dir");

	Xm8Ra::RaLibrary library;
	Check(library.Open(ra_root, &error), "open seeded library");
	Xm8Ra::RaMediaStore store(&library);
	std::vector<Xm8Ra::RaLibraryGameListItem> games;
	Check(library.ListGames(&games, &error), "list seeded games");
	Check(games.size() == 2, "seeded library lists two RA games");

	bool found_multi = false;
	bool found_single = false;
	for (const Xm8Ra::RaLibraryGameListItem& game : games) {
		if (game.title == "RA Test Multi Disk") {
			found_multi = true;
			Check(game.ra_game_id == 8801001, "multi disk RA game id");
			Check(game.media_count == 2, "multi disk media count");
			Check(game.has_progress, "multi disk progress present");
			Xm8Ra::ResolvedLaunchProfile profile;
			Check(store.ResolveLaunchProfile(game.game_id, &profile, &error),
				"resolve multi disk launch profile");
			Check(profile.drives[0].assigned && profile.drives[1].assigned,
				"multi disk launch assigns both drives");
			Check(profile.drives[0].working_path.find("/ra/media/") !=
				std::string::npos,
				"multi disk drive 1 uses RA working copy");
			Check(profile.drives[1].working_path.find("/ra/media/") !=
				std::string::npos,
				"multi disk drive 2 uses RA working copy");
		}
		else if (game.title == "RA Test Game") {
			found_single = true;
			Check(game.ra_game_id == 8801002, "single D88 RA game id");
			Check(game.media_count == 1, "single D88 media count");
			Check(game.has_progress, "single D88 progress present");
			Xm8Ra::ResolvedLaunchProfile profile;
			Check(store.ResolveLaunchProfile(game.game_id, &profile, &error),
				"resolve single D88 launch profile");
			Check(profile.drives[0].assigned && !profile.drives[1].assigned,
				"single D88 launch assigns drive 1 only");
			Check(profile.drives[0].working_path.find("/ra/media/") !=
				std::string::npos,
				"single D88 uses RA working copy");
		}
	}
	Check(found_multi, "seeded library contains multi disk game");
	Check(found_single, "seeded library contains single D88 game");

	library.Close();
	RemoveTree(base);
	if (failures != 0) {
		std::cerr << failures << " test(s) failed\n";
		return EXIT_FAILURE;
	}
	std::cout << "RA seed fixture tests passed\n";
	return EXIT_SUCCESS;
}
