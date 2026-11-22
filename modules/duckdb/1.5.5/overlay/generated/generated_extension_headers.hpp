#pragma once

// Headers of the extensions that are linked into DuckDB. Upstream's CMake build generates
// this file at configure time; the Bazel build instead sets DUCKDB_EXTENSION_<NAME>_LINKED
// for each extension whose flag is on.
#include "core_functions_extension.hpp"

#ifdef DUCKDB_EXTENSION_AUTOCOMPLETE_LINKED
#include "autocomplete_extension.hpp"
#endif

#ifdef DUCKDB_EXTENSION_JSON_LINKED
#include "json_extension.hpp"
#endif

#ifdef DUCKDB_EXTENSION_PARQUET_LINKED
#include "parquet_extension.hpp"
#endif

#ifdef DUCKDB_EXTENSION_TPCDS_LINKED
#include "tpcds_extension.hpp"
#endif

#ifdef DUCKDB_EXTENSION_TPCH_LINKED
#include "tpch_extension.hpp"
#endif
