"""Unity builds: compile each source directory as one translation unit.

Upstream's CMake build does the same (add_library_unity in its CMakeLists.txt files).
DuckDB's sources include the same large headers over and over, so compiling a directory
at a time takes a fraction of the time of compiling each file on its own.
"""

load("@bazel_skylib//rules:write_file.bzl", "write_file")
load("@rules_cc//cc:cc_library.bzl", "cc_library")

def unity_cc_library(name, srcs, non_unity_dirs = [], non_unity_srcs = [], **kwargs):
    """A cc_library whose sources are compiled one directory at a time.

    For each directory, generates a file that includes the directory's sources and
    compiles that in their place.

    Args:
      name: name of the library.
      srcs: source files, as returned by glob().
      non_unity_dirs: directories whose files are compiled one by one, for the
        directories upstream keeps out of its unity build.
      non_unity_srcs: single files that are compiled on their own.
      **kwargs: passed to cc_library.
    """
    groups = {}
    single = []
    for src in srcs:
        directory = src.rpartition("/")[0]
        if directory in non_unity_dirs or src in non_unity_srcs:
            single.append(src)
        else:
            groups.setdefault(directory, []).append(src)

    generated = []
    grouped = []
    for directory, files in groups.items():
        target = "%s_unity_%s" % (name, directory.replace("/", "_"))
        write_file(
            name = target,
            out = target + ".cpp",
            # Resolved against the repository root, which Bazel puts on the quote
            # include path.
            content = ['#include "%s"' % f for f in sorted(files)] + [""],
        )
        generated.append(target + ".cpp")
        grouped.extend(files)

    cc_library(
        name = name,
        srcs = generated + single,
        # The sources the generated files include.
        textual_hdrs = grouped,
        **kwargs
    )
