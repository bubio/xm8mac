#include "Fixtures/d88_fixture.h"

#include <cstdlib>
#include <iostream>
#include <string>

int main(int argc, char *argv[])
{
	if (argc != 2) {
		std::cerr << "usage: generate_d88_fixtures <output-directory>\n";
		return EXIT_FAILURE;
	}

	std::string error;
	if (!D88Fixture::GenerateStandardSet(argv[1], &error)) {
		std::cerr << error << '\n';
		return EXIT_FAILURE;
	}
	std::cout << "generated D88 fixtures in " << argv[1] << '\n';
	return EXIT_SUCCESS;
}
