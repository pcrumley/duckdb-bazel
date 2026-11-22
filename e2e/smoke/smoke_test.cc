// Minimal consumer of @duckdb: open an in-memory database and run a query.

#include <cstdint>
#include <iostream>

#include "duckdb.hpp"

int main() {
  duckdb::DuckDB db(nullptr);
  duckdb::Connection con(db);

  // Needs only the core library.
  auto result = con.Query("SELECT 42");
  if (result->HasError()) {
    std::cerr << "query failed: " << result->GetError() << "\n";
    return 1;
  }
  auto value = result->GetValue(0, 0).GetValue<int32_t>();
  if (value != 42) {
    std::cerr << "expected 42, got " << value << "\n";
    return 1;
  }

  // sum and round live in the core_functions extension.
  result = con.Query("SELECT round(sum(x), 2) FROM (VALUES (1.234), (2.345)) t(x)");
  if (result->HasError()) {
    std::cerr << "query failed: " << result->GetError() << "\n";
    return 1;
  }
  auto total = result->GetValue(0, 0).GetValue<double>();
  if (total < 3.575 || total > 3.585) {
    std::cerr << "expected 3.58, got " << total << "\n";
    return 1;
  }

  std::cout << "duckdb " << duckdb::DuckDB::LibraryVersion() << " ok\n";
  return 0;
}
