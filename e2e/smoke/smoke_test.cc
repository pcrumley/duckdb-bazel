// Minimal consumer of @duckdb: open an in-memory database and exercise the core library
// and the optional extensions. The arguments name the optional extensions that must be
// loaded; any other optional extension must not be.

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <set>
#include <string>

#include "duckdb.hpp"

namespace {

int failures = 0;

// Runs a query that returns a single value and compares it, as text, with `expected`.
void Check(duckdb::Connection &con, const std::string &sql, const std::string &expected) {
  auto result = con.Query(sql);
  if (result->HasError()) {
    std::cerr << "FAILED: " << sql << "\n  " << result->GetError() << "\n";
    failures++;
    return;
  }
  auto actual = result->GetValue(0, 0).ToString();
  if (actual != expected) {
    std::cerr << "FAILED: " << sql << "\n  expected " << expected << ", got " << actual << "\n";
    failures++;
  }
}

// Checks that `extension` is loaded exactly when it is expected to be, and returns
// whether it is expected.
bool Expect(duckdb::Connection &con, const std::set<std::string> &expected, const std::string &extension) {
  auto result = con.Query("SELECT count(*) FROM duckdb_extensions() WHERE loaded AND extension_name = '" +
                          extension + "'");
  bool loaded = !result->HasError() && result->GetValue(0, 0).GetValue<int64_t>() == 1;
  bool want = expected.count(extension) > 0;
  if (loaded != want) {
    std::cerr << "FAILED: extension " << extension << " is " << (loaded ? "loaded" : "not loaded")
              << " but was " << (want ? "expected" : "not expected") << "\n";
    failures++;
  }
  return want;
}

}  // namespace

int main(int argc, char **argv) {
  std::set<std::string> expected(argv + 1, argv + argc);

  duckdb::DuckDB db(nullptr);
  duckdb::Connection con(db);

  // Needs only the core library.
  Check(con, "SELECT 42", "42");

  // sum and round live in the core_functions extension, which is always linked.
  Check(con, "SELECT round(sum(x), 2) FROM (VALUES (1.234), (2.345)) t(x)", "3.58");

  std::string extensions = "core_functions";

  if (Expect(con, expected, "parquet")) {
    extensions += " parquet";
    const char *tmpdir = std::getenv("TEST_TMPDIR");
    std::string path = std::string(tmpdir ? tmpdir : "/tmp") + "/smoke.parquet";
    Check(con, "COPY (SELECT range AS i FROM range(1000)) TO '" + path + "' (FORMAT parquet)", "1000");
    Check(con, "SELECT sum(i) FROM read_parquet('" + path + "')", "499500");
  }

  if (Expect(con, expected, "autocomplete")) {
    extensions += " autocomplete";
    Check(con, "SELECT trim(suggestion) FROM sql_auto_complete('SEL') LIMIT 1", "SELECT");
  }

  if (Expect(con, expected, "json")) {
    extensions += " json";
    Check(con, "SELECT json_extract('{\"a\": {\"b\": 41}}', '$.a.b')::INTEGER + 1", "42");
  }

  if (Expect(con, expected, "tpch")) {
    extensions += " tpch";
    Check(con, "SELECT count(*) FROM tpch_queries()", "22");
    auto generated = con.Query("CALL dbgen(sf = 0.01)");
    if (generated->HasError()) {
      std::cerr << "FAILED: CALL dbgen(sf = 0.01)\n  " << generated->GetError() << "\n";
      failures++;
    }
    Check(con, "SELECT count(*) FROM lineitem", "60175");
  }

  if (failures > 0) {
    return 1;
  }
  std::cout << "duckdb " << duckdb::DuckDB::LibraryVersion() << " ok, extensions: " << extensions << "\n";
  return 0;
}
