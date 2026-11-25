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

// The first value of the first row. Written with the row iterator, which is the same in
// DuckDB 1.x and 2.x: what Connection::Query returns changed between them.
duckdb::Value FirstValue(duckdb::QueryResult &result) {
  for (auto &row : result) {
    return row.GetBaseValue(0);
  }
  return duckdb::Value();
}

// Runs a query that returns a single value and compares it, as text, with `expected`.
void Check(duckdb::Connection &con, const std::string &sql, const std::string &expected) {
  auto result = con.Query(sql);
  if (result->HasError()) {
    std::cerr << "FAILED: " << sql << "\n  " << result->GetError() << "\n";
    failures++;
    return;
  }
  auto actual = FirstValue(*result).ToString();
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
  bool loaded = !result->HasError() && FirstValue(*result).GetValue<int64_t>() == 1;
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

  if (Expect(con, expected, "icu")) {
    extensions += " icu";
    // Time zone data: Amsterdam is two hours ahead of UTC in summer.
    Check(con, "SELECT timezone('Europe/Amsterdam', TIMESTAMPTZ '2024-07-01 12:00:00+00')::VARCHAR",
          "2024-07-01 14:00:00");
    // Collation data: German sorts a-umlaut with a, ahead of z. DuckDB takes UTF-8, so
    // with MSVC this literal relies on /utf-8, which BUILD.bazel sets as upstream's build does.
    Check(con, "SELECT string_agg(s, ',' ORDER BY s COLLATE de) FROM (VALUES ('z'), ('\u00e4'), ('a')) t(s)",
          "a,\u00e4,z");
  }

  if (Expect(con, expected, "json")) {
    extensions += " json";
    Check(con, "SELECT json_extract('{\"a\": {\"b\": 41}}', '$.a.b')::INTEGER + 1", "42");
  }

  if (Expect(con, expected, "tpcds")) {
    extensions += " tpcds";
    Check(con, "SELECT count(*) FROM tpcds_queries()", "99");
    // In its own schema: tpch below generates a table named customer too.
    auto generated = con.Query("CREATE SCHEMA ds; CALL dsdgen(sf = 0.01, schema = 'ds')");
    if (generated->HasError()) {
      std::cerr << "FAILED: CALL dsdgen(sf = 0.01)\n  " << generated->GetError() << "\n";
      failures++;
    }
    Check(con, "SELECT count(*) FROM ds.date_dim", "73049");
    Check(con, "SELECT count(*) > 0 FROM ds.store_sales", "true");
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
