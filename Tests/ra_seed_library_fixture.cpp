#include "Fixtures/ra_library_fixture_seed.h"
#include "ra_paths.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void PrintUsage()
{
	std::cerr
		<< "usage: ra_seed_library_fixture "
		<< "(--setting-dir <dir> | --ra-root <dir>)\n";
}

} // namespace

int main(int argc, char *argv[])
{
	std::string ra_root;
	for (int i = 1; i < argc; ++i) {
		const std::string arg = argv[i] != nullptr ? argv[i] : "";
		if (arg == "--setting-dir") {
			if (!ra_root.empty() || ++i >= argc) {
				PrintUsage();
				return EXIT_FAILURE;
			}
			ra_root = Xm8Ra::RootFromSettingDir(argv[i]);
		}
		else if (arg == "--ra-root") {
			if (!ra_root.empty() || ++i >= argc) {
				PrintUsage();
				return EXIT_FAILURE;
			}
			ra_root = argv[i] != nullptr ? argv[i] : "";
		}
		else {
			PrintUsage();
			return EXIT_FAILURE;
		}
	}

	if (ra_root.empty()) {
		PrintUsage();
		return EXIT_FAILURE;
	}

	RaLibraryFixtureSeed::SeedResult result;
	std::string error;
	if (!RaLibraryFixtureSeed::Seed(ra_root, &result, &error)) {
		std::cerr << error << '\n';
		return EXIT_FAILURE;
	}

	std::cout << "seeded RA library fixture\n";
	std::cout << "ra_root: " << result.ra_root << '\n';
	std::cout << "source_dir: " << result.source_dir << '\n';
	for (const auto& game : result.games) {
		std::cout << "game: " << game.title
			<< " game_id=" << game.game_id
			<< " ra_game_id=" << game.ra_game_id << '\n';
		for (const auto& media : game.media) {
			std::cout << "  source: " << media.source_path << '\n';
			std::cout << "  working: " << media.working_path << '\n';
			std::cout << "  md5: " << media.md5 << '\n';
		}
	}
	return EXIT_SUCCESS;
}
