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

Where each version is set:

| Versions | File |
| --- | --- |
| markdownlint-cli2, yamllint, actionlint, gitleaks, lychee | `scripts/install-ci-tools.sh` (the template's installer, kept identical to upstream) |
| syft, CMake, Ninja (with the SHA-256 of the Ninja archive) | `scripts/tool-pins.extra` |
| GoogleTest (with the SHA-256 of the release archive) | `cmake/Dependencies.cmake` |
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
npm and pip installs.

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

## Troubleshooting

| Symptom | Cause and fix |
| --- | --- |
| The first `make configure` (or `make setup`, `make build`) fails while downloading GoogleTest | Each new build tree downloads the GoogleTest release archive from github.com and checks its SHA-256. Run it with network access once per build tree; `make clean` removes a half-configured tree. A GoogleTest installed at exactly the pinned version is used instead when CMake finds it, except in `build/asan-ubsan`, which always builds it from source |
| `make lint-cpp` prints `install: brew install llvm` on Linux | The hint names Homebrew; on Ubuntu, `sudo apt-get install clang-format clang-tidy` provides both |
| `npm install -g` fails with `EACCES` during `make ci-tools` | Node.js is installed in a directory the user cannot write. Use a Node.js installed in the home directory (a nodejs.org archive unpacked there, or a Node version manager) |
| `make lint-licenses` (part of `make lint` and `make verify`) fails without network access | syft looks licences up in the package registries; run it with network access |
