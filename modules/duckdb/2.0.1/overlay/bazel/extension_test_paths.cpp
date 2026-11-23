// Upstream's CMake build generates this file from test/helpers/extension_test_paths.cpp.in
// with the test directories of extensions built from outside the DuckDB tree. This build
// has none: the tests of the in-tree extensions are under test/.
#include "test_helpers.hpp"

namespace duckdb {

vector<string> LoadedExtensionTestPaths() {
	return {};
}

} // namespace duckdb
