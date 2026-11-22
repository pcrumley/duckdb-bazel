# duckdb-bazel

A native Bazel (bzlmod) build of [DuckDB](https://duckdb.org), packaged as a small Bazel
registry. DuckDB is not in the Bazel Central Registry yet
([bazel-central-registry#4890](https://github.com/bazelbuild/bazel-central-registry/issues/4890));
the aim is to submit this there once it is complete.

**Status: scaffold.** The module resolves and exports DuckDB's headers, but compiles no
sources yet, so you can compile against `@duckdb` but not link.

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
e2e/smoke/                   consumer workspace that resolves duckdb from this checkout
tools/update_integrity.py    syncs overlay/MODULE.bazel and the hashes in source.json
```

## Develop

After any change under `modules/`, refresh the overlay hashes:

```
tools/update_integrity.py          # --check to verify only
```

Then build and test from the consumer workspace:

```
cd e2e/smoke
bazel test //:smoke_test
```

## Licence

MIT, see [LICENSE](LICENSE). DuckDB itself is also MIT licensed.
