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
</p>

Reading industrial sensors on a Linux edge device is the same problem every time: frame a bus, map registers to named points, poll them on a schedule, decode and scale the values, and tell the caller when a reading cannot be trusted. `phm-sensor-io` is that layer, written once, with a clean seam between **how the bus works** and **what this particular deployment measures**.

Modbus RTU is the first transport. The abstraction is written so that adding another bus does not change a single line in the code above it.

> **Status: early.** The API is not stable; expect breaking changes before `v1.0`. Language and build system are being settled — see the open issues.

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

```bash
make help     # all targets
make verify   # lint + tests — the gate before every PR
```

There is no published package yet. Watch [releases](https://github.com/TzuH-Hsu/phm-sensor-io/releases) or the open issues for the first `v0.1.0`.

## Contributing

Work from an issue, branch as `<type>/<issue#>-<slug>`, use Conventional Commits, open a PR. `main` is protected: PR required, `ci` must pass. Conventions live in [`AGENTS.md`](AGENTS.md) (canonical) and [`CONTRIBUTING.md`](CONTRIBUTING.md).

Two rules are specific to this repository and non-negotiable:

- **No deployment-specific content** — no organisation names, site names, equipment models, point tables, register maps, data samples or project codenames, anywhere, including tests and fixtures.
- **No dependency on a downstream project** — applications depend on this library, never the reverse.

Contributions are provided under Apache-2.0 by default ([§5](LICENSE)). No CLA.

## Security

Report vulnerabilities privately — see [`SECURITY.md`](SECURITY.md). Please do not open a public issue for a suspected vulnerability.

## License

[Apache-2.0](LICENSE). Copyright 2026 Tzu-Hsuan Hsu. Attribution requirements are in [`NOTICE`](NOTICE).
