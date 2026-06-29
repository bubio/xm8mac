#include "Fixtures/d88_fixture.h"
#include "ra_library.h"
#include "ra_media_store.h"

#include <chrono>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sys/stat.h>
#include <string>
#include <vector>

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
		failures++;
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

bool MakeDirectoryTree(const std::string& path, std::string *error)
{
	std::string current;
	size_t index = 0;
	if (!path.empty() && path[0] == '/') {
		current = "/";
		index = 1;
	}

	while (index <= path.size()) {
		const size_t slash = path.find('/', index);
		const std::string part = path.substr(index,
			slash == std::string::npos ? std::string::npos : slash - index);
		if (!part.empty()) {
			if (!current.empty() && current.back() != '/') {
				current += '/';
			}
			current += part;
			if (mkdir(current.c_str(), 0755) != 0 && errno != EEXIST) {
				if (error != nullptr) {
					*error = std::strerror(errno);
				}
				return false;
			}
		}
		if (slash == std::string::npos) {
			break;
		}
		index = slash + 1;
	}
	return true;
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
				if (name == "." || name == "..") {
					continue;
				}
				RemoveTree(JoinPath(path, name.c_str()));
			}
			closedir(dir);
		}
		rmdir(path.c_str());
	}
	else {
		unlink(path.c_str());
	}
}

std::vector<char> ReadFile(const std::string& path)
{
	std::ifstream stream(path, std::ios::binary);
	return std::vector<char>(std::istreambuf_iterator<char>(stream),
		std::istreambuf_iterator<char>());
}

bool AppendByte(const std::string& path, char value)
{
	std::ofstream stream(path, std::ios::binary | std::ios::app);
	if (!stream.is_open()) {
		return false;
	}
	stream.put(value);
	return stream.good();
}

bool WriteTextFile(const std::string& path, const char *text)
{
	std::ofstream stream(path, std::ios::binary | std::ios::trunc);
	if (!stream.is_open()) {
		return false;
	}
	stream << text;
	return stream.good();
}

bool CopyFileBytes(const std::string& source, const std::string& destination)
{
	std::ifstream input(source, std::ios::binary);
	std::ofstream output(destination, std::ios::binary | std::ios::trunc);
	if (!input.is_open() || !output.is_open()) {
		return false;
	}
	output << input.rdbuf();
	return input.good() && output.good();
}

bool DirectoryHasPrefix(const std::string& path, const char *prefix)
{
	DIR *dir = opendir(path.c_str());
	if (dir == nullptr) {
		return false;
	}
	const std::string prefix_string = prefix;
	bool found = false;
	while (dirent *entry = readdir(dir)) {
		const std::string name = entry->d_name;
		if (name.compare(0, prefix_string.size(), prefix_string) == 0) {
			found = true;
			break;
		}
	}
	closedir(dir);
	return found;
}

} // namespace

int main()
{
	const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
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
	) + "/xm8-ra-library-store-" + std::to_string(unique);
	const std::string source_dir = JoinPath(base, "source");
	const std::string ra_root = JoinPath(base, "ra");

	std::string error;
	Check(MakeDirectoryTree(base, &error), "create test root");
	if (!error.empty()) {
		std::cerr << error << '\n';
	}
	Check(D88Fixture::GenerateStandardSet(source_dir, &error),
		"generate fixture set");
	if (!error.empty()) {
		std::cerr << error << '\n';
	}

	Xm8Ra::RaLibrary library;
	Check(library.Open(ra_root, &error), "open RA library");
	if (!error.empty()) {
		std::cerr << error << '\n';
	}
	Check(PathExists(JoinPath(ra_root, "library.sqlite3")),
		"library DB exists");

	Xm8Ra::RaSettings settings;
	Check(library.LoadSettings(&settings, &error), "load default RA settings");
	Check(!settings.enabled, "RA settings default disabled");
	Check(settings.last_mode == Xm8Ra::kRaModeSoftcore,
		"RA settings default softcore");
	settings.enabled = true;
	settings.last_mode = Xm8Ra::kRaModeHardcore;
	settings.unofficial_enabled = true;
	settings.notification_seconds = 8;
	settings.image_cache_limit_mib = 256;
	Check(library.SaveSettings(settings, &error), "save RA settings");

	Xm8Ra::RaMediaStore store(&library);
	Xm8Ra::ImportedMedia first;
	const std::string single = JoinPath(source_dir, "single.d88");
	const auto original_single = ReadFile(single);
	Check(store.ImportDesktopD88(single, &first, &error), "import single D88");
	if (!error.empty()) {
		std::cerr << error << '\n';
	}
	Check(first.record.md5 == "5c50ca4f9e3a7afbe4d6666e8974949d",
		"single md5 registered");
	Check(first.record.game_id > 0, "game id assigned");
	Check(first.copied, "first import created working copy");
	Check(ReadFile(first.working_path) == original_single,
		"working copy equals original at creation");
	Check(ReadFile(single) == original_single, "original remains unchanged");
	Check(first.working_path.find("/ra/media/") != std::string::npos,
		"working copy is under RA media root");

	Xm8Ra::ImportedMedia duplicate;
	Check(store.ImportDesktopD88(single, &duplicate, &error),
		"duplicate import");
	Check(duplicate.record.md5 == first.record.md5,
		"duplicate md5 reused");
	Check(duplicate.record.game_id == first.record.game_id,
		"duplicate game reused");
	Check(!duplicate.copied, "duplicate did not overwrite working copy");
	Check(ReadFile(duplicate.working_path) == original_single,
		"duplicate working copy unchanged");

	Check(AppendByte(first.working_path, '\x24'),
		"simulate save data in working copy");
	Check(ReadFile(single) == original_single,
		"simulated save data does not change original");
	std::string reset_path;
	Check(store.ResetWorkingCopy(single, first.record.md5, &reset_path, &error),
		"reset working copy from matching original");
	Check(reset_path == first.working_path, "reset targets same working path");
	Check(ReadFile(first.working_path) == original_single,
		"reset working copy restores original bytes");

	Xm8Ra::ImportedPlaylist playlist;
	Check(store.ImportM3U(JoinPath(source_dir, "pair.m3u"), &playlist, &error),
		"import M3U playlist");
	Check(playlist.media.size() == 2, "M3U imports two media entries");
	Check(playlist.game_id == first.record.game_id,
		"M3U reuses first media game");
	Check(playlist.anchor_md5 == first.record.md5,
		"M3U anchor is first media");
	if (playlist.media.size() == 2) {
		Check(playlist.media[0].record.game_id == playlist.game_id,
			"M3U first media game id");
		Check(playlist.media[1].record.game_id == playlist.game_id,
			"M3U second media game id");
		Check(playlist.media[1].record.md5 ==
			"ff400f51a2567419b3778691a905952e",
			"M3U second media md5");
		Check(playlist.media[1].working_path.find("/ra/media/") !=
			std::string::npos,
			"M3U second working copy is under RA media root");
	}

	const std::string folder_dir = JoinPath(base, "folder-scan");
	const std::string nested_dir = JoinPath(folder_dir, "nested");
	Check(MakeDirectoryTree(nested_dir, &error), "create folder scan tree");
	Check(CopyFileBytes(JoinPath(source_dir, "single.d88"),
		JoinPath(nested_dir, "alpha.D88")), "copy uppercase D88");
	Check(CopyFileBytes(JoinPath(source_dir, "second.d88"),
		JoinPath(nested_dir, "beta.d88")), "copy second D88");
	Check(WriteTextFile(JoinPath(nested_dir, "group.M3U"),
		"# recursive import fixture\nalpha.D88#0\nbeta.d88#0\n"),
		"write uppercase M3U");
	Xm8Ra::ImportedFolder folder_import;
	Check(store.ImportFolderRecursive(folder_dir, &folder_import, &error),
		"recursive folder import");
	Check(folder_import.scanned_candidates == 3,
		"recursive folder scans D88 and M3U candidates");
	Check(folder_import.playlists.size() == 1,
		"recursive folder imports playlist");
	Check(folder_import.standalone_media.size() == 2,
		"recursive folder imports standalone D88 after playlist");
	if (!folder_import.playlists.empty()) {
		Check(folder_import.playlists[0].media.size() == 2,
			"recursive playlist has two media");
		Check(folder_import.playlists[0].anchor_md5 ==
			"5c50ca4f9e3a7afbe4d6666e8974949d",
			"recursive playlist anchor uses first D88");
	}

	Check(AppendByte(single, '\x55'), "modify source fixture");
	Check(!store.ResetWorkingCopy(single, first.record.md5, &reset_path, &error),
		"reset rejects modified original");
	Xm8Ra::ImportedMedia modified;
	Check(store.ImportDesktopD88(single, &modified, &error),
		"modified source imports as new medium");
	Check(modified.record.md5 != first.record.md5,
		"modified source has distinct md5");
	Check(ReadFile(first.working_path) == original_single,
		"existing working save is not overwritten");

	library.Close();

	Xm8Ra::RaLibrary reopened;
	Check(reopened.Open(ra_root, &error), "reopen RA library");
	Xm8Ra::RaSettings persisted;
	Check(reopened.LoadSettings(&persisted, &error),
		"load persisted RA settings");
	Check(persisted.enabled, "RA enabled setting persisted");
	Check(persisted.last_mode == Xm8Ra::kRaModeHardcore,
		"RA hardcore setting persisted");
	Check(persisted.unofficial_enabled,
		"RA unofficial setting persisted");
	Check(persisted.notification_seconds == 8,
		"RA notification setting persisted");
	Check(persisted.image_cache_limit_mib == 256,
		"RA image cache setting persisted");
	Xm8Ra::RaSettings invalid = persisted;
	invalid.last_mode = 99;
	Check(!reopened.SaveSettings(invalid, &error),
		"reject invalid RA mode");
	reopened.Close();

	const std::string corrupt_root = JoinPath(base, "corrupt-ra");
	Check(MakeDirectoryTree(corrupt_root, &error), "create corrupt DB root");
	Check(WriteTextFile(JoinPath(corrupt_root, "library.sqlite3"),
		"this is not a sqlite database"),
		"write corrupt DB");
	Xm8Ra::RaLibrary recovered;
	Check(recovered.Open(corrupt_root, &error), "recover corrupt RA DB");
	Check(DirectoryHasPrefix(corrupt_root, "library.sqlite3.corrupt."),
		"corrupt DB quarantined");
	Check(recovered.LoadSettings(&settings, &error),
		"new settings exist after corrupt DB recovery");
	Check(!settings.enabled, "recovered DB settings default disabled");
	recovered.Close();

	RemoveTree(base);

	if (failures != 0) {
		std::cerr << failures << " test(s) failed\n";
		return EXIT_FAILURE;
	}
	std::cout << "RA library/store tests passed\n";
	return EXIT_SUCCESS;
}
