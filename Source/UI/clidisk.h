#ifndef CLIDISK_H
#define CLIDISK_H

#include <string>
#include <vector>

enum class CliAction {
	Run,
	ShowHelp,
	ShowVersion,
	Error
};

enum class CliSystemMode {
	Unspecified,
	V1S,
	V1H,
	V2,
	N
};

enum class CliClockMode {
	Unspecified,
	Clock4MHz,
	Clock8MHz,
	Clock8MHzH
};

struct DiskSpec {
	std::string path;
	int bank;
	int drive;
};

struct CliOptions {
	CliAction action = CliAction::Run;
	CliSystemMode system = CliSystemMode::Unspecified;
	CliClockMode clock = CliClockMode::Unspecified;
	std::vector<DiskSpec> disks;
	std::string error;
};

CliOptions ParseCommandLine(int argc, char *argv[]);
const char* GetCommandLineHelp();

#endif
