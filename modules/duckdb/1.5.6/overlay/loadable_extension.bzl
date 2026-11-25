"""Builds one of DuckDB's test extensions as a module that DuckDB loads at run time."""

load("@bazel_skylib//rules:write_file.bzl", "write_file")
load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@rules_cc//cc:cc_shared_library.bzl", "cc_shared_library")

def loadable_test_extension(name, srcs, entry_point, abi_type, duckdb_version, out = None, **kwargs):
    """A loadable extension for the test suite: test/extension/<name>.duckdb_extension.

    Built the way upstream builds them by default: everything the extension uses,
    DuckDB included, is linked into it, and only its entry point is exported.

    Args:
      name: the extension's name.
      srcs: its sources.
      entry_point: the function DuckDB calls after loading it.
      abi_type: CPP or C_STRUCT_UNSTABLE, recorded in the file's metadata.
      duckdb_version: the DuckDB version the extension is for, recorded too.
      out: the file to produce, if not test/extension/<name>.duckdb_extension.
      **kwargs: passed to the cc_library that compiles the sources.
    """
    cc_library(
        name = name + "_loadable_objects",
        testonly = True,
        srcs = srcs,
        # Nothing refers to the entry point.
        alwayslink = True,
        **kwargs
    )

    # For the GNU linkers: export the entry point and nothing else.
    write_file(
        name = name + "_loadable_exports",
        testonly = True,
        out = name + "_loadable_exports.map",
        content = ["{ global: %s; local: *; };" % entry_point, ""],
    )

    cc_shared_library(
        name = name + "_loadable_library",
        testonly = True,
        additional_linker_inputs = select({
            "@platforms//os:macos": [],
            "@platforms//os:windows": [],
            "//conditions:default": [name + "_loadable_exports"],
        }),
        user_link_flags = select({
            "@platforms//os:macos": ["-Wl,-exported_symbol,_" + entry_point],
            "@platforms//os:windows": ["/EXPORT:" + entry_point],
            "//conditions:default": ["-Wl,--version-script=$(location %s_loadable_exports)" % name],
        }),
        deps = [name + "_loadable_objects"],
    )

    # The library alone, without the import library that comes with it on Windows.
    native.filegroup(
        name = name + "_loadable_library_file",
        testonly = True,
        srcs = [name + "_loadable_library"],
        output_group = "main_shared_library_output",
    )

    native.genrule(
        name = name + "_loadable_extension",
        testonly = True,
        srcs = [name + "_loadable_library_file"],
        outs = [out or "test/extension/%s.duckdb_extension" % name],
        cmd = "$(execpath //:append_extension_metadata) $< $@ %s %s default-version" % (abi_type, duckdb_version),
        cmd_bat = "$(execpath //:append_extension_metadata) $< $@ %s %s default-version" % (abi_type, duckdb_version),
        tools = ["//:append_extension_metadata"],
    )
