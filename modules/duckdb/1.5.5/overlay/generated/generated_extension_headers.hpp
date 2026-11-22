#pragma once

// Headers of the extensions that are linked into DuckDB. Upstream's CMake build generates
// this file at configure time; this is the equivalent for the Bazel build.
#include "core_functions_extension.hpp"
#include "parquet_extension.hpp"
