// Minimal consumer of @duckdb: open an in-memory database and run a query.

#include <cstdint>
#include <iostream>

#include "duckdb.hpp"

int main() {
  duckdb::DuckDB db(nullptr);
  duckdb::Connection con(db);

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
  std::cout << "duckdb " << duckdb::DuckDB::LibraryVersion() << " ok\n";
  return 0;
}
