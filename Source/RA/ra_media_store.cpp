#include "ra_media_store.h"

#include "m3u.h"

#include <chrono>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <random>
#include <sys/stat.h>
#include <unistd.h>

namespace Xm8Ra {
namespace {

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

bool PathExists(const std::string& path)
{
	struct stat st;
	return stat(path.c_str(), &st) == 0;
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

bool RaMediaStore::ImportD88IntoGame(const std::string& source_path,
	int64_t game_id, int ordinal, ImportedMedia *imported, std::string *error)
{
	if (library_ == nullptr || imported == nullptr) {
		if (error != nullptr) {
			*error = "invalid argument";
		}
		return false;
	}

	D88MediaInfo media;
	if (!ProbeD88File(source_path.c_str(), &media, error)) {
		return false;
	}

	std::string working_path;
	bool copied = false;
	if (!EnsureWorkingCopy(source_path, media, &working_path, &copied, error)) {
		return false;
	}

	MediaRecord record;
	const bool registered = game_id > 0 ?
		library_->RegisterDesktopMediaInGame(media, source_path,
			DisplayNameForPath(source_path), FileMtime(source_path),
			game_id, ordinal, &record, error) :
		library_->RegisterDesktopMedia(media, source_path,
			DisplayNameForPath(source_path), FileMtime(source_path),
			&record, error);
	if (!registered) {
		if (copied) {
			RemoveFile(working_path, nullptr);
		}
		return false;
	}

	imported->record = record;
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

	if (PathExists(target)) {
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
