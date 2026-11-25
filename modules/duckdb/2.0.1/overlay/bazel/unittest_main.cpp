// Entry point of DuckDB's test runner under `bazel test`.
//
// Upstream's runner (test/unittest.cpp, whose main() is renamed to duckdb_unittest_main
// by the build) expects to be told where the source tree is. This wrapper works that out
// from the environment Bazel gives a test, and turns Bazel's test sharding into the
// runner's own "run this slice of the tests" options.

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

int duckdb_unittest_main(int argc, char *argv[]);

namespace {

std::string Env(const char *name) {
	const char *value = std::getenv(name);
	return value ? value : "";
}

} // namespace

int main(int argc, char *argv[]) {
	std::vector<std::string> args(argv, argv + argc);

	// The tests read test/ and data/ from DuckDB's source tree, which is in the runfiles.
	auto runfiles = Env("TEST_SRCDIR");
	if (!runfiles.empty()) {
		args.push_back("--test-dir");
		auto test_dir = runfiles + "/" + DUCKDB_RUNFILES_DIRECTORY;
		args.push_back(test_dir);

		// The runner keeps its scratch files in a directory per process under this one,
		// and creates this one if it is missing. Without a sandbox the shards share it,
		// and two that start together both try to create it; the loser aborts. Create
		// it here, where failing because it already exists does not matter.
		auto scratch_dir = test_dir + "/duckdb_unittest_tempdir";
#ifdef _WIN32
		_mkdir(scratch_dir.c_str());
#else
		mkdir(scratch_dir.c_str(), 0777);
#endif
	}

#ifdef _WIN32
	// Bazel gives a test a home directory (HOME) but not the variable Windows uses for
	// it, and a few tests need DuckDB to find one.
	auto tmpdir = Env("TEST_TMPDIR");
	if (Env("USERPROFILE").empty() && !tmpdir.empty()) {
		_putenv_s("USERPROFILE", tmpdir.c_str());
	}
#endif

	// The runner's scratch directory is left at its default, a directory under the source
	// tree. Pointing it at TEST_TMPDIR with --test-temp-dir would also turn off the
	// clean-up between tests, and tests that reuse a database file name then collide.

	// Bazel sharding: run the slice of the test list that belongs to this shard.
	auto total_shards = Env("TEST_TOTAL_SHARDS");
	auto shard_index = Env("TEST_SHARD_INDEX");
	if (!total_shards.empty() && !shard_index.empty()) {
		// Tells Bazel that this runner understands sharding.
		auto status_file = Env("TEST_SHARD_STATUS_FILE");
		if (!status_file.empty()) {
			if (auto file = std::fopen(status_file.c_str(), "w")) {
				std::fclose(file);
			}
		}
		int total = std::atoi(total_shards.c_str());
		int index = std::atoi(shard_index.c_str());
		if (total > 1 && total <= 100) {
			args.push_back("--start-offset-percentage");
			args.push_back(std::to_string(100 * index / total));
			args.push_back("--end-offset-percentage");
			args.push_back(std::to_string(100 * (index + 1) / total));
		}
	}

	// Tests that cannot pass under Bazel are skipped by name. Only when no test was asked
	// for on the command line: a filter given there replaces this one.
	//
	// The filter starts with ~[.] because the runner's hidden tests, the slow ones among
	// them, are only left out by default when there is no filter at all.
	std::vector<std::string> skipped;
	// These two install the demo extension from test/extension/, a loadable module that
	// this build does not produce.
	skipped.push_back("Core extension downloads use managed HTTP transports");
	skipped.push_back("UPDATE EXTENSIONS does not send an etag to a moved repository");
#ifdef __APPLE__
	// Opens a file of the test data for writing. Bazel's macOS sandbox lets a test write
	// only in its own directories, and the test data is linked from outside them.
	skipped.push_back("Test opening an invalid database file");
#endif
	bool has_filter = false;
	for (int i = 1; i < argc; i++) {
		if (argv[i][0] != '-') {
			has_filter = true;
		}
	}
	if (!skipped.empty() && !has_filter) {
		std::string filter = "~[.]";
		for (auto &name : skipped) {
			filter += "~\"" + name + "\"";
		}
		args.push_back(filter);
	}

	std::vector<char *> new_argv;
	for (auto &arg : args) {
		new_argv.push_back(&arg[0]);
	}
	new_argv.push_back(nullptr);
	return duckdb_unittest_main(static_cast<int>(args.size()), new_argv.data());
}
