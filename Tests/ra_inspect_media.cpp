#include "ra_media_probe.h"
#include "ra_library.h"
#include "ra_media_store.h"

#include <cstdlib>
#include <iostream>

int main(int argc, char *argv[])
{
	std::string ra_root;
	const char *media_path = nullptr;
	if (argc == 2) {
		media_path = argv[1];
	}
	else if (argc == 4 && argv[1] != nullptr &&
		std::string(argv[1]) == "--import-root") {
		ra_root = argv[2] != nullptr ? argv[2] : "";
		media_path = argv[3];
	}
	else {
		std::cerr << "Usage: ra_inspect_media [--import-root <ra-root>] "
			"<file.d88>\n";
		return EXIT_FAILURE;
	}

	Xm8Ra::D88MediaInfo media;
	std::string error;
	if (media_path == nullptr ||
		!Xm8Ra::ProbeD88File(media_path, &media, &error)) {
		std::cerr << "Failed to inspect D88: " << error << '\n';
		return EXIT_FAILURE;
	}

	std::cout << "path=" << media_path << '\n'
		<< "size=" << media.size << '\n'
		<< "media_md5=" << media.md5 << '\n'
		<< "banks=" << media.banks << '\n';
	for (int i = 0; i < media.banks; i++) {
		std::cout << "bank[" << i << "].name=" << media.bank_names[i] << '\n'
			<< "bank[" << i << "].ra_hash=" << media.bank_md5s[i] << '\n';
	}

	if (!ra_root.empty()) {
		Xm8Ra::RaLibrary library;
		if (!library.Open(ra_root, &error)) {
			std::cerr << "Failed to open RA library: " << error << '\n';
			return EXIT_FAILURE;
		}
		Xm8Ra::RaMediaStore store(&library);
		Xm8Ra::ImportedMedia imported;
		if (!store.ImportDesktopD88(media_path, &imported, &error)) {
			std::cerr << "Failed to import D88: " << error << '\n';
			return EXIT_FAILURE;
		}
		Xm8Ra::ResolvedLaunchProfile launch;
		if (!store.ResolveLaunchProfile(imported.record.game_id, &launch,
			&error)) {
			std::cerr << "Failed to resolve launch profile: " << error << '\n';
			return EXIT_FAILURE;
		}
		std::cout << "import.game_id=" << imported.record.game_id << '\n'
			<< "import.copied=" << (imported.copied ? 1 : 0) << '\n'
			<< "import.working_path=" << imported.working_path << '\n'
			<< "launch.anchor_md5=" << launch.anchor_md5 << '\n'
			<< "launch.drive0.path=" << launch.drives[0].working_path << '\n'
			<< "launch.drive0.ra_hash="
			<< media.bank_md5s[launch.drives[0].bank_index] << '\n';
	}
	return EXIT_SUCCESS;
}
