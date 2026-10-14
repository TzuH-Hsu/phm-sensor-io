# Development

How to build and test this library on a development machine. The Makefile is
the only executable contract: CI runs the same targets (`make ci-pr`, an alias
of `make verify`).

## Common commands

| Command | What it does |
| --- | --- |
| `make setup` | Configures the `dev` build in `build/dev` and lists every development tool missing from `PATH`; installs nothing |
| `make build` | Builds the library and the unit tests in `build/dev` |
| `make test` | Runs the unit tests on the plain build and on the AddressSanitizer + UndefinedBehaviorSanitizer build (`build/asan-ubsan`) |
| `make test-one TEST=<regex>` | Runs only the unit tests whose `<Suite>.<Test>` name matches the regular expression (`ctest -R`) on the `dev` build |
| `make verify` | Runs every lint and every test; the check to pass before opening a pull request |
| `make clean` | Removes `build/` and nothing else |

`make help` lists every target.

## Tools

The tools and how to install them are listed in
[CONTRIBUTING.md](../CONTRIBUTING.md#getting-started). The C++ build needs a
C++20 compiler, CMake 3.28 or later and Ninja; `make lint-cpp` (part of
`make lint`) also needs clang-format and clang-tidy.

## Layout

| Path | Contents |
| --- | --- |
| `include/phm/sensor_io/` | Public headers, included as `"phm/sensor_io/<name>.hpp"` |
| `src/` | Library sources and private headers; CMake target `phm_sensor_io`, alias `phm_sensor_io::phm_sensor_io` |
| `tests/unit/` | GoogleTest unit tests, one executable per test file |
| `cmake/` | Warning flags, sanitizer flags and the pinned test dependencies |
| `CMakePresets.json` | Build presets `dev` and `asan-ubsan`, each building into `build/<preset>` |

The C++ namespace is `phm::sensor_io`. CMake options start with
`PHM_SENSOR_IO_`.
