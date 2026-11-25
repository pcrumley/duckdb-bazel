// Turns a shared library into a file DuckDB accepts as a loadable extension, by
// appending the metadata that DuckDB reads from the end of the file. Does what
// upstream's scripts/append_metadata.cmake does.
//
//   append_extension_metadata <library> <output> <abi type> <duckdb version> <extension version>
//
// The platform recorded is the one this tool is compiled for, as upstream's build
// records the platform of a program it runs. So the result is only loadable when the
// build is not a cross-compilation.

#include "duckdb/common/platform.hpp"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>

namespace {

// A metadata field: 32 bytes, padded with zeros.
std::string Field(const std::string &value) {
	std::string field = value.substr(0, 32);
	field.resize(32, '\0');
	return field;
}

} // namespace

int main(int argc, char *argv[]) {
	if (argc != 6) {
		std::cerr << "usage: " << argv[0] << " <library> <output> <abi type> <duckdb version> <extension version>\n";
		return 1;
	}
	std::ifstream input(argv[1], std::ios::binary);
	std::ofstream output(argv[2], std::ios::binary);
	if (!input || !output) {
		std::cerr << "cannot open " << (input ? argv[2] : argv[1]) << "\n";
		return 1;
	}
	output << input.rdbuf();

	// The footer has the form of a WebAssembly custom section named duckdb_signature:
	// section id 0, its length (531), the length of the name (16), the name, and the
	// length of the payload (512).
	std::string footer;
	footer += '\0';
	footer += "\x93\x04\x10";
	footer += "duckdb_signature";
	footer += "\x80\x04";
	// Eight fields, last one first.
	footer += Field("") + Field("") + Field("");
	footer += Field(argv[3]);
	footer += Field(argv[5]);
	footer += Field(argv[4]);
	footer += Field(duckdb::DuckDBPlatform());
	footer += Field("4");
	// Room for a signature. These extensions are not signed.
	footer += std::string(256, '\0');
	output.write(footer.data(), static_cast<std::streamsize>(footer.size()));
	output.close();
	return output ? 0 : 1;
}
