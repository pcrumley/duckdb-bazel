# duckdb-bazel

A native Bazel (bzlmod) build of [DuckDB](https://duckdb.org), packaged as a small Bazel
registry. DuckDB is not in the Bazel Central Registry yet
([bazel-central-registry#4890](https://github.com/bazelbuild/bazel-central-registry/issues/4890));
the aim is to submit this there once it is complete.

**Status: work in progress.** The core library and the extensions listed below build and
pass a smoke test in CI on Linux x86_64, Linux arm64, macOS arm64 and Windows x86_64
(MSVC). DuckDB's own test suite passes in CI on Linux x86_64.

## Use

In `.bazelrc`, add this registry ahead of the BCR:

```
common --registry=https://raw.githubusercontent.com/pcrumley/duckdb-bazel/main
common --registry=https://bcr.bazel.build
```

In `MODULE.bazel`:

```starlark
bazel_dep(name = "duckdb", version = "1.5.5")
```

Then depend on `@duckdb` from a `cc_library`, `cc_binary` or `cc_test` and
`#include "duckdb.hpp"` (C++ API) or `"duckdb.h"` (C API).

Requires Bazel 7.2.1 or newer.

On Windows the build passes MSVC-style compiler flags, so it needs MSVC or clang-cl, and
it defines `DUCKDB_STATIC_BUILD` for your targets: DuckDB is always linked statically.

DuckDB takes UTF-8 strings. MSVC encodes string literals in the system code page by
default, so if your code passes non-ASCII literals to DuckDB, compile it with `/utf-8`,
as upstream's own build does, for example with `copts = ["/utf-8"]` on those targets.

### Extensions

DuckDB's in-tree extensions are linked into the library according to flags. The defaults
match upstream's build.

| Extension | Flag | Default |
|---|---|---|
| `core_functions` | always linked | on |
| `parquet` | `--@duckdb//:parquet` | on |
| `json` | `--@duckdb//:json` | off |
| `autocomplete` | `--@duckdb//:autocomplete` | off |
| `icu` | `--@duckdb//:icu` | off |
| `tpch` | `--@duckdb//:tpch` | off |
| `tpcds` | `--@duckdb//:tpcds` | off |

For example, in `.bazelrc`:

```
build --@duckdb//:json=true
build --@duckdb//:parquet=false
```

### jemalloc

On Linux, DuckDB uses its vendored jemalloc for its own allocations, as upstream's builds
do. It is prefixed (`duckdb_je_*`) and does not replace `malloc` for the rest of your
process. Upstream does not support it on musl; turn it off there, or anywhere you prefer
the system allocator:

```
build --@duckdb//:jemalloc=false
```

On other operating systems the flag has no effect and the system allocator is used.

### Command-line shell

The `duckdb` shell is a target too, with the extensions your flags select:

```
bazel run -c opt @duckdb//tools/shell:duckdb
```

Nothing builds it unless you ask for it.

### Shared library

By default DuckDB is linked statically into your binaries. `@duckdb//:libduckdb` builds
it as one shared library instead, `libduckdb.so` or `libduckdb.dylib`, holding the core
and the extensions your flags select. To link a target against it, keep the dependency
on `@duckdb` and add:

```starlark
dynamic_deps = ["@duckdb//:libduckdb"],
```

It is not available on Windows.

### Optimisation

Build with `-c opt` for anything but development: that is the equivalent of upstream's
release build, and several times faster at run time than Bazel's default mode. As
upstream does, the build then compiles DuckDB with `-O3` on GCC and Clang, in place of
the `-O2` Bazel's toolchains use. Your own targets are not affected.

### Tests

DuckDB's own test suite is a target too: its C++ API tests and SQL logic tests, about
4,500 test cases, run as 16 shards.

```
bazel test -c opt @duckdb//:unittest
```

It tests the library as your flags configure it, and skips tests that need an extension
that is not linked. Use `-c opt`: an unoptimised DuckDB runs its debug checks and takes
several times longer. Three kinds of test are left out: those that load extensions built
as separate modules, which this build does not produce; upstream's slow tests, which its
runner skips by default too; and one that cannot pass when the test data is reached
through symbolic links, as it is under Bazel.

### Build time

A first build of DuckDB with every extension takes about five minutes on 16 cores. Like
upstream, it compiles most source directories as one unit each.
Bazel caches the result, but CI and fresh checkouts pay it again unless you use
`--disk_cache` or a remote cache.

## Layout

```
bazel_registry.json          marks this repository as a registry
modules/duckdb/
  metadata.json
  1.5.5/
    source.json              upstream tarball, plus a hash for each overlay file
    MODULE.bazel
    presubmit.yml            BCR CI configuration
    overlay/                 files added on top of the upstream source
      MODULE.bazel           copy of ../MODULE.bazel
      BUILD.bazel            the build itself
      unity.bzl              macro that compiles a source directory as one unit
      bazel/                 entry point that runs upstream's test runner under Bazel
      tools/shell/           package of the command-line shell
      generated/             stand-ins for files upstream's CMake generates
e2e/smoke/                   consumer workspace that resolves duckdb from this checkout
tools/update_integrity.py    syncs overlay/MODULE.bazel and the hashes in source.json
```

## Develop

After any change under `modules/`, refresh the overlay hashes:

```
tools/update_integrity.py          # --check to verify only
```

Then build and test from the consumer workspace. Bazel caches registry files for the
lifetime of its server, so restart it first or it keeps using the previous overlay:

```
cd e2e/smoke
bazel shutdown
bazel test //...
```

The workspace's `.bazelrc` sets a disk cache in `~/.cache/duckdb-bazel-disk-cache`, so
only the first build pays the full compile.

## Licence

MIT, see [LICENSE](LICENSE). DuckDB itself is also MIT licensed.
