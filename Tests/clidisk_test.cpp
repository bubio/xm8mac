#include "clidisk.h"

#include <climits>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

int failures = 0;

CliOptions Parse(std::initializer_list<const char*> arguments)
{
	std::vector<std::string> storage;
	std::vector<char*> argv;

	for (const char *argument : arguments) {
		storage.emplace_back(argument);
	}
	for (std::string& argument : storage) {
		argv.push_back(argument.data());
	}
	return ParseCommandLine(static_cast<int>(argv.size()), argv.data());
}

void Check(bool condition, const char *message)
{
	if (!condition) {
		std::cerr << "FAIL: " << message << '\n';
		failures++;
	}
}

void CheckError(std::initializer_list<const char*> arguments, const char *message)
{
	Check(Parse(arguments).action == CliAction::Error, message);
}

} // namespace

int main()
{
	{
		CliOptions options = Parse({"xm8"});
		Check(options.action == CliAction::Run, "no arguments action");
		Check(options.disks.empty(), "no arguments disks");
	}
	{
		CliOptions options = Parse({"xm8", "one.d88", "two.d88#1"});
		Check(options.disks.size() == 2, "two disks parsed");
		Check(options.disks[0].drive == 0 && options.disks[0].bank == 0,
			"drive 0 defaults");
		Check(options.disks[1].drive == 1 && options.disks[1].bank == 1,
			"drive 1 bank");
	}
	CheckError({"xm8", "one", "two", "three"}, "reject three disks");
	{
		CliOptions hash = Parse({"xm8", "game.d88#12"});
		CliOptions colon = Parse({"xm8", "game.d88:1"});
		CliOptions zero_hash = Parse({"xm8", "game.d88#0"});
		CliOptions zero_colon = Parse({"xm8", "game.d88:0"});
		Check(hash.disks[0].path == "game.d88" && hash.disks[0].bank == 12,
			"hash bank");
		Check(colon.disks[0].path == "game.d88" && colon.disks[0].bank == 1,
			"colon bank");
		Check(zero_hash.disks[0].bank == 0, "zero hash bank");
		Check(zero_colon.disks[0].bank == 0, "zero colon bank");
	}
	{
		CliOptions drive = Parse({"xm8", "C:\\disk.d88"});
		CliOptions bank = Parse({"xm8", "C:\\disk.d88:1"});
		Check(drive.disks[0].path == "C:\\disk.d88" &&
			drive.disks[0].bank == 0, "windows drive");
		Check(bank.disks[0].path == "C:\\disk.d88" &&
			bank.disks[0].bank == 1, "windows drive bank");
	}
	CheckError({"xm8", ""}, "reject empty path");
	CheckError({"xm8", "game.d88#"}, "reject empty bank");
	CheckError({"xm8", "game.d88#-1"}, "reject negative bank");
	CheckError({"xm8", "game.d88#x"}, "reject non-numeric bank");
	CheckError({"xm8", "game.d88#2147483648"}, "reject bank overflow");
	Check(Parse({"xm8", "game.d88#2147483647"}).disks[0].bank == INT_MAX,
		"accept INT_MAX bank");
	{
		CliOptions options = Parse({"xm8", "/tmp/a#b:c/日本 語.d88"});
		Check(options.disks[0].path == "/tmp/a#b:c/日本 語.d88",
			"separators in parent path");
	}
	CheckError({"xm8", "game#name.d88"}, "reject hash in basename");
	CheckError({"xm8", "game:name.d88"}, "reject colon in basename");
	{
		CliOptions options = Parse({"xm8", "--", "-game.d88"});
		Check(options.disks.size() == 1 &&
			options.disks[0].path == "-game.d88", "double dash");
	}
	for (const char *value : {"V1S", "v1h", "V2", "n"}) {
		Check(Parse({"xm8", "--system", value}).action == CliAction::Run,
			"valid system");
	}
	for (const char *value : {"4", "4mhz", "8", "8MHz", "8h", "8mhzh"}) {
		Check(Parse({"xm8", "--clock", value}).action == CliAction::Run,
			"valid clock");
	}
	CheckError({"xm8", "--foo"}, "unknown option");
	CheckError({"xm8", "--system", "V3"}, "invalid system");
	CheckError({"xm8", "--clock", "16MHz"}, "invalid clock");
	CheckError({"xm8", "--system"}, "missing system");
	CheckError({"xm8", "--clock"}, "missing clock");
	CheckError({"xm8", "--system", "V2", "--system", "N"},
		"duplicate system");
	CheckError({"xm8", "--clock", "4", "--clock", "8"}, "duplicate clock");
	Check(Parse({"xm8", "--help"}).action == CliAction::ShowHelp, "help");
	Check(Parse({"xm8", "-h"}).action == CliAction::ShowHelp, "short help");
	Check(Parse({"xm8", "--version"}).action == CliAction::ShowVersion,
		"version");
	CheckError({"xm8", "--help", "game.d88"}, "help must be alone");
	CheckError({"xm8", "--version", "--help"}, "actions conflict");
	Check(Parse({"xm8", "--system", "v1s"}).system == CliSystemMode::V1S,
		"system enum");
	Check(Parse({"xm8", "--clock", "8h"}).clock == CliClockMode::Clock8MHzH,
		"clock enum");

	if (failures != 0) {
		std::cerr << failures << " test(s) failed\n";
		return EXIT_FAILURE;
	}
	std::cout << "clidisk tests passed\n";
	return EXIT_SUCCESS;
}
