# phm-sensor-io

<p align="center">
  <strong>A reusable sensor I/O layer for industrial PHM edge devices. Modbus RTU first.</strong>
</p>

<p align="center">
  <a href="https://github.com/TzuH-Hsu/phm-sensor-io/actions/workflows/ci.yml"><img alt="CI" src="https://github.com/TzuH-Hsu/phm-sensor-io/actions/workflows/ci.yml/badge.svg?branch=main"></a>
  <img alt="Status" src="https://img.shields.io/badge/Status-Early-orange">
  <a href="LICENSE"><img alt="License Apache 2.0" src="https://img.shields.io/badge/License-Apache%202.0-D22128"></a>
  <img alt="Bus: Modbus RTU" src="https://img.shields.io/badge/Bus-Modbus%20RTU-1d76db">
  <img alt="Platform: Linux" src="https://img.shields.io/badge/Platform-Linux-0e8a16">
  <img alt="C++20" src="https://img.shields.io/badge/C%2B%2B-20-00599C">
</p>

Reading industrial sensors on a Linux edge device is the same problem every time: frame a bus, map registers to named points, poll them on a schedule, decode and scale the values, and tell the caller when a reading cannot be trusted. `phm-sensor-io` is that layer, written once, with a clean seam between **how the bus works** and **what this particular deployment measures**.

Modbus RTU is the first transport. The abstraction is written so that adding another bus does not change a single line in the code above it.

> **Status: early.** The API is not stable; expect breaking changes before `v1.0`. Written in C++20 and built with CMake (Ninja generator).

## Design

Three ideas carry the whole library:

- **Points are configuration, not code.** A deployment describes its points — slave address, register, type, scale, unit — in a file. Adding a sensor never means recompiling the application.
- **Every reading carries a quality flag.** `ok`, `timeout`, `crc_error`, `out_of_range`, `stale`. Nothing is silently interpolated or dropped; deciding what to do with a bad reading belongs to the layer above.
- **The raw register value is kept alongside the decoded one.** Scaling factors get entered wrong, and when they do you want to recompute rather than re-collect.

## Scope

| In scope | Out of scope |
| --- | --- |
| Bus transport and framing | Storage, uplink, inference |
| Point/register mapping from configuration | Anything specific to one deployment or site |
| Polling schedule, retry, timeout | Alerting and notification |
| Value decoding, scaling, unit handling | Device provisioning |
| Quality signalling | |

## Getting started

From a fresh clone to a passing build, passing tests and a local run. Every make target, where each tool version is pinned, and more troubleshooting are in [docs/development.md](docs/development.md).

### 1. Install the tools

| Tool | Version | Where the version is set | Needed for |
| --- | --- | --- | --- |
| C++ compiler (GCC or Clang) | C++20; CI uses the runner image's GCC | `CMakeLists.txt` | `make build` |
| CMake | 3.28 or later; CI installs a pinned release | `CMakeLists.txt` (minimum); `scripts/tool-pins.extra` (CI) | `make build` |
| Ninja | CI installs a pinned release | `scripts/tool-pins.extra` | `make build` |
| GoogleTest | Pinned; the first configure downloads it | `cmake/Dependencies.cmake` | `make test` |
| clang-format, clang-tidy | CI uses the runner image's; `clang-format-22` and `clang-tidy-22` are preferred when present | The CI runner image; `Makefile` (`CLANG_FORMAT`, `CLANG_TIDY`) | `make lint` |
| markdownlint-cli2, yamllint, actionlint, gitleaks, lychee | Pinned | `scripts/install-ci-tools.sh` | `make lint` |
| syft | Pinned | `scripts/tool-pins.extra` | `make lint` |
| Node.js | 22 or later (markdownlint-cli2 needs it); CI uses the current LTS | `.github/workflows/ci.yml` (`node-version`) | `make lint` |
| Python 3 | CI uses the runner image's | The CI runner image | `make lint` |

Step 3 installs the pinned versions. On Ubuntu 24.04, the image CI runs on, this installs the compiler, the build tools, the C++ lint tools and what step 3 needs:

```bash
sudo apt-get install -y g++ make git cmake ninja-build clang-format clang-tidy python3 pipx curl unzip
```

Then install Node.js 22 or later from [nodejs.org](https://nodejs.org/) or with a Node version manager.

### 2. Get the source

```bash
git clone https://github.com/TzuH-Hsu/phm-sensor-io.git && cd phm-sensor-io
```

The repository has no submodules.

### 3. Set up the environment once

```bash
ALLOW_LOCAL_INSTALL=1 make ci-tools && pipx ensurepath && export PATH="$HOME/.local/bin:$PATH" && make setup
```

On Linux x86_64, `make ci-tools` installs the lint tools, CMake and Ninja at the pinned versions (binaries into `/usr/local/bin`, using sudo when that is not writable; markdownlint-cli2 with `npm install -g`; yamllint with pipx into `~/.local/bin`, which `pipx ensurepath` puts on `PATH` for new shells and the `export` for this one), and `make setup` then prints `missing: <tool>` for every tool still not on `PATH` and configures the `dev` build in `build/dev`, downloading GoogleTest; it installs nothing itself. On another platform, install the tools with the commands in [CONTRIBUTING.md](CONTRIBUTING.md#getting-started) and run only `make setup`.

### 4. Build

```bash
make build
```

Builds the library and the unit tests in `build/dev`.

### 5. Run the tests

```bash
make test                     # every test, on the plain and on the sanitized build
make test-one TEST=Skeleton   # only the tests whose <Suite>.<Test> name matches
```

`make test` builds and runs the tests on `build/dev` and on `build/asan-ubsan` (AddressSanitizer and UndefinedBehaviorSanitizer), and ctest reports `100% tests passed` for each; `make test-one` runs only the matching tests on `build/dev`.

### 6. Run it locally

```bash
build/dev/tests/skeleton_test
```

The library has no bus code yet, so the one program that runs is this test executable: it prints the GoogleTest report for `Skeleton.PublicHeadersCompile`, ends with `[  PASSED  ] 1 test.` and exits 0. Reading registers from a simulated Modbus RTU slave over a pseudo-terminal arrives with #1.

To use the library from another CMake project, install it with `cmake --install build/dev --prefix <dir>` and call `find_package(phm_sensor_io CONFIG REQUIRED)` there with `<dir>` on `CMAKE_PREFIX_PATH`; `make test-install` does this with [`tests/consumer/`](tests/consumer/CMakeLists.txt).

### 7. Before opening a pull request

```bash
make verify
```

Runs every lint and both test builds, the same targets CI runs (`make ci-pr` is an alias); it makes network calls (the licence lookups of `make lint-licenses`, and the GoogleTest download of a new build tree).

### 8. Common problems

- **`make setup` prints `missing: <tool>` and fails.** That tool is not on `PATH`; install it as in steps 1 and 3. Building and testing need only the compiler, CMake and Ninja, so steps 4 to 6 work before every lint tool is installed.
- **`make ci-tools` fails with `linux x86_64` in the message.** Its release binaries exist for Linux x86_64 only; on macOS or arm64, install each tool with the command in [CONTRIBUTING.md](CONTRIBUTING.md#getting-started).
- **The first configure fails while downloading GoogleTest.** Each new build tree downloads it from github.com and checks its SHA-256; run it with network access, after `make clean` if a half-configured `build/` is left behind.

More in [docs/development.md](docs/development.md#troubleshooting).

There is no published package yet. Watch [releases](https://github.com/TzuH-Hsu/phm-sensor-io/releases) or the open issues for the first `v0.1.0`.

## Contributing

Work from an issue, branch as `<type>/<issue#>-<slug>`, use Conventional Commits, open a PR. `main` is protected: PR required, `ci` must pass. Conventions live in [`AGENTS.md`](AGENTS.md) (canonical) and [`CONTRIBUTING.md`](CONTRIBUTING.md).

Two rules are specific to this repository and non-negotiable:

- **No deployment-specific content** — no organisation names, site names, equipment models, point tables, register maps, data samples or project codenames, anywhere, including tests and fixtures.
- **No dependency on a downstream project** — applications depend on this library, never the reverse.

Contributions are provided under Apache-2.0 by default ([§5](LICENSE)). No CLA.

## Security

Report vulnerabilities privately — see [`SECURITY.md`](SECURITY.md). Please do not open a public issue for a suspected vulnerability.

## Third-party licences

The Modbus RTU transport uses [libmodbus](https://libmodbus.org/), licensed LGPL-2.1-or-later. This library uses it under **LGPL-3.0** (the "or later" option). This library's own code stays Apache-2.0; the LGPL applies to libmodbus itself, not to code that links it dynamically.

Anyone who ships this library in a product must meet the LGPL-3.0 §4 conditions for libmodbus. The licence texts are authoritative; in short:

- **Link dynamically.** libmodbus is loaded as a shared library (`libmodbus.so`) and ships as its own file, so the recipient can replace it with a modified version (§4(d)(1)). Do not link it statically and do not copy its source into this library or into an application.
- **Give notice.** Each copy of the product states prominently that it uses libmodbus and that libmodbus is covered by the LGPL, and includes the LGPL-3.0 and GPL-3.0 texts and the libmodbus copyright notices. If the product displays copyright notices while it runs, the libmodbus copyright notice is displayed among them, with a reference to the licence texts (§4(c)).
- **Provide the source.** Supply the complete corresponding source of the libmodbus version shipped. If libmodbus itself is modified, the modified source is provided under the same licence.
- **Allow modification and debugging.** The product's licence terms must not restrict the recipient from modifying the libmodbus portion, or from reverse engineering to debug such modifications.
- **Installation information** (§4(e), GPL-3.0 §6) is required only when the product is a User Product as defined there.

Dependency licence policy, in short: permissive licences are used directly; weak copyleft is allowed under conditions (LGPL through dynamic linking only, MPL-2.0 for unmodified files with source available) and each such component is listed in [`scripts/licence-exceptions.json`](scripts/licence-exceptions.json); strong copyleft (GPL, AGPL, SSPL) is not linked.

## License

[Apache-2.0](LICENSE). Copyright 2026 Tzu-Hsuan Hsu. Attribution requirements are in [`NOTICE`](NOTICE).
