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
| `make test-install` | Installs `build/dev` into a prefix under `build/`, builds the consumer project `tests/consumer` against it with `find_package()` and runs it, and checks that `find_package()` fails without `CMAKE_PREFIX_PATH` and for another major version; CI runs it after `make verify` |
| `make verify` | Runs every lint and every test; the check to pass before opening a pull request |
| `make clean` | Removes `build/` and nothing else |

`make help` lists every target.

## Using the library from another project

`cmake --install build/dev --prefix <dir>` installs the public headers and a
CMake package into `<dir>`; another CMake project then calls
`find_package(phm_sensor_io CONFIG REQUIRED)` with `<dir>` on
`CMAKE_PREFIX_PATH` and links `phm_sensor_io::phm_sensor_io`, as
[`tests/consumer/`](../tests/consumer/CMakeLists.txt) does. The package version
follows the release, and `find_package()` accepts it only for a request with
the same major version.

## Tools

The tools and how to install them are listed in
[CONTRIBUTING.md](../CONTRIBUTING.md#getting-started). The C++ build needs a
C++20 compiler, CMake 3.28 or later, Ninja, pkg-config and the shared
libmodbus; `make lint-cpp` (part of `make lint`) also needs clang-format and
clang-tidy.

Where each version is set:

| Versions | File |
| --- | --- |
| markdownlint-cli2, yamllint, actionlint, gitleaks, lychee | `scripts/install-ci-tools.sh` (the template's installer, kept identical to upstream) |
| syft, CMake, Ninja (with the SHA-256 of the Ninja archive) | `scripts/tool-pins.extra` |
| nlohmann/json and GoogleTest (with the SHA-256 of each release archive) | `cmake/Dependencies.cmake` |
| libmodbus, pkg-config | The distribution's packages; CI installs `CI_SYSTEM_PACKAGES` (`Makefile`) with apt-get |
| Minimum CMake, C++ standard | `CMakeLists.txt`, `CMakePresets.json` |
| Node.js | `.github/workflows/ci.yml` (`node-version`) |
| Compiler, clang-format, clang-tidy, Python 3 | The CI runner image (`ubuntu-latest` unless `CI_RUNNER_LABELS` names another) |

`make check-tool-versions` compares the pins in the first two files with the
latest upstream releases.

On Linux x86_64, `ALLOW_LOCAL_INSTALL=1 make ci-tools` installs the tools CI
installs, at the versions CI uses: actionlint, gitleaks, lychee, syft, CMake
and Ninja into `/usr/local/bin` (`INSTALL_DIR`; sudo is used when it is not
writable), markdownlint-cli2 with `npm install -g` and yamllint with pipx (or
`pip install --user` when pipx is absent). It needs `curl`, `unzip`, Node.js
and pipx or pip, and does not install the compiler, clang-format, clang-tidy,
Node.js or Python. Without `ALLOW_LOCAL_INSTALL=1` outside CI it refuses the
npm and pip installs. With its default tool list it then runs
`make ci-system-packages`, which installs `libmodbus-dev` and `pkg-config` with
apt-get, under the same `ALLOW_LOCAL_INSTALL=1` rule.

## Layout

| Path | Contents |
| --- | --- |
| `include/phm/sensor_io/` | Public headers, included as `"phm/sensor_io/<name>.hpp"` |
| `src/` | Library sources and private headers; CMake target `phm_sensor_io`, alias `phm_sensor_io::phm_sensor_io` |
| `tests/unit/` | GoogleTest unit tests, one executable per test file |
| `tests/support/` | Test support, not installed: the simulated Modbus RTU slave (a pseudo-terminal pair served by its own thread) and access to the fixtures |
| `tests/fixtures/` | Configuration documents and Modbus RTU frames the tests read; every value in them is invented |
| `tests/consumer/` | A separate CMake project that builds against the installed package (`make test-install`) |
| `cmake/` | Warning flags, sanitizer flags, the dependencies (libmodbus through pkg-config, the pinned nlohmann/json and GoogleTest) and the template of the installed package configuration file |
| `CMakePresets.json` | Build presets `dev` and `asan-ubsan`, each building into `build/<preset>` |

The C++ namespace is `phm::sensor_io`. CMake options start with
`PHM_SENSOR_IO_`.

## Troubleshooting

| Symptom | Cause and fix |
| --- | --- |
| The first `make configure` (or `make setup`, `make build`) fails while downloading GoogleTest | Each new build tree downloads the GoogleTest release archive from github.com and checks its SHA-256. Run it with network access once per build tree; `make clean` removes a half-configured tree. A GoogleTest installed at exactly the pinned version is used instead when CMake finds it, except in `build/asan-ubsan`, which always builds it from source |
| `make lint-cpp` prints `install: brew install llvm` on Linux | The hint names Homebrew; on Ubuntu, `sudo apt-get install clang-format clang-tidy` provides both |
| `npm install -g` fails with `EACCES` during `make ci-tools` | Node.js is installed in a directory the user cannot write. Use a Node.js installed in the home directory (a nodejs.org archive unpacked there, or a Node version manager) |
| The configure fails with `None of the required 'libmodbus' found` | pkg-config cannot find libmodbus. Install `libmodbus-dev` and `pkg-config` (Ubuntu) or `libmodbus` and `pkg-config` (Homebrew). A configure that finds only a static `libmodbus.a` fails on purpose: libmodbus must be linked dynamically |
| `make lint-licenses` (part of `make lint` and `make verify`) fails without network access | syft looks licences up in the package registries; run it with network access |
