# GitHub Project OS - make-target contract: CI workflows call these targets
# and never contain logic of their own. L0 = lint (lint-docs, lint-actions,
# lint-secrets, lint-cpp, lint-licenses, check). L1 = test (ctest on a plain and
# on a sanitized build). verify = L0+L1 (canonical pre-PR gate).
# maintenance = lint-docs-external + check-tool-versions, the weekly drift
# detectors. It runs both and aggregates their exit status, so the workflow step
# stays a bare `make maintenance` and the job's combined failure behaviour is
# reproducible locally. All three are intentionally excluded from
# lint/verify/ci-pr: they make external network calls. The network calls in
# `lint` and `test` are `lint-licenses`, which reads licences from the package
# registries because lock files carry none, and the first configure of a build
# tree, which downloads the pinned, hash-verified test dependencies.
# CHANGELOG.md is excluded from markdownlint: release-please generates it.
# Customize tool invocations HERE, not in .github/workflows/*.

SHELL := /usr/bin/env bash
.PHONY: help setup lint-licenses sbom lint-docs lint-docs-external lint-actions lint-secrets lint-cpp check lint configure build test-unit test-sanitize test test-one clean test-install verify ci-pr ci-tools ci-system-packages check-tool-versions maintenance

.DEFAULT_GOAL := help

help: ## Show this help
	@awk 'BEGIN {FS = ":.*##"; printf "\nUsage: make \033[36m<target>\033[0m\n\n"} \
	/^[a-zA-Z0-9_-]+:.*##/ { printf "  \033[36m%-15s\033[0m %s\n", $$1, $$2 }' \
	$(MAKEFILE_LIST)
	@echo ""

lint-docs: ## Lint markdown and YAML, check internal links
	@command -v markdownlint-cli2 >/dev/null 2>&1 || { echo "install: npm install -g markdownlint-cli2"; exit 1; }
	@command -v yamllint >/dev/null 2>&1 || { echo "install: brew install yamllint (or pip install yamllint)"; exit 1; }
	@command -v lychee >/dev/null 2>&1 || { echo "install: brew install lychee"; exit 1; }
	markdownlint-cli2 '**/*.md' '#node_modules' '#build' '#CHANGELOG.md'
	yamllint .
	lychee --offline --no-progress -- './**/*.md'

lint-docs-external: ## Full external link check (weekly maintenance workflow; not run in CI-PR)
	@command -v lychee >/dev/null 2>&1 || { echo "install: brew install lychee"; exit 1; }
	lychee --config lychee.toml --no-progress -- './**/*.md'

lint-actions: ## Lint GitHub Actions workflows
	@command -v actionlint >/dev/null 2>&1 || { echo "install: brew install actionlint"; exit 1; }
	actionlint

lint-secrets: ## Scan for committed secrets
	@command -v gitleaks >/dev/null 2>&1 || { echo "install: brew install gitleaks"; exit 1; }
	gitleaks detect --no-banner

check: ## Run repo self-consistency scripts (skips scripts not yet added)
	@if [ -x scripts/check-skills.sh ]; then scripts/check-skills.sh; else echo "skip: scripts/check-skills.sh not present yet"; fi
	@if [ -x scripts/check-local-md.sh ]; then scripts/check-local-md.sh; else echo "skip: scripts/check-local-md.sh not present yet"; fi
	@if [ -x scripts/check-label-forms.sh ]; then scripts/check-label-forms.sh; else echo "skip: scripts/check-label-forms.sh not present yet"; fi
	@if [ -x scripts/check-node-tests.sh ]; then scripts/check-node-tests.sh; else echo "skip: scripts/check-node-tests.sh not present yet"; fi
	@if [ -f scripts/test_check_licenses.py ]; then PYTHONDONTWRITEBYTECODE=1 python3 -m unittest -q scripts/test_check_licenses.py; else echo "skip: scripts/test_check_licenses.py not present yet"; fi

lint: lint-docs lint-actions lint-secrets lint-cpp lint-licenses check ## L0 - aggregate all lint/consistency checks

# --- C++ build, tests and static analysis --------------------------------------
# Presets are in CMakePresets.json; each preset builds into build/<preset>.
# The versioned LLVM tool names are preferred so a machine with several LLVM
# releases installed uses the one named first.
CLANG_FORMAT ?= $(firstword $(shell command -v clang-format-22 clang-format 2>/dev/null))
CLANG_TIDY ?= $(firstword $(shell command -v clang-tidy-22 clang-tidy 2>/dev/null))
CXX_SOURCES = git ls-files -z -- '*.cpp' '*.hpp'
CXX_TRANSLATION_UNITS = git ls-files -z -- '*.cpp'
LLVM_HINT = install: brew install llvm, then put "$$(brew --prefix llvm)/bin" on PATH

# The tools `make verify` needs besides clang-format and clang-tidy, which are
# checked separately because their names may carry a version suffix.
# CONTRIBUTING.md says how to install each one. libmodbus, which the library
# links, is checked through pkg-config.
DEV_TOOLS := cmake ninja pkg-config markdownlint-cli2 yamllint lychee node actionlint gitleaks syft python3

# The audit runs before configuring, so one run lists every missing tool even
# when cmake or ninja is among them.
setup: ## Report every development tool missing from PATH and configure the dev build
	@missing=0; \
	for tool in $(DEV_TOOLS); do \
		command -v "$$tool" >/dev/null 2>&1 || { echo "missing: $$tool (see CONTRIBUTING.md)"; missing=1; }; \
	done; \
	test -n "$(CLANG_FORMAT)" || { echo 'missing: clang-format ($(LLVM_HINT))'; missing=1; }; \
	test -n "$(CLANG_TIDY)" || { echo 'missing: clang-tidy ($(LLVM_HINT))'; missing=1; }; \
	if command -v pkg-config >/dev/null 2>&1 && ! pkg-config --exists libmodbus; then \
		echo "missing: libmodbus (see CONTRIBUTING.md)"; missing=1; \
	fi; \
	if command -v cmake >/dev/null 2>&1 && command -v ninja >/dev/null 2>&1; then \
		$(MAKE) --no-print-directory configure || exit 1; \
	fi; \
	if [ "$$missing" -ne 0 ]; then exit 1; fi; \
	echo "setup: every development tool found; build/dev is configured"

configure: ## Configure the host build (build/dev)
	@command -v cmake >/dev/null 2>&1 || { echo "install: brew install cmake"; exit 1; }
	@command -v ninja >/dev/null 2>&1 || { echo "install: brew install ninja"; exit 1; }
	cmake --preset dev

build: configure ## Build the library and the tests (build/dev)
	cmake --build --preset dev

test-unit: build ## Run the unit tests on the host build
	ctest --preset dev

test-sanitize: ## Build and run the unit tests with ASan and UBSan (build/asan-ubsan)
	@command -v cmake >/dev/null 2>&1 || { echo "install: brew install cmake"; exit 1; }
	@command -v ninja >/dev/null 2>&1 || { echo "install: brew install ninja"; exit 1; }
	cmake --preset asan-ubsan
	cmake --build --preset asan-ubsan
	ctest --preset asan-ubsan

test: test-unit test-sanitize ## L1 - unit tests, plain and sanitized

# TEST is a ctest regular expression (`ctest -R`) matched against the test
# names, which are the GoogleTest names <Suite>.<Test>.
# The usage check runs before the build, so a missing TEST fails at once.
test-one: ## Run the unit tests matching TEST=<regex> on the host build
	@test -n "$(TEST)" || { echo 'usage: make test-one TEST=<regex>, for example TEST=SensorBus'; exit 2; }
	@$(MAKE) --no-print-directory build
	ctest --preset dev -R '$(TEST)'

clean: ## Remove the build trees (build/) and nothing else
	rm -rf build

# test-install installs build/dev into a prefix under build/ and builds
# tests/consumer, a separate CMake project, against it the way a downstream
# project does, then runs the consumer program. The consumer searches only
# CMAKE_PREFIX_PATH (PHM_SENSOR_IO_CONSUMER_PREFIX_PATH_ONLY), so a copy
# installed elsewhere on the machine can neither make the check pass nor hide
# a failure. It then checks the two configure failures a consumer meets: no
# CMAKE_PREFIX_PATH, and a request for another major version. Not part of
# `test`: CI runs it as a step of its own.
INSTALL_TEST_DIR := build/install-test
INSTALL_TEST_PREFIX := $(CURDIR)/$(INSTALL_TEST_DIR)/prefix
CONSUMER_CONFIGURE := cmake -S tests/consumer -G Ninja -DPHM_SENSOR_IO_CONSUMER_PREFIX_PATH_ONLY=ON
PROJECT_VERSION_MAJOR = $(shell sed -n 's/^project.phm_sensor_io VERSION \([0-9][0-9]*\)\..*/\1/p' CMakeLists.txt)

test-install: build ## Install build/dev into a prefix under build/ and build tests/consumer against it
	@test -n "$(PROJECT_VERSION_MAJOR)" || { echo 'test-install: no project(phm_sensor_io VERSION ...) in CMakeLists.txt'; exit 1; }
	rm -rf $(INSTALL_TEST_DIR)
	cmake --install build/dev --prefix $(INSTALL_TEST_PREFIX)
	$(CONSUMER_CONFIGURE) -B $(INSTALL_TEST_DIR)/consumer \
		-DCMAKE_PREFIX_PATH=$(INSTALL_TEST_PREFIX) -DPHM_SENSOR_IO_CONSUMER_VERSION=$(PROJECT_VERSION_MAJOR)
	cmake --build $(INSTALL_TEST_DIR)/consumer
	$(INSTALL_TEST_DIR)/consumer/consumer
	@echo "test-install: expect find_package() to fail without CMAKE_PREFIX_PATH"
	@if $(CONSUMER_CONFIGURE) -B $(INSTALL_TEST_DIR)/no-prefix \
		> $(INSTALL_TEST_DIR)/no-prefix.log 2>&1; then \
		cat $(INSTALL_TEST_DIR)/no-prefix.log; echo "test-install: find_package() succeeded without CMAKE_PREFIX_PATH"; exit 1; fi
	@grep -F 'Could not find a package configuration file provided by "phm_sensor_io"' $(INSTALL_TEST_DIR)/no-prefix.log \
		|| { cat $(INSTALL_TEST_DIR)/no-prefix.log; exit 1; }
	@other_major=$$(( $(PROJECT_VERSION_MAJOR) + 1 )); \
	echo "test-install: expect find_package() to refuse major version $$other_major"; \
	if $(CONSUMER_CONFIGURE) -B $(INSTALL_TEST_DIR)/other-major \
		-DCMAKE_PREFIX_PATH=$(INSTALL_TEST_PREFIX) -DPHM_SENSOR_IO_CONSUMER_VERSION=$$other_major \
		> $(INSTALL_TEST_DIR)/other-major.log 2>&1; then \
		cat $(INSTALL_TEST_DIR)/other-major.log; echo "test-install: find_package() accepted major version $$other_major"; exit 1; fi; \
	grep -F "compatible with requested version \"$$other_major\"" $(INSTALL_TEST_DIR)/other-major.log \
		|| { cat $(INSTALL_TEST_DIR)/other-major.log; exit 1; }
	@echo "test-install: the installed package works and both failures are reported"

lint-cpp: configure ## Check C++ formatting (clang-format) and static analysis (clang-tidy)
	@test -n "$(CLANG_FORMAT)" || { echo '$(LLVM_HINT)'; exit 1; }
	@test -n "$(CLANG_TIDY)" || { echo '$(LLVM_HINT)'; exit 1; }
	$(CXX_SOURCES) | xargs -0 -r $(CLANG_FORMAT) --dry-run --Werror
	$(CXX_TRANSLATION_UNITS) | xargs -0 -r $(CLANG_TIDY) -p build/dev --quiet

verify: lint test ## L0+L1 - canonical local pre-PR gate

ci-pr: verify ## Alias of verify; what ci.yml runs

# The tools ci.yml installs before `make ci-pr`; maintenance.yml passes its own
# TOOLS. Kit tools go to the template's scripts/install-ci-tools.sh; the tools
# this repository adds (EXTRA_CI_TOOLS, pinned in scripts/tool-pins.extra) go to
# scripts/install-extra-tools.sh, so the kit script stays identical to upstream.
# The C++ compiler, clang-format and clang-tidy come from the runner image.
# With the default TOOLS, ci-tools also installs CI_SYSTEM_PACKAGES, the
# distribution packages the C++ build needs (ci-system-packages); a run that
# names its own TOOLS, such as maintenance.yml's, does not.
CI_TOOLS := markdownlint-cli2 yamllint actionlint gitleaks lychee syft cmake ninja
EXTRA_CI_TOOLS := syft cmake ninja
TOOLS ?= $(CI_TOOLS)

ci-tools: ## Install pinned CI tools (Linux x86_64; locally with ALLOW_LOCAL_INSTALL=1) - default CI_TOOLS, or TOOLS="lychee"
	@set -e; kit="$(filter-out $(EXTRA_CI_TOOLS),$(TOOLS))"; extra="$(filter $(EXTRA_CI_TOOLS),$(TOOLS))"; \
	if [ -n "$$kit" ]; then scripts/install-ci-tools.sh $$kit; fi; \
	if [ -n "$$extra" ]; then scripts/install-extra-tools.sh $$extra; fi
	@if [ "$(origin TOOLS)" = "file" ]; then $(MAKE) --no-print-directory ci-system-packages; fi

# The shared libmodbus the Modbus RTU transport links (dynamically, as
# libmodbus.so) and pkg-config, which CMake uses to find it. They come from the
# distribution at its version, with apt-get, as the compiler does; on another
# system install them with its package manager (CONTRIBUTING.md).
CI_SYSTEM_PACKAGES := libmodbus-dev pkg-config

ci-system-packages: ## Install the distribution packages the C++ build needs (apt-get; in CI or with ALLOW_LOCAL_INSTALL=1)
	@command -v apt-get >/dev/null 2>&1 || { echo "ci-system-packages: needs apt-get; install $(CI_SYSTEM_PACKAGES) with your package manager (CONTRIBUTING.md)"; exit 1; }
	@if [ -z "$$CI" ] && [ "$$ALLOW_LOCAL_INSTALL" != 1 ]; then \
		echo "ci-system-packages: refusing to run apt-get outside CI; set ALLOW_LOCAL_INSTALL=1 to install $(CI_SYSTEM_PACKAGES)"; exit 1; \
	fi
	@sudo_cmd=""; [ "$$(id -u)" -eq 0 ] || sudo_cmd="sudo"; \
	$$sudo_cmd apt-get update -qq && \
	$$sudo_cmd apt-get install -y -qq --no-install-recommends $(CI_SYSTEM_PACKAGES)

check-tool-versions: ## Compare CI tool pins against upstream (weekly maintenance; makes network calls)
	scripts/check-tool-versions.sh

maintenance: ## Everything the weekly maintenance workflow runs (network; not in verify)
	@rc=0; \
	$(MAKE) --no-print-directory lint-docs-external || rc=1; \
	$(MAKE) --no-print-directory check-tool-versions || rc=1; \
	exit $$rc

# --- Licence hygiene and SBOM -------------------------------------------------
# `lint-licenses` is part of `lint`; syft is pinned in scripts/tool-pins.extra
# and installed by `make ci-tools`.
# Both targets skip ./build: it holds build trees, including the test-only
# dependencies CMake downloads at configure time, none of which is delivered.
# Both targets switch off syft's two GitHub Actions catalogers: the actions a
# workflow or action.yml references are CI tooling, not delivered with the
# product, and carry no licence data. Do not `--exclude './.github/**'` instead:
# that also drops the npm dependencies of a local action under .github/actions/.
# Lock files such as pnpm-lock.yaml and uv.lock carry no licence data, so both
# targets let syft look licences up from the package registries (network).
# SBOM_IMAGES lists the container images that ship with the product (for
# example SBOM_IMAGES="ghcr.io/owner/api:1.2.3 nginx:1.30.5-alpine"). `sbom`
# writes one full SPDX file per image. `lint-licenses` also checks each image
# (without binary classifiers): language packages and Go modules against the
# allowlist; OS packages are the system layer, covered by the source offer, so
# only licences the policy rejects outright (AGPL, SSPL, ...) fail there.
# Output names carry a short hash of the image reference, so two references
# never map to the same file.
SYFT_ENV := SYFT_JAVASCRIPT_SEARCH_REMOTE_LICENSES=true SYFT_PYTHON_SEARCH_REMOTE_LICENSES=true SYFT_GOLANG_SEARCH_REMOTE_LICENSES=true
SYFT_CATALOGERS := --select-catalogers '-github-actions-usage-cataloger,-github-action-workflow-usage-cataloger'
SYFT_EXCLUDES := --exclude './build/**'
SYFT_IMAGE_CATALOGERS := --select-catalogers '-binary-classifier-cataloger,-elf-binary-package-cataloger,-pe-binary-package-cataloger,-linux-kernel-cataloger'
SBOM_IMAGES ?=

lint-licenses: ## Reject copyleft dependencies (GPL/AGPL/LGPL/SSPL/...)
	@command -v syft >/dev/null 2>&1 || { echo "install: brew install syft"; exit 1; }
	set -o pipefail; $(SYFT_ENV) syft dir:. -o json -q $(SYFT_CATALOGERS) $(SYFT_EXCLUDES) | python3 scripts/check-licenses.py
	@set -o pipefail; for img in $(SBOM_IMAGES); do \
		echo "image $$img"; \
		$(SYFT_ENV) syft "$$img" -o json -q $(SYFT_IMAGE_CATALOGERS) | python3 scripts/check-licenses.py --image || exit 1; \
	done

sbom: ## Write SPDX SBOM + readable third-party licence list to dist/ (repo and SBOM_IMAGES)
	@command -v syft >/dev/null 2>&1 || { echo "install: brew install syft"; exit 1; }
	@mkdir -p dist
	$(SYFT_ENV) syft dir:. -q $(SYFT_CATALOGERS) $(SYFT_EXCLUDES) -o spdx-json=dist/sbom.spdx.json -o template=dist/third-party-licences.txt -t scripts/licence-table.tmpl
	@echo "wrote dist/sbom.spdx.json and dist/third-party-licences.txt"
	@for img in $(SBOM_IMAGES); do \
		safe=$$(printf '%s' "$$img" | tr -c 'A-Za-z0-9.-' '_')-$$(printf '%s' "$$img" | python3 -c 'import hashlib,sys;print(hashlib.sha256(sys.stdin.buffer.read()).hexdigest()[:8])'); \
		$(SYFT_ENV) syft "$$img" -q -o spdx-json="dist/sbom-image-$$safe.spdx.json" -o template="dist/third-party-licences-image-$$safe.txt" -t scripts/licence-table.tmpl || exit 1; \
		echo "wrote dist/sbom-image-$$safe.spdx.json"; \
	done
