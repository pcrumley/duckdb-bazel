// Runs the DuckDB command-line shell that Bazel built and checks what it prints.
#include <cstdio>
#include <iostream>
#include <memory>
#include <string>

#include "rules_cc/cc/runfiles/runfiles.h"

#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#endif

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "usage: shell_test <runfiles path of the shell>\n";
    return 2;
  }
  std::string error;
  std::unique_ptr<rules_cc::cc::runfiles::Runfiles> runfiles(
      rules_cc::cc::runfiles::Runfiles::CreateForTest(&error));
  if (!runfiles) {
    std::cerr << "runfiles: " << error << "\n";
    return 2;
  }
  std::string command =
      "\"" + runfiles->Rlocation(argv[1]) + "\" -csv -c \"SELECT 6 * 7 AS answer, version() AS version\"";
#ifdef _WIN32
  // cmd.exe strips the first and last quote of a command that has more than two.
  command = "\"" + command + "\"";
#endif
  FILE *pipe = popen(command.c_str(), "r");
  if (!pipe) {
    std::cerr << "could not run: " << command << "\n";
    return 1;
  }
  std::string output;
  char buffer[256];
  while (fgets(buffer, sizeof(buffer), pipe)) {
    output += buffer;
  }
  int status = pclose(pipe);
  std::cout << output;
  if (status != 0) {
    std::cerr << "the shell exited with status " << status << "\n";
    return 1;
  }
  if (output.find("answer,version") == std::string::npos || output.find("42,v") == std::string::npos) {
    std::cerr << "unexpected output\n";
    return 1;
  }
  return 0;
}
