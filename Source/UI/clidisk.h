#ifndef CLIDISK_H
#define CLIDISK_H
#ifdef __min
#pragma push_macro("__min")
#undef __min
#define CLIDISK_RESTORE_INTERNAL_MIN_MACRO
#endif

#ifdef min
#pragma push_macro("min")
#undef min
#define CLIDISK_RESTORE_MIN_MACRO
#endif
#ifdef max
#pragma push_macro("max")
#undef max
#define CLIDISK_RESTORE_MAX_MACRO
#endif

#include <string>
#include <vector>

#ifdef CLIDISK_RESTORE_INTERNAL_MIN_MACRO
#pragma pop_macro("__min")
#undef CLIDISK_RESTORE_INTERNAL_MIN_MACRO
#endif
#ifdef CLIDISK_RESTORE_MAX_MACRO
#pragma pop_macro("max")
#undef CLIDISK_RESTORE_MAX_MACRO
#endif
#ifdef CLIDISK_RESTORE_MIN_MACRO
#pragma pop_macro("min")
#undef CLIDISK_RESTORE_MIN_MACRO
#endif

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
