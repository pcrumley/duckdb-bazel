# duckdb-bazel

A native Bazel (bzlmod) build of [DuckDB](https://duckdb.org), packaged as a small Bazel
registry. DuckDB is not in the Bazel Central Registry yet
([bazel-central-registry#4890](https://github.com/bazelbuild/bazel-central-registry/issues/4890));
the aim is to submit this there once it is complete.

**Status: work in progress.** The core library and the extensions listed below build and
run queries on Linux x86_64. Other platforms are untested.

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

### Extensions

DuckDB's in-tree extensions are linked into the library according to flags. The defaults
match upstream's build.

| Extension | Flag | Default |
|---|---|---|
| `core_functions` | always linked | on |
| `parquet` | `--@duckdb//:parquet` | on |
| `json` | `--@duckdb//:json` | off |
| `autocomplete` | `--@duckdb//:autocomplete` | off |

For example, in `.bazelrc`:

```
build --@duckdb//:json=true
build --@duckdb//:parquet=false
```

The other in-tree extensions (`icu`, `tpch`, `tpcds`) are not available yet.

### jemalloc

On Linux, DuckDB uses its vendored jemalloc for its own allocations, as upstream's builds
do. It is prefixed (`duckdb_je_*`) and does not replace `malloc` for the rest of your
process. Upstream does not support it on musl; turn it off there, or anywhere you prefer
the system allocator:

```
build --@duckdb//:jemalloc=false
```

On other operating systems the flag has no effect and the system allocator is used.

### Build time

DuckDB is about 1,750 source files, and a first build takes over ten minutes on 16 cores.
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
bazel test //:smoke_test
```

The workspace's `.bazelrc` sets a disk cache in `~/.cache/duckdb-bazel-disk-cache`, so
only the first build pays the full compile.

## Licence

MIT, see [LICENSE](LICENSE). DuckDB itself is also MIT licensed.
