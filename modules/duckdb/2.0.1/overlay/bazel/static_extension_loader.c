// Registers the extensions that are linked into this build with DuckDB.
//
// Upstream's CMake build generates this file at configure time from
// extension/loader/static_extension_loader.c.in and the list of linked extensions. The
// Bazel build instead sets DUCKDB_EXTENSION_<NAME>_LINKED for each extension whose flag
// is on. extension/loader/static_extension_autoregister.cpp, compiled next to this file,
// calls duckdb_register_static_extensions before main.
//
// httplib and loadable_extensions are parts of the core library that upstream registers
// the same way. Its third, local_extension_repository, is left out: it points automatic
// installs at the build directory of a CMake build.

#include "duckdb_static_extension.h"

// As in upstream's template: the loader belongs to the program or library it is compiled
// into and is not exported from it.
#ifndef DUCKDB_STATIC_EXTENSION_LOADER_API
#if defined(__GNUC__) || defined(__clang__)
#define DUCKDB_STATIC_EXTENSION_LOADER_API __attribute__((visibility("hidden")))
#else
#define DUCKDB_STATIC_EXTENSION_LOADER_API
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define DUCKDB_STATIC_EXTENSIONS(X)                                                                                    \
	X(httplib)                                                                                                         \
	X(loadable_extensions)                                                                                             \
	X(core_functions)                                                                                                  \
	DUCKDB_STATIC_EXTENSION_AUTOCOMPLETE(X)                                                                            \
	DUCKDB_STATIC_EXTENSION_ICU(X)                                                                                     \
	DUCKDB_STATIC_EXTENSION_JSON(X)                                                                                    \
	DUCKDB_STATIC_EXTENSION_PARQUET(X)                                                                                 \
	DUCKDB_STATIC_EXTENSION_TPCDS(X)                                                                                   \
	DUCKDB_STATIC_EXTENSION_TPCH(X)

#ifdef DUCKDB_EXTENSION_AUTOCOMPLETE_LINKED
#define DUCKDB_STATIC_EXTENSION_AUTOCOMPLETE(X) X(autocomplete)
#else
#define DUCKDB_STATIC_EXTENSION_AUTOCOMPLETE(X)
#endif
#ifdef DUCKDB_EXTENSION_ICU_LINKED
#define DUCKDB_STATIC_EXTENSION_ICU(X) X(icu)
#else
#define DUCKDB_STATIC_EXTENSION_ICU(X)
#endif
#ifdef DUCKDB_EXTENSION_JSON_LINKED
#define DUCKDB_STATIC_EXTENSION_JSON(X) X(json)
#else
#define DUCKDB_STATIC_EXTENSION_JSON(X)
#endif
#ifdef DUCKDB_EXTENSION_PARQUET_LINKED
#define DUCKDB_STATIC_EXTENSION_PARQUET(X) X(parquet)
#else
#define DUCKDB_STATIC_EXTENSION_PARQUET(X)
#endif
#ifdef DUCKDB_EXTENSION_TPCDS_LINKED
#define DUCKDB_STATIC_EXTENSION_TPCDS(X) X(tpcds)
#else
#define DUCKDB_STATIC_EXTENSION_TPCDS(X)
#endif
#ifdef DUCKDB_EXTENSION_TPCH_LINKED
#define DUCKDB_STATIC_EXTENSION_TPCH(X) X(tpch)
#else
#define DUCKDB_STATIC_EXTENSION_TPCH(X)
#endif

#define DECLARE(name) int32_t duckdb_extension_##name##_describe(duckdb_extension_descriptor *descriptor);
DUCKDB_STATIC_EXTENSIONS(DECLARE)

DUCKDB_STATIC_EXTENSION_LOADER_API int32_t duckdb_register_static_extensions(void) {
	int32_t result = 0;
#define REGISTER(name)                                                                                                 \
	if (duckdb_register_static_extension(duckdb_extension_##name##_describe) != 0) {                                   \
		result = 1;                                                                                                    \
	}
	DUCKDB_STATIC_EXTENSIONS(REGISTER)
	return result;
}

#ifdef __cplusplus
}
#endif
