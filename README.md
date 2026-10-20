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

## Usage

The caller builds or loads a configuration, creates a `SensorBus` from it, and calls `read()` for a point whenever its own polling cycle says so. The library has no timer and no thread of its own, and never stamps a time: the caller assigns sample times.

```cpp
#include "phm/sensor_io/sensor_io.hpp"

namespace sio = phm::sensor_io;

auto config = sio::load_config("/etc/example/points.json");  // or parse_config(text), or a BusConfig built in code
if (!config) {
  // config.error().pointer is a JSON Pointer such as "/points/1/values/0/register"
}
auto bus = sio::SensorBus::create(std::move(config).value());
// ... on each polling cycle:
auto reading = bus->read("point-1");  // Result<PointReading, ReadError>
if (reading && reading->quality == sio::Quality::ok) {
  for (const sio::ValueReading& value : reading->values) {
    // value.value (scaled), value.raw (register words as received), value.unit
  }
}
```

Everything is in namespace `phm::sensor_io`; `phm/sensor_io/sensor_io.hpp` includes every public header, and no public header includes a libmodbus header or names a libmodbus type. The headers carry the full contract; the rest of this section is the overview.

### Configuration

A configuration is a JSON document (`parse_config`, `load_config`) or a `BusConfig` built in code (`validate`, `SensorBus::create`); both are checked against the same rules. Unknown keys are rejected, an integer key given `19200.0` or `"19200"` is an error, and omitted keys take their defaults. Register numbers are zero-based addresses as sent on the wire, not the 3xxxx/4xxxx documentation notation.

```json
{
  "bus": "modbus_rtu",
  "ports": [{ "name": "line-a", "device": "/dev/ttyUSB0", "baud": 9600, "parity": "none" }],
  "points": [{
    "name": "point-1", "port": "line-a", "slave_address": 10, "function": "read_input_registers",
    "values": [
      { "name": "value_a", "register": 16, "type": "uint16", "scale": 0.5,  "unit": "unit_a" },
      { "name": "value_b", "register": 17, "type": "int16",  "scale": 0.25, "unit": "unit_b", "min": -40 }
    ]
  }]
}
```

| Key | Type | Default | Rule |
| --- | --- | --- | --- |
| `bus` | string | required | `"modbus_rtu"` only |
| `ports`, `points` | array | required | at least 1 each |
| port `name` | string | required | `^[A-Za-z0-9_.-]{1,32}$`, unique among ports |
| port `device` | string | required | absolute path of the serial device, opened as given |
| port `baud` | integer | `19200` | 1200, 2400, 4800, 9600, 19200, 38400, 57600 or 115200 |
| port `parity` | string | `"even"` | `"none"`, `"even"` or `"odd"`; data bits are always 8 |
| port `stop_bits` | integer | `1` | `1` or `2` |
| port `response_timeout_ms` | integer | `500` | 10–10000 |
| port `retries` | integer | `1` | 0–5 |
| point `name` | string | required | `^[A-Za-z0-9_.-]{1,64}$`, unique among points |
| point `port` | string | required | a port `name` |
| point `slave_address` | integer | required | 1–247 (0 is broadcast) |
| point `function` | string | required | `"read_holding_registers"` (0x03) or `"read_input_registers"` (0x04) |
| point `values` | array | required | at least 1; the values span 1–125 registers, read in one request |
| value `name` | string | required | `^[A-Za-z0-9_.-]{1,32}$`, unique within the point |
| value `register` | integer | required | 0–65535; a 32-bit value takes this register and the next, so it cannot start at 65535 |
| value `type` | string | required | `"int16"`, `"uint16"` (1 register), `"int32"`, `"uint32"`, `"float32"` (2 registers) |
| value `word_order` | string | `"high_first"` | `"high_first"` or `"low_first"`; only for 32-bit types, the key is rejected on a 16-bit one |
| value `scale`, `offset` | number | `1.0`, `0.0` | finite; the value is `raw * scale + offset` |
| value `unit` | string | `""` | 0–16 characters, copied to the result |
| value `min`, `max` | number or null | `null` | inclusive bounds on the scaled value; `min <= max` |

A configuration error is the first error found, as `ConfigError{pointer, message}`: `pointer` is an RFC 6901 JSON Pointer to the offending element (for example `/ports/0/speed` for an unknown key), the same for a document and for the equivalent `BusConfig`, and `""` for an unreadable file or invalid JSON.

### Readings and quality

`read()` sends one request per point and returns a `PointReading`: one `quality` for the whole read, the exact `fault` behind it, the `exception_code` of an exception response, the number of request frames sent (`attempts`), and one `ValueReading` per configured value, in configuration order. A failed read is still a result; it is never dropped or filled in from an earlier read.

| Outcome | `fault` | `quality` | Values |
| --- | --- | --- | --- |
| Valid reply, every value within range | `none` | `ok` | `value` and `raw` set |
| Valid reply, a value outside `[min, max]` or a `float32` NaN or infinity | `none` | `out_of_range` | that value's `value` empty; every `raw` set |
| No complete reply within the response timeout | `no_reply` | `timeout` | empty |
| The port could not be opened, read or written | `io_error` | `timeout` | empty |
| Reply with a CRC-16/MODBUS mismatch | `crc_mismatch` | `crc_error` | empty |
| Reply from another slave address, with another function code or a wrong byte count | `malformed_reply` | `crc_error` | empty |
| Modbus exception response | `exception_response` | `crc_error` | empty |

`stale` is never produced here: whether a reading is too old is the consuming application's decision.

### Errors, retries and threads

- **Errors.** Bus failures are only `quality` and `fault` in the `PointReading`. A point name that is not configured is `ReadError::unknown_point` and sends nothing. Configuration errors are `ConfigError`. The library throws nothing but `std::bad_alloc`.
- **Line state.** Before every request frame, the first attempt of a `read()` as well as each retry, the line must have been silent for the RTU inter-frame silence since the last frame sent or received on the port (3.5 characters of 11 bits at the port's baud rate, about 4.0 ms at 9600 baud and 2.0 ms at 19200, or 1.75 ms above 19200 baud), and the port's receive buffer is discarded. Bytes that keep arriving are discarded until the line is quiet, for at most the response timeout. A reply that arrives before the next request is never taken as that request's reply; one that arrives after it cannot be told apart from that request's own reply. Discarded bytes are not reported and do not count as attempts.
- **Retries.** `no_reply`, `crc_mismatch` and `malformed_reply` are retried; `exception_response` and `io_error` are not. One `read()` sends at most `1 + retries` request frames and reports the last attempt, so it takes at most about `(1 + retries) × response_timeout` plus the frame times and silences. libmodbus's own error recovery is off.
- **Ports.** A port is opened on the first `read()` of one of its points and stays open. After `io_error` it is closed and opened again on its next `read()`; other ports are unaffected.
- **Threads.** `read()` may be called from several threads at once. The library holds one lock per port, so reads on one port run one after another and a serial line never has more than one request outstanding; reads on different ports run in parallel. `create`, move and destruction must not overlap with other calls on the same object.

## Getting started

From a fresh clone to a passing build, passing tests and a local run. Every make target, where each tool version is pinned, and more troubleshooting are in [docs/development.md](docs/development.md).

### 1. Install the tools

| Tool | Version | Where the version is set | Needed for |
| --- | --- | --- | --- |
| C++ compiler (GCC or Clang) | C++20; CI uses the runner image's GCC | `CMakeLists.txt` | `make build` |
| CMake | 3.28 or later; CI installs a pinned release | `CMakeLists.txt` (minimum); `scripts/tool-pins.extra` (CI) | `make build` |
| Ninja | CI installs a pinned release | `scripts/tool-pins.extra` | `make build` |
| libmodbus (shared library) and pkg-config | The distribution's; CI installs Ubuntu's `libmodbus-dev` | `Makefile` (`CI_SYSTEM_PACKAGES`) | `make build` |
| nlohmann/json | Pinned; the first configure downloads it unless that version is installed | `cmake/Dependencies.cmake` | `make build` |
| GoogleTest | Pinned; the first configure downloads it | `cmake/Dependencies.cmake` | `make test` |
| clang-format, clang-tidy | CI uses the runner image's; `clang-format-22` and `clang-tidy-22` are preferred when present | The CI runner image; `Makefile` (`CLANG_FORMAT`, `CLANG_TIDY`) | `make lint` |
| markdownlint-cli2, yamllint, actionlint, gitleaks, lychee | Pinned | `scripts/install-ci-tools.sh` | `make lint` |
| syft | Pinned | `scripts/tool-pins.extra` | `make lint` |
| Node.js | 22 or later (markdownlint-cli2 needs it); CI uses the current LTS | `.github/workflows/ci.yml` (`node-version`) | `make lint` |
| Python 3 | CI uses the runner image's | The CI runner image | `make lint` |

Step 3 installs the pinned versions. On Ubuntu 24.04, the image CI runs on, this installs the compiler, the build tools, the C++ lint tools and what step 3 needs:

```bash
sudo apt-get install -y g++ make git cmake ninja-build clang-format clang-tidy python3 pipx curl unzip libmodbus-dev pkg-config
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
make test-one TEST=SensorBus  # only the tests whose <Suite>.<Test> name matches
```

`make test` builds and runs the tests on `build/dev` and on `build/asan-ubsan` (AddressSanitizer and UndefinedBehaviorSanitizer), and ctest reports `100% tests passed` for each; `make test-one` runs only the matching tests on `build/dev`.

### 6. Run it locally

```bash
build/dev/tests/sensor_bus_test
```

The library has no executable of its own, so this test program is the local run: it reads registers from a simulated Modbus RTU slave over a pseudo-terminal through libmodbus, exactly as from a serial device, with no hardware. It prints the GoogleTest report for the `SensorBusTest` cases, ends with `[  PASSED  ]` and exits 0.

To use the library from another CMake project, install it with `cmake --install build/dev --prefix <dir>` and call `find_package(phm_sensor_io CONFIG REQUIRED)` there with `<dir>` on `CMAKE_PREFIX_PATH`; `make test-install` does this with [`tests/consumer/`](tests/consumer/CMakeLists.txt).

### 7. Before opening a pull request

```bash
make verify
```

Runs every lint and both test builds, the same targets CI runs (`make ci-pr` is an alias); it makes network calls (the licence lookups of `make lint-licenses`, and the GoogleTest download of a new build tree).

### 8. Common problems

- **`make setup` prints `missing: <tool>` and fails.** That tool is not on `PATH`; install it as in steps 1 and 3. Building and testing need only the compiler, CMake and Ninja, so steps 4 to 6 work before every lint tool is installed.
- **`make ci-tools` fails with `linux x86_64` in the message.** Its release binaries exist for Linux x86_64 only; on macOS or arm64, install each tool with the command in [CONTRIBUTING.md](CONTRIBUTING.md#getting-started).
- **The first configure fails while downloading GoogleTest or nlohmann/json.** Each new build tree downloads them from github.com and checks their SHA-256; run it with network access, after `make clean` if a half-configured `build/` is left behind.
- **The configure fails with `None of the required 'libmodbus' found`.** pkg-config cannot find libmodbus: install `libmodbus-dev` and `pkg-config` (Ubuntu) or `libmodbus` and `pkg-config` (Homebrew). A configure that finds only a static `libmodbus.a` fails on purpose: libmodbus must be linked dynamically.

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

The JSON configuration parser is [nlohmann/json](https://github.com/nlohmann/json), licensed MIT. It is header-only and compiled into this library; a product that ships the library includes its copyright and permission notice.

Dependency licence policy, in short: permissive licences are used directly; weak copyleft is allowed under conditions (LGPL through dynamic linking only, MPL-2.0 for unmodified files with source available) and each such component is listed in [`scripts/licence-exceptions.json`](scripts/licence-exceptions.json); strong copyleft (GPL, AGPL, SSPL) is not linked.

## License

[Apache-2.0](LICENSE). Copyright 2026 Tzu-Hsuan Hsu. Attribution requirements are in [`NOTICE`](NOTICE).
