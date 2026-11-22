// Upstream's CMake build generates this file at configure time from the list of extensions
// linked into the library. The Bazel build instead sets DUCKDB_EXTENSION_<NAME>_LINKED for
// each extension whose flag is on.

#include "duckdb/main/extension/generated_extension_loader.hpp"
#include "duckdb/main/extension_helper.hpp"

namespace duckdb {

//! Tries to load <extension> from the extensions that are linked into this library.
ExtensionLoadResult ExtensionHelper::LoadExtension(DuckDB &db, const std::string &extension) {
	if (extension == "core_functions") {
		db.LoadStaticExtension<CoreFunctionsExtension>();
		return ExtensionLoadResult::LOADED_EXTENSION;
	}
#ifdef DUCKDB_EXTENSION_AUTOCOMPLETE_LINKED
	if (extension == "autocomplete") {
		db.LoadStaticExtension<AutocompleteExtension>();
		return ExtensionLoadResult::LOADED_EXTENSION;
	}
#endif
#ifdef DUCKDB_EXTENSION_JSON_LINKED
	if (extension == "json") {
		db.LoadStaticExtension<JsonExtension>();
		return ExtensionLoadResult::LOADED_EXTENSION;
	}
#endif
#ifdef DUCKDB_EXTENSION_PARQUET_LINKED
	if (extension == "parquet") {
		db.LoadStaticExtension<ParquetExtension>();
		return ExtensionLoadResult::LOADED_EXTENSION;
	}
#endif
#ifdef DUCKDB_EXTENSION_TPCH_LINKED
	if (extension == "tpch") {
		db.LoadStaticExtension<TpchExtension>();
		return ExtensionLoadResult::LOADED_EXTENSION;
	}
#endif
	return ExtensionLoadResult::NOT_LOADED;
}

vector<string> LinkedExtensions() {
	return {
	    "core_functions",
#ifdef DUCKDB_EXTENSION_AUTOCOMPLETE_LINKED
	    "autocomplete",
#endif
#ifdef DUCKDB_EXTENSION_JSON_LINKED
	    "json",
#endif
#ifdef DUCKDB_EXTENSION_PARQUET_LINKED
	    "parquet",
#endif
#ifdef DUCKDB_EXTENSION_TPCH_LINKED
	    "tpch",
#endif
	};
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
