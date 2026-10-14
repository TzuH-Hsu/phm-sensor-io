#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Install checksum-verified release binaries for the CI tools this repository
# adds on top of the template kit.
#
# The template's scripts/install-ci-tools.sh knows only its own kit tools and
# stays identical to upstream. Tools this repository adds are pinned in
# scripts/tool-pins.extra (`<tool> <version> <source>`), which
# `make check-tool-versions` also reads, so each pin has one home and is watched
# for drift. This script installs the tools named on its command line at the
# versions pinned there.
#
# Usage: scripts/install-extra-tools.sh <tool> [<tool>...]
#        make ci-tools   (installs the kit tools and these)
#
# Known tools: syft (GitHub release tarball, sha256 verified against the
# release's checksum file), cmake (release tarball, sha256 verified against the
# release's checksum file, installed as a tree with its binaries linked into
# INSTALL_DIR), ninja (release zip, sha256 pinned in the `# sha256` comment of
# its row in scripts/tool-pins.extra because upstream publishes no checksum
# file).
#
# INSTALL_DIR (default /usr/local/bin) can point at a throwaway prefix for
# testing. sudo is used only when the target is not writable by the caller.
# Linux x86_64 only, like the kit tarball tools.

set -euo pipefail

INSTALL_DIR="${INSTALL_DIR:-/usr/local/bin}"
PINS_FILE="${PINS_FILE:-scripts/tool-pins.extra}"
KNOWN_TOOLS="cmake, ninja, syft"

if [ "$#" -eq 0 ]; then
  echo "usage: $0 <tool> [<tool>...]  (known: ${KNOWN_TOOLS})" >&2
  exit 2
fi

if [ "$(uname -s)" != "Linux" ] || [ "$(uname -m)" != "x86_64" ]; then
  echo "::error::$0 installs linux x86_64 assets only (this is $(uname -s) $(uname -m))" >&2
  exit 1
fi

pinned_version() {
  local tool="$1" version
  version="$(awk -v t="$tool" '$1 == t { print $2; exit }' "$PINS_FILE")"
  if [ -z "$version" ]; then
    echo "::error::no pin for '$tool' in $PINS_FILE" >&2
    exit 1
  fi
  printf '%s' "$version"
}

place_binary() {
  local found="$1" binary="$2"
  mkdir -p "$INSTALL_DIR" 2>/dev/null || sudo mkdir -p "$INSTALL_DIR"
  if [ -w "$INSTALL_DIR" ]; then
    install -m 0755 "$found" "${INSTALL_DIR}/${binary}"
  else
    sudo install -m 0755 "$found" "${INSTALL_DIR}/${binary}"
  fi
}

# The digest in the `# sha256 <digest>` comment of <tool>'s row.
pinned_sha256() {
  local tool="$1" sha256
  sha256="$(awk -v t="$tool" '$1 == t && $4 == "#" && $5 == "sha256" { print $6; exit }' "$PINS_FILE")"
  if [ -z "$sha256" ]; then
    echo "::error::no '# sha256 <digest>' comment for '$tool' in $PINS_FILE" >&2
    exit 1
  fi
  printf '%s' "$sha256"
}

# Verify <asset> against its own line in the release checksum file <checksums>.
verify_listed() {
  local asset="$1" checksums="$2" hash
  hash="$(awk -v a="$asset" '$2 == a || $2 == "*" a { print $1; exit }' "$checksums")"
  if [ -z "$hash" ]; then
    echo "::error::$asset is not listed in $checksums" >&2
    exit 1
  fi
  printf '%s  %s\n' "$hash" "$asset" | sha256sum -c -
}

# Download a release tarball and its checksum file, verify the tarball against
# its own line in that file, and install the named binary from it.
install_tar_binary() {
  local repo="$1" tag="$2" asset="$3" checksums="$4" binary="$5"
  local url="https://github.com/${repo}/releases/download/${tag}"
  curl -fsSLO "$url/$asset"
  curl -fsSLO "$url/$checksums"
  verify_listed "$asset" "$checksums"
  mkdir -p "extract-${binary}"
  tar -xzf "$asset" -C "extract-${binary}"
  local found
  found="$(find "extract-${binary}" -type f -name "$binary" -print -quit)"
  if [ -z "$found" ]; then
    echo "::error::binary '$binary' not found in $asset" >&2
    exit 1
  fi
  place_binary "$found" "$binary"
}

# CMake needs its share/ tree next to the binaries, so the whole release is
# unpacked beside INSTALL_DIR and its three binaries are linked into it. CMake
# resolves the link to find its modules.
install_cmake() {
  local version="$1"
  local asset="cmake-${version}-linux-x86_64.tar.gz"
  local checksums="cmake-${version}-SHA-256.txt"
  local url="https://github.com/Kitware/CMake/releases/download/v${version}"
  local root
  root="$(dirname "$INSTALL_DIR")/lib/cmake-${version}"
  curl -fsSLO "$url/$asset"
  curl -fsSLO "$url/$checksums"
  verify_listed "$asset" "$checksums"
  local sudo_cmd=""
  if ! mkdir -p "$root" 2>/dev/null || [ ! -w "$root" ]; then
    sudo_cmd="sudo"
    sudo mkdir -p "$root"
  fi
  $sudo_cmd tar -xzf "$asset" -C "$root" --strip-components=1
  mkdir -p "$INSTALL_DIR" 2>/dev/null || sudo mkdir -p "$INSTALL_DIR"
  local link_sudo=""
  [ -w "$INSTALL_DIR" ] || link_sudo="sudo"
  local binary
  for binary in cmake ctest cpack; do
    $link_sudo ln -sf "${root}/bin/${binary}" "${INSTALL_DIR}/${binary}"
  done
}

install_ninja() {
  local version="$1" sha256="$2"
  local asset="ninja-linux.zip"
  curl -fsSLO "https://github.com/ninja-build/ninja/releases/download/v${version}/${asset}"
  printf '%s  %s\n' "$sha256" "$asset" | sha256sum -c -
  mkdir -p extract-ninja
  unzip -q "$asset" -d extract-ninja
  place_binary extract-ninja/ninja ninja
}

PINS_FILE="$(cd "$(dirname "$PINS_FILE")" && pwd)/$(basename "$PINS_FILE")"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
cd "$tmp"

for tool in "$@"; do
  case "$tool" in
    syft)
      version="$(pinned_version syft)"
      install_tar_binary "anchore/syft" "v${version}" \
        "syft_${version}_linux_amd64.tar.gz" \
        "syft_${version}_checksums.txt" \
        "syft"
      ;;
    cmake)
      install_cmake "$(pinned_version cmake)"
      ;;
    ninja)
      install_ninja "$(pinned_version ninja)" "$(pinned_sha256 ninja)"
      ;;
    *)
      echo "::error::unknown tool '$tool' (known: ${KNOWN_TOOLS})" >&2
      exit 1
      ;;
  esac
  echo "installed ${tool} $(pinned_version "$tool") to ${INSTALL_DIR}"
done
