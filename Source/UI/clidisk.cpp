#include "clidisk.h"

#include <algorithm>
#include <cctype>
#include <climits>
#include "m3u.h"

namespace {

std::string ToUpper(const std::string& value)
{
	std::string result = value;
	std::transform(result.begin(), result.end(), result.begin(),
		[](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
	return result;
}

CliOptions Error(const std::string& message)
{
	CliOptions options;
	options.action = CliAction::Error;
	options.error = message;
	return options;
}

bool ParseBank(const std::string& value, int *bank)
{
	unsigned long long parsed = 0;

	if (value.empty()) {
		return false;
	}
	for (char ch : value) {
		if (ch < '0' || ch > '9') {
			return false;
		}
		parsed = parsed * 10 + static_cast<unsigned long long>(ch - '0');
		if (parsed > static_cast<unsigned long long>(INT_MAX)) {
			return false;
		}
	}

	*bank = static_cast<int>(parsed);
	return true;
}

bool ParseDiskSpec(const std::string& argument, int drive, DiskSpec *spec,
	std::string *error)
{
	const std::string::size_type slash = argument.find_last_of("/\\");
	const std::string::size_type hash = argument.find_last_of('#');
	const std::string::size_type colon = argument.find_last_of(':');
	std::string::size_type separator = std::string::npos;

	if (argument.empty()) {
		*error = "empty disk path";
		return false;
	}
	if (hash != std::string::npos &&
		(slash == std::string::npos || hash > slash)) {
		separator = hash;
	}
	const bool windows_drive_colon = colon == 1 &&
		std::isalpha(static_cast<unsigned char>(argument[0])) != 0;
	if (colon != std::string::npos && !windows_drive_colon &&
		(slash == std::string::npos || colon > slash) &&
		(separator == std::string::npos || colon > separator)) {
		separator = colon;
	}

	spec->path = argument;
	spec->bank = 0;
	spec->drive = drive;
	if (separator == std::string::npos) {
		return true;
	}

	spec->path = argument.substr(0, separator);
	const std::string bank_text = argument.substr(separator + 1);
	if (spec->path.empty()) {
		*error = "empty disk path";
		return false;
	}
	if (!ParseBank(bank_text, &spec->bank)) {
		*error = "invalid bank: " + bank_text;
		return false;
	}
	return true;
}

bool ParseSystem(const std::string& value, CliSystemMode *system)
{
	const std::string normalized = ToUpper(value);
	if (normalized == "V1S") {
		*system = CliSystemMode::V1S;
	} else if (normalized == "V1H") {
		*system = CliSystemMode::V1H;
	} else if (normalized == "V2") {
		*system = CliSystemMode::V2;
	} else if (normalized == "N") {
		*system = CliSystemMode::N;
	} else {
		return false;
	}
	return true;
}

bool ParseClock(const std::string& value, CliClockMode *clock)
{
	const std::string normalized = ToUpper(value);
	if (normalized == "4" || normalized == "4MHZ") {
		*clock = CliClockMode::Clock4MHz;
	} else if (normalized == "8" || normalized == "8MHZ") {
		*clock = CliClockMode::Clock8MHz;
	} else if (normalized == "8H" || normalized == "8MHZH") {
		*clock = CliClockMode::Clock8MHzH;
	} else {
		return false;
	}
	return true;
}

bool IsM3U(const std::string& path)
{
    if (path.length() < 4) {
        return false;
    }

    std::string ext = ToUpper(path.substr(path.length() - 4));
    return ext == ".M3U";
}

} // namespace

CliOptions ParseCommandLine(int argc, char *argv[])
{
	CliOptions options;
	bool positional_only = false;
	bool system_seen = false;
	bool clock_seen = false;
	bool action_seen = false;

	for (int index = 1; index < argc; index++) {
		const std::string argument = argv[index] != nullptr ? argv[index] : "";

		if (!positional_only && argument == "--") {
			positional_only = true;
			continue;
		}
		if (!positional_only && (argument == "-h" || argument == "--help")) {
			if (action_seen) {
				return Error("multiple actions specified");
			}
			options.action = CliAction::ShowHelp;
			action_seen = true;
			continue;
		}
		if (!positional_only && argument == "--version") {
			if (action_seen) {
				return Error("multiple actions specified");
			}
			options.action = CliAction::ShowVersion;
			action_seen = true;
			continue;
		}
		if (!positional_only && argument == "--system") {
			if (system_seen) {
				return Error("duplicate option: --system");
			}
			if (++index >= argc) {
				return Error("missing value for --system");
			}
			const std::string value = argv[index] != nullptr ? argv[index] : "";
			if (!ParseSystem(value, &options.system)) {
				return Error("invalid system: " + value);
			}
			system_seen = true;
			continue;
		}
		if (!positional_only && argument == "--clock") {
			if (clock_seen) {
				return Error("duplicate option: --clock");
			}
			if (++index >= argc) {
				return Error("missing value for --clock");
			}
			const std::string value = argv[index] != nullptr ? argv[index] : "";
			if (!ParseClock(value, &options.clock)) {
				return Error("invalid clock: " + value);
			}
			clock_seen = true;
			continue;
		}
		if (!positional_only && !argument.empty() && argument[0] == '-') {
			return Error("unknown option: " + argument);
		}
		if (options.disks.size() >= 2) {
			return Error("too many disk images; maximum is 2");
		}

		if (IsM3U(argument)) {

    		M3UResult playlist = LoadM3U(argument);

    		if (!playlist.success) {
        	return Error(playlist.error);
    		}

			for (const auto& entry : playlist.entries) {
                
                if (options.disks.size() >=2) {
                    break;
                }

	        DiskSpec spec;
	        std::string error;
	
	        if (!ParseDiskSpec(entry,
	            static_cast<int>(options.disks.size()),
	            &spec,
	            &error)) {
	            return Error(error);
	        }
	
	        options.disks.push_back(spec);
	    }
	
		} else {
	
	    DiskSpec spec;
	    std::string error;
	
	    if (!ParseDiskSpec(argument,
	        static_cast<int>(options.disks.size()),
	        &spec,
	        &error)) {
	        return Error(error);
	    }
	
	    options.disks.push_back(spec);
		}
	}

	if (action_seen && (system_seen || clock_seen || !options.disks.empty())) {
		return Error("--help and --version must be used alone");
	}
	return options;
}

const char* GetCommandLineHelp()
{
	return
		"Usage: xm8 [options] [--] [<disk-spec> ...]\n"
		"\n"
		"Options:\n"
		"  --system <V1S|V1H|V2|N>\n"
		"  --clock <4|4MHz|8|8MHz|8H|8MHzH>\n"
		"  -h, --help\n"
		"  --version\n"
		"\n"
		"A disk spec is <path>, <path>#<bank>, or <path>:<bank>.\n"
		"Bank numbers are zero-based. Up to two disk images may be specified.\n";
}
