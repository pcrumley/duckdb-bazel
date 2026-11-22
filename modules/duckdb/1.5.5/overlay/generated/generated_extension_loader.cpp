// Upstream's CMake build generates this file at configure time from the list of extensions
// linked into the library. This is the equivalent for the Bazel build.

#include "duckdb/main/extension/generated_extension_loader.hpp"
#include "duckdb/main/extension_helper.hpp"

namespace duckdb {

//! Tries to load <extension> from the extensions that are linked into this library.
ExtensionLoadResult ExtensionHelper::LoadExtension(DuckDB &db, const std::string &extension) {
	if (extension == "core_functions") {
		db.LoadStaticExtension<CoreFunctionsExtension>();
		return ExtensionLoadResult::LOADED_EXTENSION;
	}
	return ExtensionLoadResult::NOT_LOADED;
}

vector<string> LinkedExtensions() {
	return {"core_functions"};
}

void ExtensionHelper::LoadAllExtensions(DuckDB &db) {
	for (auto &ext_name : LinkedExtensions()) {
		LoadExtension(db, ext_name);
	}
}

vector<string> ExtensionHelper::LoadedExtensionTestPaths() {
	return {};
}

} // namespace duckdb
