# flagcxx
A flag parsing library for C++17 and above, inspired by the Go flag package.

# Usage
Copy `flag.h` into your project include path. Then:

```c++
#include "flag.h"

int main(int argc, char** argv) {
  // Flag values are stored in plain old variables.
  int count;
  float amount;
  string name{"unknown"}; // Default values are given at variable initialization
  bool enable;

  // Use the FlagSet type to define the command line flags and map them to the variables
  // where their value will be stored.
  flag::FlagSet flags;
  flags.Var(count, "count", "The count of things, an integer value");
  flags.Var(amount, "amount", "The amount of something, a floating point value");
  flags.Var(name, "name", "The name of a thing, a string value");
  flags.Var(enable, "enable", "A boolean flag to enable something, or not");

  // Use the parse method to parse the command line arguments into the flag variables.
  auto error = flags.Parse(argc, argv);

  // Errors are returned via a std::optional return value.
  if (error) {
    std::cerr << "Could not parse command line: " << error->What() << std::endl;
    return 1;
  }

  if (enable) {
    // etc..
  }
}
```
