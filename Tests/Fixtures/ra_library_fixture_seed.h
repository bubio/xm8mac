#ifndef TESTS_FIXTURES_RA_LIBRARY_FIXTURE_SEED_H
#define TESTS_FIXTURES_RA_LIBRARY_FIXTURE_SEED_H

#include <cstdint>
#include <string>
#include <vector>

namespace RaLibraryFixtureSeed {

struct SeededMedia {
	std::string source_path;
	std::string working_path;
	std::string md5;
};

struct SeededGame {
	int64_t game_id = 0;
	int64_t ra_game_id = 0;
	std::string title;
	std::vector<SeededMedia> media;
};

struct SeedResult {
	std::string ra_root;
	std::string source_dir;
	std::vector<SeededGame> games;
};

bool Seed(const std::string& ra_root, SeedResult *result, std::string *error);

} // namespace RaLibraryFixtureSeed

#endif
