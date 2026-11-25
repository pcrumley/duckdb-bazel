# duckdb-bazel

A native Bazel (bzlmod) build of [DuckDB](https://duckdb.org), packaged as a small Bazel
registry. It is not an official DuckDB project. DuckDB is not in the Bazel Central
Registry yet
([bazel-central-registry#4890](https://github.com/bazelbuild/bazel-central-registry/issues/4890));
this is being prepared for submission there.

The core library, the shell, the shared library and the extensions listed below build
and pass a smoke test in CI on Linux x86_64, Linux arm64, macOS arm64 and Windows x86_64
(MSVC). DuckDB's own test suite passes in CI on the same four platforms (on Windows with
clang-cl).

## Use

In `.bazelrc`, add this registry ahead of the BCR:

```
common --registry=https://raw.githubusercontent.com/pcrumley/duckdb-bazel/main
common --registry=https://bcr.bazel.build
```

In `MODULE.bazel`:

```starlark
bazel_dep(name = "duckdb", version = "2.0.1")
```

Then depend on `@duckdb` from a `cc_library`, `cc_binary` or `cc_test` and
`#include "duckdb.hpp"` (C++ API) or `"duckdb.h"` (C API).

Requires Bazel 7.2.1 or newer.

On Windows the build passes MSVC-style compiler flags, so it needs MSVC or clang-cl, and
`@duckdb` defines `DUCKDB_STATIC_BUILD` for your targets, as static linking needs.

Upstream links its Windows binaries against the static C runtime (`/MT`). Bazel's
Windows toolchain uses the dynamic one (`/MD`) unless told otherwise, and DuckDB here
follows the toolchain, since a library has to use the same runtime as the code it is
linked into. For a `duckdb.exe` or `duckdb.dll` that does not need the Visual C++
runtime installed, build with `--features=static_link_msvcrt`.

DuckDB takes UTF-8 strings. MSVC encodes string literals in the system code page by
default, so if your code passes non-ASCII literals to DuckDB, compile it with `/utf-8`,
as upstream's own build does, for example with `copts = ["/utf-8"]` on those targets.

### Changes in DuckDB 2.0

- Linked extensions, `core_functions` included, are registered by a static initialiser in
  the library. A database opened before that runs, from a static initialiser of your
  own, has no extensions. Open databases from `main` onwards.
- Outside `-c opt`, DuckDB is built with its internal assertions on
  (`DUCKDB_FORCE_ASSERT`) and is much slower. 2.0 does not compile in Bazel's default
  mode without them. The build chooses by compilation mode: `-c opt` gets `NDEBUG`.

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

By default DuckDB is linked statically into your binaries. To link it dynamically,
depend on `@duckdb//:duckdb_shared` instead of `@duckdb`. A binary must not get both.

The library itself is `@duckdb//:libduckdb`: `libduckdb.so`, `libduckdb.dylib` or
`duckdb.dll`, holding the core and the extensions your flags select.

### Optimisation

Build with `-c opt` for anything but development: that is the equivalent of upstream's
release build, and several times faster at run time than Bazel's default mode. As
upstream does, the build then compiles DuckDB with `-O3` on GCC and Clang, in place of
the `-O2` Bazel's toolchains use. Your own targets are not affected.

### Tests

DuckDB's own test suite is a target too: its C++ API tests and SQL logic tests, about
8,500 test cases in DuckDB 2.0, run as 16 shards.

```
bazel test -c opt @duckdb//:unittest
```

It tests the library as your flags configure it, and skips tests that need an extension
that is not linked. Use `-c opt`: an unoptimised DuckDB runs its debug checks and takes
several times longer. Upstream's slow tests are left out, as its runner leaves them out
by default too.

Some tests load an extension at run time. For those the build produces the small test
extensions under `test/extension/` as loadable modules, each with its own copy of DuckDB
linked in, as upstream builds them by default. They add about 300 MB to the build.

Bazel gives a test its data as symbolic links, which a few of DuckDB's tests cannot work
through. The test's entry point replaces the files those tests use, about 5 MB, with real
copies before it starts.

On Windows, add `--enable_runfiles`: the suite reads its test files from a runfiles
directory, which Bazel only creates there when asked, and which needs permission to
create symbolic links (administrator rights or Developer Mode).

Upstream builds and tests DuckDB 2.0 on Windows with clang-cl, not with MSVC's own
compiler, and so does CI here for the test suite. With MSVC the library builds and passes
the smoke test, but a few of DuckDB's tests fail. `e2e/smoke/.bazelrc` shows how to
select clang-cl (`--config=clang-cl`).

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
  2.0.1/
    source.json              upstream tarball, plus a hash for each overlay file
    MODULE.bazel
    presubmit.yml            BCR CI configuration
    overlay/                 files added on top of the upstream source
      MODULE.bazel           copy of ../MODULE.bazel
      BUILD.bazel            the build itself
      unity.bzl              macro that compiles a source directory as one unit
      bazel/                 entry point that runs upstream's test runner under Bazel
      bazel/test_module/     module the BCR presubmit tests DuckDB with
      tools/shell/           package of the command-line shell
      generated/             stand-ins for files upstream's CMake generates
e2e/smoke/                   consumer workspace that resolves duckdb from this checkout
tools/update_integrity.py    syncs overlay/MODULE.bazel and the hashes in source.json
tools/bump_version.py        adds a DuckDB version, starting from an existing one
tools/presubmit_tree.py      lays out a version's source tree as a registry does
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

To see what the BCR presubmit will see, lay out a version's tree and run its test module
with no flags:

```
tools/presubmit_tree.py 1.5.6 /tmp/duckdb-1.5.6
cd /tmp/duckdb-1.5.6/bazel/test_module
bazel test //...
```

The "BCR presubmit" workflow does this on Linux, macOS and Windows with Bazel 7, 8 and 9.
Start it by hand from the Actions tab; each job is a clean build of about an hour.

The workspace's `.bazelrc` sets a disk cache in `~/.cache/duckdb-bazel-disk-cache`, so
only the first build pays the full compile.

## Licence

MIT, see [LICENSE](LICENSE). DuckDB itself is also MIT licensed.
