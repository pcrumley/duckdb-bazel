// Entry point of DuckDB's test runner under `bazel test`.
//
// Upstream's runner (test/unittest.cpp, whose main() is renamed to duckdb_unittest_main
// by the build) expects to be told where the source tree is. This wrapper works that out
// from the environment Bazel gives a test, and turns Bazel's test sharding into the
// runner's own "run this slice of the tests" options.

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <random>
#include <string>
#include <system_error>
#include <vector>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

int duckdb_unittest_main(int argc, char *argv[]);

namespace {

// The root of the runner's scratch directories, relative to the source tree.
const char *const SCRATCH_DIRECTORY = "t";

std::string Env(const char *name) {
	const char *value = std::getenv(name);
	return value ? value : "";
}

// Bazel gives a test its data files as symbolic links to the real files. A few tests
// cannot work through links: DuckDB's ** glob does not follow them, its allowed_directories
// setting resolves them to a path outside the allowed directory, and the macOS sandbox
// refuses to open their targets for writing. These are the files and directories those
// tests use, relative to the source tree. They are small, and are replaced by real copies
// before the runner starts.
const char *const REAL_FILES[] = {
    "data/csv/glob",
    "data/json",
    "data/parquet-testing/blob.parquet",
    "data/parquet-testing/glob",
    "data/parquet-testing/h2oai/h2oai_group_small.parquet",
};

// Replaces `path` by a copy of the file it links to, if it is a link to a regular file.
// Errors are ignored: the tests that needed the copy will report them.
void ReplaceLinkByCopy(const std::filesystem::path &path, const std::string &suffix) {
	namespace fs = std::filesystem;
	std::error_code error;
	if (!fs::is_symlink(path, error) || !fs::is_regular_file(path, error)) {
		return;
	}
	// Copied next to the link and renamed over it, so that another shard using the same
	// directory never sees a partial file.
	auto copy = path;
	copy += suffix;
	if (fs::copy_file(path, copy, error)) {
		fs::rename(copy, path, error);
	}
	if (error) {
		fs::remove(copy, error);
	}
}

void ReplaceLinksByCopies(const std::filesystem::path &root) {
	namespace fs = std::filesystem;
	auto suffix = ".copy" + std::to_string(std::random_device()());
	for (auto relative : REAL_FILES) {
		auto path = root / relative;
		std::error_code error;
		if (!fs::is_directory(path, error)) {
			ReplaceLinkByCopy(path, suffix);
			continue;
		}
		// Collected first: replacing entries while iterating over them is not safe.
		std::vector<fs::path> entries;
		for (fs::recursive_directory_iterator it(path, error), end; !error && it != end; it.increment(error)) {
			entries.push_back(it->path());
		}
		for (auto &entry : entries) {
			ReplaceLinkByCopy(entry, suffix);
		}
	}
}

} // namespace

int main(int argc, char *argv[]) {
	std::vector<std::string> args(argv, argv + argc);

	// The tests read test/ and data/ from DuckDB's source tree, which is in the runfiles.
	auto runfiles = Env("TEST_SRCDIR");
	if (!runfiles.empty()) {
		auto test_dir = runfiles + "/" + DUCKDB_RUNFILES_DIRECTORY;

		ReplaceLinksByCopies(test_dir);

		// The runner keeps its scratch files under the source tree, in
		// <root>/<run id>/<test name>/. Without a sandbox the shards share the root, and
		// two that start together both try to create it; the loser aborts. Create it
		// here, where failing because it already exists does not matter.
		//
		// The root and the run id are given short names in place of the defaults
		// (duckdb_unittest_tempdir and a UUID) to keep the paths short: see below.
		auto scratch_dir = test_dir + "/" + SCRATCH_DIRECTORY;
#ifdef _WIN32
		_mkdir(scratch_dir.c_str());
#else
		mkdir(scratch_dir.c_str(), 0777);
#endif
		args.push_back("--temp-dir-root");
		args.push_back(SCRATCH_DIRECTORY);
		args.push_back("--run-id");
		args.push_back("r" + std::to_string(std::random_device()()));

#ifdef _WIN32
		// Some tests open their scratch files with functions that stop at 260 characters
		// on Windows, and under the runfiles directory, itself over 100 characters, they
		// run past that. So the runner is started through a link to the source tree with
		// a shorter path, in the directory Bazel gives each test for temporary files. If
		// the link cannot be made, the tests run from the long path as before.
		auto tmpdir = Env("TEST_TMPDIR");
		if (!tmpdir.empty()) {
			auto link = std::filesystem::path(tmpdir) / "s";
			std::error_code error;
			// One may be left from an earlier attempt at this shard.
			std::filesystem::remove(link, error);
			std::filesystem::create_directory_symlink(std::filesystem::absolute(test_dir), link, error);
			if (!error) {
				test_dir = link.string();
			}
		}
#endif
		args.push_back("--test-dir");
		args.push_back(test_dir);
	}

#ifdef _WIN32
	// Bazel gives a test a home directory (HOME) but not the variable Windows uses for
	// it, and a few tests need DuckDB to find one.
	if (Env("USERPROFILE").empty() && !Env("TEST_TMPDIR").empty()) {
		_putenv_s("USERPROFILE", Env("TEST_TMPDIR").c_str());
	}
#endif

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

	std::vector<char *> new_argv;
	for (auto &arg : args) {
		new_argv.push_back(&arg[0]);
	}
	new_argv.push_back(nullptr);
	return duckdb_unittest_main(static_cast<int>(args.size()), new_argv.data());
}
