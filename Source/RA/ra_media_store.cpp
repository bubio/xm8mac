#include "ra_media_store.h"

#include "m3u.h"
#include "pathresolver.h"

#include <chrono>
#include <cerrno>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <random>
#include <sys/stat.h>
#include <unistd.h>

namespace Xm8Ra {
namespace {

constexpr int kMaxRecursiveCandidates = 10000;

std::string DisplayNameForPath(const std::string& source_path)
{
	const size_t slash = source_path.find_last_of("/\\");
	if (slash == std::string::npos) {
		return source_path;
	}
	return source_path.substr(slash + 1);
}

std::string JoinPath(const std::string& base, const std::string& child)
{
	if (!base.empty() && base.back() == '/') {
		return base + child;
	}
	return base + "/" + child;
}

std::string StripM3UBankSuffix(const std::string& entry)
{
	const size_t hash = entry.find_last_of('#');
	if (hash == std::string::npos || hash + 1 >= entry.size()) {
		return entry;
	}
	for (size_t i = hash + 1; i < entry.size(); i++) {
		if (entry[i] < '0' || entry[i] > '9') {
			return entry;
		}
	}
	return entry.substr(0, hash);
}

std::string ToLowerAscii(const std::string& value)
{
	std::string result = value;
	for (char& ch : result) {
		if (ch >= 'A' && ch <= 'Z') {
			ch = static_cast<char>(ch - 'A' + 'a');
		}
	}
	return result;
}

bool HasExtension(const std::string& path, const char *extension)
{
	const size_t dot = path.find_last_of('.');
	if (dot == std::string::npos) {
		return false;
	}
	return ToLowerAscii(path.substr(dot)) == extension;
}

bool PathExists(const std::string& path)
{
	struct stat st;
	return stat(path.c_str(), &st) == 0;
}

bool IsRegularFileNoFollow(const std::string& path)
{
	struct stat st;
	return lstat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

bool ResolveSourcePath(const std::string& source_path, std::string *resolved,
	std::string *error)
{
	char buffer[4096];
	if (!ResolvePathForIO(source_path.c_str(), buffer, sizeof(buffer))) {
		if (error != nullptr) {
			*error = "cannot resolve media source path";
		}
		return false;
	}
	if (resolved != nullptr) {
		*resolved = buffer;
	}
	return true;
}

bool MakeDirectoryTree(const std::string& path, std::string *error)
{
	if (path.empty()) {
		if (error != nullptr) {
			*error = "empty directory path";
		}
		return false;
	}

	std::string current;
	size_t index = 0;
	if (path[0] == '/') {
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

bool RemoveFile(const std::string& path, std::string *error)
{
	if (std::remove(path.c_str()) == 0 || errno == ENOENT) {
		return true;
	}
	if (error != nullptr) {
		*error = std::strerror(errno);
	}
	return false;
}

bool MoveFile(const std::string& source, const std::string& destination,
	std::string *error)
{
	if (std::rename(source.c_str(), destination.c_str()) == 0) {
		return true;
	}
	if (error != nullptr) {
		*error = std::strerror(errno);
	}
	return false;
}

bool CollectRecursiveCandidates(const std::string& folder_path,
	std::vector<std::string> *m3u_paths, std::vector<std::string> *d88_paths,
	int *scanned_candidates, std::string *error)
{
	DIR *dir = opendir(folder_path.c_str());
	if (dir == nullptr) {
		if (error != nullptr) {
			*error = std::strerror(errno);
		}
		return false;
	}

	std::vector<std::string> directories;
	std::vector<std::string> files;
	while (dirent *entry = readdir(dir)) {
		const std::string name = entry->d_name;
		if (name == "." || name == "..") {
			continue;
		}
		const std::string path = JoinPath(folder_path, name);
		struct stat st;
		if (lstat(path.c_str(), &st) != 0) {
			continue;
		}
		if (S_ISDIR(st.st_mode)) {
			directories.push_back(path);
		}
		else if (S_ISREG(st.st_mode)) {
			files.push_back(path);
		}
	}
	closedir(dir);

	std::sort(directories.begin(), directories.end());
	std::sort(files.begin(), files.end());

	for (const std::string& file : files) {
		if (HasExtension(file, ".m3u") || HasExtension(file, ".d88")) {
			if (*scanned_candidates >= kMaxRecursiveCandidates) {
				if (error != nullptr) {
					*error = "too many RA media candidates in folder";
				}
				return false;
			}
			(*scanned_candidates)++;
			if (HasExtension(file, ".m3u")) {
				m3u_paths->push_back(file);
			}
			else {
				d88_paths->push_back(file);
			}
		}
	}

	for (const std::string& directory : directories) {
		if (!CollectRecursiveCandidates(directory, m3u_paths, d88_paths,
			scanned_candidates, error)) {
			return false;
		}
	}

	return true;
}

std::string RandomPartName()
{
	const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
	std::random_device rd;
	std::mt19937_64 gen((static_cast<uint64_t>(rd()) << 32) ^
		static_cast<uint64_t>(now));
	std::uniform_int_distribution<uint64_t> dist;
	char buffer[64];
	std::snprintf(buffer, sizeof(buffer), "%016llx%016llx.d88.part",
		static_cast<unsigned long long>(dist(gen)),
		static_cast<unsigned long long>(dist(gen)));
	return buffer;
}

} // namespace

RaMediaStore::RaMediaStore(RaLibrary *library) : library_(library)
{
}

bool RaMediaStore::ImportDesktopD88(const std::string& source_path,
	ImportedMedia *imported, std::string *error)
{
	return ImportD88IntoGame(source_path, 0, 0, imported, error);
}

bool RaMediaStore::ImportM3U(const std::string& playlist_path,
	ImportedPlaylist *imported, std::string *error)
{
	if (imported == nullptr) {
		if (error != nullptr) {
			*error = "invalid argument";
		}
		return false;
	}
	imported->media.clear();
	imported->game_id = 0;
	imported->anchor_md5.clear();

	M3UResult playlist = LoadM3U(playlist_path);
	if (!playlist.success) {
		if (error != nullptr) {
			*error = playlist.error;
		}
		return false;
	}
	if (playlist.entries.empty()) {
		if (error != nullptr) {
			*error = "M3U contains no D88 entries";
		}
		return false;
	}

	ImportedMedia first;
	if (!ImportD88IntoGame(StripM3UBankSuffix(playlist.entries[0]), 0, 0,
		&first, error)) {
		return false;
	}
	imported->game_id = first.record.game_id;
	imported->anchor_md5 = first.record.md5;
	imported->media.push_back(first);

	for (size_t i = 1; i < playlist.entries.size(); i++) {
		ImportedMedia next;
		if (!ImportD88IntoGame(StripM3UBankSuffix(playlist.entries[i]),
			imported->game_id, static_cast<int>(i), &next, error)) {
			return false;
		}
		imported->media.push_back(next);
	}

	return true;
}

bool RaMediaStore::ImportFolderRecursive(const std::string& folder_path,
	ImportedFolder *imported, std::string *error)
{
	if (imported == nullptr) {
		if (error != nullptr) {
			*error = "invalid argument";
		}
		return false;
	}
	imported->standalone_media.clear();
	imported->playlists.clear();
	imported->scanned_candidates = 0;

	std::vector<std::string> m3u_paths;
	std::vector<std::string> d88_paths;
	if (!CollectRecursiveCandidates(folder_path, &m3u_paths, &d88_paths,
		&imported->scanned_candidates, error)) {
		return false;
	}

	for (const std::string& m3u_path : m3u_paths) {
		ImportedPlaylist playlist;
		if (!ImportM3U(m3u_path, &playlist, error)) {
			return false;
		}
		imported->playlists.push_back(playlist);
	}

	for (const std::string& d88_path : d88_paths) {
		ImportedMedia media;
		if (!ImportDesktopD88(d88_path, &media, error)) {
			return false;
		}
		imported->standalone_media.push_back(media);
	}

	return true;
}

bool RaMediaStore::ResetWorkingCopy(const std::string& source_path,
	const std::string& expected_md5, std::string *working_path,
	std::string *error)
{
	if (library_ == nullptr || working_path == nullptr ||
		expected_md5.size() != 32) {
		if (error != nullptr) {
			*error = "invalid argument";
		}
		return false;
	}

	std::string io_source_path;
	if (!ResolveSourcePath(source_path, &io_source_path, error)) {
		return false;
	}

	D88MediaInfo media;
	if (!ProbeD88File(io_source_path.c_str(), &media, error)) {
		return false;
	}
	if (media.md5 != expected_md5) {
		if (error != nullptr) {
			*error = "source media does not match registered MD5";
		}
		return false;
	}

	const std::string media_dir = JoinPath(library_->MediaRoot(), media.md5);
	const std::string target = JoinPath(media_dir, "working.d88");
	const std::string temporary = JoinPath(library_->TempRoot(), RandomPartName());
	const std::string backup = JoinPath(library_->TempRoot(),
		media.md5 + "-" + RandomPartName() + ".old");
	*working_path = target;

	if (!MakeDirectoryTree(library_->TempRoot(), error) ||
		!MakeDirectoryTree(media_dir, error)) {
		return false;
	}
	if (!CopyAndVerify(io_source_path, temporary, media, error)) {
		RemoveFile(temporary, nullptr);
		return false;
	}

	bool had_existing = false;
	if (PathExists(target)) {
		had_existing = true;
		if (!MoveFile(target, backup, error)) {
			RemoveFile(temporary, nullptr);
			return false;
		}
	}

	if (!MoveFile(temporary, target, error)) {
		RemoveFile(temporary, nullptr);
		if (had_existing) {
			MoveFile(backup, target, nullptr);
		}
		return false;
	}

	if (had_existing) {
		RemoveFile(backup, nullptr);
	}
	return true;
}

bool RaMediaStore::CheckMediaHealth(const std::string& md5,
	MediaHealthStatus *status, std::string *error)
{
	if (library_ == nullptr || status == nullptr || md5.size() != 32) {
		if (error != nullptr) {
			*error = "invalid argument";
		}
		return false;
	}

	MediaHealthRecord record;
	if (!library_->LoadMediaHealthRecord(md5, &record, error)) {
		return false;
	}

	MediaHealthStatus checked;
	checked.md5 = record.md5;

	const std::string working_path =
		JoinPath(library_->Root(), record.working_relpath);
	checked.working_exists = IsRegularFileNoFollow(working_path);
	if (checked.working_exists) {
		D88MediaInfo working;
		checked.working_probe_ok =
			ProbeD88File(working_path.c_str(), &working, nullptr);
	}

	std::string io_source_path;
	struct stat source_stat;
	checked.source_exists =
		ResolveSourcePath(record.source_locator, &io_source_path, nullptr) &&
		stat(io_source_path.c_str(),
		&source_stat) == 0 && S_ISREG(source_stat.st_mode);
	if (checked.source_exists) {
		checked.source_size = static_cast<int64_t>(source_stat.st_size);
		checked.source_mtime = static_cast<int64_t>(source_stat.st_mtime);
		checked.source_metadata_changed =
			checked.source_size != record.source_size ||
			(record.source_mtime >= 0 &&
			 checked.source_mtime != record.source_mtime);
		if (checked.source_metadata_changed) {
			D88MediaInfo source_media;
			if (ProbeD88File(io_source_path.c_str(), &source_media,
				nullptr)) {
				checked.source_hash_changed = source_media.md5 != record.md5;
			}
			else {
				checked.source_hash_changed = true;
			}
		}
	}

	if (!checked.working_exists) {
		checked.health_state = kRaMediaHealthWorkingMissing;
	}
	else if (!checked.working_probe_ok) {
		checked.health_state = kRaMediaHealthWorkingCorrupt;
	}
	else if (!checked.source_exists) {
		checked.health_state = kRaMediaHealthSourceMissing;
	}
	else if (checked.source_hash_changed) {
		checked.health_state = kRaMediaHealthSourceChanged;
	}
	else {
		checked.health_state = kRaMediaHealthOk;
	}

	if (!library_->UpdateMediaHealth(checked, error)) {
		return false;
	}
	*status = checked;
	return true;
}

bool RaMediaStore::ResolveLaunchProfile(int64_t game_id,
	ResolvedLaunchProfile *profile, std::string *error)
{
	if (library_ == nullptr || profile == nullptr || game_id <= 0) {
		if (error != nullptr) {
			*error = "invalid argument";
		}
		return false;
	}

	LaunchProfile launch;
	if (!library_->LoadLaunchProfile(game_id, &launch, error)) {
		return false;
	}

	ResolvedLaunchProfile resolved;
	resolved.game_id = game_id;
	int anchor_count = 0;
	for (int drive = 0; drive < 2; drive++) {
		const LaunchDrive& source = launch.drives[drive];
		if (!source.assigned) {
			continue;
		}

		MediaHealthRecord record;
		if (!library_->LoadMediaHealthRecord(source.media_md5, &record,
			error)) {
			return false;
		}
		if (record.game_id != game_id) {
			if (error != nullptr) {
				*error = "launch profile references media from another game";
			}
			return false;
		}

		MediaHealthStatus health;
		if (!CheckMediaHealth(source.media_md5, &health, error)) {
			return false;
		}

		std::string working_path =
			JoinPath(library_->Root(), record.working_relpath);
		if (health.health_state == kRaMediaHealthWorkingMissing ||
			health.health_state == kRaMediaHealthWorkingCorrupt) {
			if (!ResetWorkingCopy(record.source_locator, record.md5,
				&working_path, error)) {
				return false;
			}
			if (!CheckMediaHealth(source.media_md5, &health, error)) {
				return false;
			}
		}
		if (!health.working_exists || !health.working_probe_ok) {
			if (error != nullptr) {
				*error = "working copy is not available";
			}
			return false;
		}
		if (record.working_relpath.compare(0, 6, "media/") != 0) {
			if (error != nullptr) {
				*error = "working copy is outside RA media store";
			}
			return false;
		}

		ResolvedLaunchDisk& disk = resolved.drives[drive];
		disk.assigned = true;
		disk.drive = drive;
		disk.bank_index = source.bank_index;
		disk.media_md5 = source.media_md5;
		disk.working_path = working_path;
		disk.health_state = health.health_state;
		disk.is_ra_anchor = source.is_ra_anchor;
		if (source.is_ra_anchor) {
			anchor_count++;
			resolved.anchor_md5 = disk.media_md5;
			resolved.anchor_working_path = disk.working_path;
		}
	}

	if (anchor_count != 1) {
		if (error != nullptr) {
			*error = "launch profile must contain exactly one RA anchor";
		}
		return false;
	}

	*profile = resolved;
	return true;
}

bool RaMediaStore::ImportD88IntoGame(const std::string& source_path,
	int64_t game_id, int ordinal, ImportedMedia *imported, std::string *error)
{
	if (library_ == nullptr || imported == nullptr) {
		if (error != nullptr) {
			*error = "invalid argument";
		}
		return false;
	}

	std::string io_source_path;
	if (!ResolveSourcePath(source_path, &io_source_path, error)) {
		return false;
	}

	D88MediaInfo media;
	if (!ProbeD88File(io_source_path.c_str(), &media, error)) {
		return false;
	}

	std::string working_path;
	bool copied = false;
	if (!EnsureWorkingCopy(io_source_path, media, &working_path, &copied,
		error)) {
		return false;
	}

	MediaRecord record;
	const bool registered = game_id > 0 ?
		library_->RegisterDesktopMediaInGame(media, source_path,
			DisplayNameForPath(source_path), FileMtime(io_source_path),
			game_id, ordinal, &record, error) :
		library_->RegisterDesktopMedia(media, source_path,
			DisplayNameForPath(source_path), FileMtime(io_source_path),
			&record, error);
	if (!registered) {
		if (copied) {
			RemoveFile(working_path, nullptr);
		}
		return false;
	}

	imported->record = record;
	imported->media_info = media;
	imported->working_path = working_path;
	imported->copied = copied;
	return true;
}

bool RaMediaStore::EnsureWorkingCopy(const std::string& source_path,
	const D88MediaInfo& media, std::string *working_path, bool *copied,
	std::string *error)
{
	const std::string media_dir = JoinPath(library_->MediaRoot(), media.md5);
	const std::string target = JoinPath(media_dir, "working.d88");
	*working_path = target;
	*copied = false;

	if (IsRegularFileNoFollow(target)) {
		D88MediaInfo existing;
		if (ProbeD88File(target.c_str(), &existing, nullptr)) {
			return true;
		}
	}

	if (!MakeDirectoryTree(library_->TempRoot(), error) ||
		!MakeDirectoryTree(media_dir, error)) {
		return false;
	}

	const std::string temporary = JoinPath(library_->TempRoot(), RandomPartName());
	if (!CopyAndVerify(source_path, temporary, media, error)) {
		RemoveFile(temporary, nullptr);
		return false;
	}

	if (std::rename(temporary.c_str(), target.c_str()) != 0) {
		if (error != nullptr) {
			*error = std::strerror(errno);
		}
		RemoveFile(temporary, nullptr);
		return false;
	}
	*copied = true;
	return true;
}

bool RaMediaStore::CopyAndVerify(const std::string& source_path,
	const std::string& temporary_path, const D88MediaInfo& expected,
	std::string *error)
{
	std::ifstream input(source_path, std::ios::binary);
	std::ofstream output(temporary_path,
		std::ios::binary | std::ios::trunc);
	if (!input.is_open() || !output.is_open()) {
		if (error != nullptr) {
			*error = "cannot open media copy streams";
		}
		return false;
	}

	char buffer[65536];
	while (input.good()) {
		input.read(buffer, sizeof(buffer));
		const std::streamsize got = input.gcount();
		if (got > 0) {
			output.write(buffer, got);
			if (!output.good()) {
				if (error != nullptr) {
					*error = "cannot write working copy";
				}
				return false;
			}
		}
	}
	output.flush();
	output.close();
	input.close();

	D88MediaInfo copied;
	if (!ProbeD88File(temporary_path.c_str(), &copied, error)) {
		return false;
	}
	if (copied.md5 != expected.md5 || copied.size != expected.size ||
		copied.banks != expected.banks) {
		if (error != nullptr) {
			*error = "working copy verification mismatch";
		}
		return false;
	}
	return true;
}

int64_t RaMediaStore::FileMtime(const std::string& path) const
{
	struct stat st;
	if (stat(path.c_str(), &st) != 0) {
		return -1;
	}
	return static_cast<int64_t>(st.st_mtime);
}

} // namespace Xm8Ra
