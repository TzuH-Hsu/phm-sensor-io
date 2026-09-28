# Template drift

This file lists every place this repository deliberately differs from the
github-project-os template, measured against template v0.5.7 on 2026-09-28 (re-baselined from v0.5.6 when v0.5.7 was taken).
On a sync, keep everything below; update this file in the same PR as any new
deliberate difference.

The comparison ref is not a normal tag. Fetch it once into its own namespace, so
template tags never collide with this repository's version tags:

```bash
git remote add template https://github.com/TzuH-Hsu/github-project-os.git  # once
git config remote.template.tagOpt --no-tags
git fetch template '+refs/tags/v0.5.7:refs/template-tags/v0.5.7'
```

To re-check:

```bash
git ls-tree -r --name-only refs/template-tags/v0.5.7 > /tmp/template-files.txt
git ls-tree -r --name-only HEAD > /tmp/repo-files.txt
diff /tmp/template-files.txt /tmp/repo-files.txt
```

```bash
git diff refs/template-tags/v0.5.7 HEAD -- <file>
```

## Adopter-owned files

Files every adopter owns; they always differ from the template and are never
copied from it.

| File | What is ours |
| --- | --- |
| README.md | Project description, badges, scope table, getting-started |
| CHANGELOG.md | This repository's own release-please history |
| LICENSE | Apache-2.0 text (template ships MIT) |
| NOTICE | Apache-2.0 attribution plus MIT notice for the template-derived scaffolding |
| .release-please-manifest.json | This repository's own version counter |
| AGENTS.md — "Repository policy" section | Public-library content rules (no deployment detail, licence allowlist, SPDX header rule) |
| .github/CODEOWNERS | Repository-specific comment set (no active owner lines) |
| .github/labels.yml — area:\* set | `area:driver`, `area:docs`, `area:ci` instead of the template's starter domains |
| .github/ISSUE_TEMPLATE/\*.yml — Area options | Match this repo's area:\* set above |
| .github/PROJECT_FIELDS.md — Area row | Points at this repo's own `.github/labels.yml` domains |
| docs/adr/README.md — index | Lists only the ADRs this repository has (0001–0003, 0007, 0008) |
| docs/adr/ADR-0008 — Date/Issue | Ported issue number, not the upstream one |

## Template files not taken

| File | Why |
| --- | --- |
| docs/adr/ADR-0004-adopter-licence-choice.md | Not copied; linked by URL from docs/setup/licensing.md |
| docs/adr/ADR-0005-runner-selection-variable.md | Not copied; linked by URL from docs/setup/runners.md |
| docs/adr/ADR-0006-coarse-type-fallback.md | Not copied; personal-account decision is described in .github/PROJECT_FIELDS.md instead |
| docs/template/README.starter.md | Removed at de-template (bootstrap phase 8 here) |
| docs/template/architecture.md | Removed at de-template |
| docs/template/design-principles.md | Removed at de-template |
| docs/template/upgrading.md | Removed at de-template |
| scripts/check-license-marker.sh | Not taken; superseded by this repo's own `scripts/check-licenses.py` |

## Kit files with local changes

| File | Difference from the template | Why | Since |
| --- | --- | --- | --- |
| .github/workflows/ci.yml, issue-labeler.yml, maintenance.yml, release-please.yml | `runs-on` reads one repository variable per workflow (`CI_RUNNER_LABELS`, `AUTOMATION_RUNNER_LABELS`, `MAINTENANCE_RUNNER_LABELS`, `RELEASE_RUNNER_LABELS`) instead of the template's single `RUNNER_LABELS` | A lint job and a release job can be pointed at different runners; documented in docs/setup/runners.md | bootstrap |
| .github/workflows/ci.yml | `runs-on` default kept at `ubuntu-latest` with a note that a smaller runner measured 1.5–6x slower on this job | Measured, not assumed — keep the full runner | 76cacb2 (#15) |
| .github/labels.yml | `type:bug` / `type:feature` labels shipped active (not commented out) | This repository is a personal account, where native issue types cannot be applied (see .github/PROJECT_FIELDS.md, "Personal accounts") | bootstrap |
| .github/PROJECT_FIELDS.md | "Personal accounts" section states the verified GraphQL result directly instead of describing two resolutions to pick from | The `null` case was already confirmed for this repository (2026-09-15) | bb37223 (#18) |
| AGENTS.md | Build-and-validation paragraph lists four per-workflow runner variables instead of one `RUNNER_LABELS` | Matches the workflow files' actual variable names | bb37223 (#18) |
| skills/github-actions-hygiene/SKILL.md | Rule 1 names the four per-workflow runner variables | Matches the workflow files' actual variable names | bb37223 (#18) |
| docs/setup/runners.md | Documents four per-workflow variables and a comparison table instead of one `RUNNER_LABELS` | Matches the workflow files' actual variable names | bb37223 (#18) |
| scripts/install-ci-tools.sh | Comment says "`*_RUNNER_LABELS` variables" (plural) instead of `RUNNER_LABELS` | Matches the workflow files' actual variable names | bb37223 (#18) |
| docs/setup/licensing.md | Adds an adopter note at the top: this repo's `scripts/bootstrap.sh` has no phase 9 (it predates that phase), states the Apache-2.0 choice, and links upstream ADR-0004 by URL | This repo bootstrapped before the licence-choice phase existed upstream | bb37223 (#18) |
| docs/setup/bootstrap.md | Phase numbers read 7/8 instead of 8/9/10 | This repo's bootstrap.sh has fewer phases than the current template (no coarse-Type or licence-choice phase; see below) | ed98cc2 (#13) |
| scripts/bootstrap.sh | Only the phase-5 repository-settings block (squash-only, `squash_merge_commit_title=PR_TITLE`, `squash_merge_commit_message=PR_BODY`, `allow_update_branch=true`) was taken from the current template; the rest of the script is older and has no phases 6 or 9 | Phase 2's REST-based issue-type probe misreports on personal accounts, and phases 6/9 (repo security settings, licence choice) postdate this repo's bootstrap; only the phase-5 fix was worth porting | bb37223 (#18) |
| SECURITY.md | "Repository security settings" section attributes the four-protection check to "the upstream template's `scripts/bootstrap.sh`" and adds a manual `gh api ... .security_and_analysis` check | This repo's own bootstrap.sh predates that phase | bb37223 (#18) |
| skills/anti-patterns/SKILL.md | "Inherited-licence leak" row describes this repo's own NOTICE history instead of citing bootstrap phase 9 / ADR-0004 | Same reason — no phase 9 here | bb37223 (#18) |
| skills/labels-and-taxonomy/SKILL.md | Notes that this repository's (older) phase 2 does not flag the coarse-Type-fallback-on-an-organization-repo mistake | Same reason — phase 2 here predates the GraphQL probe | bb37223 (#18) |
| skills/issue-writing/SKILL.md (~line 67) | The `### Area` example is a generic placeholder instead of a concrete `area:docs` / `area:ci` checkbox pair | Same placeholder in all six sibling repos: four of them have no `area:docs`/`area:ci`, so the template v0.5.2 sync replaced the example everywhere to keep one patch applicable to all six | #18 |
| Makefile | `check` no longer calls `scripts/check-license-marker.sh` (removed) and instead runs `scripts/test_check_licenses.py` under `python3 -m unittest` when present | check-license-marker.sh was not taken (see above); the licence allowlist has its own test | 9d49270 (#39) |
| .gitignore | Adds `dist/` (SBOM output) | `make sbom` writes SPDX + licence-list files there | bootstrap |
| CONTRIBUTING.md | Tool-install table reorders two rows and drops a trailing comma | Editorial only, no functional difference | 76cacb2 (#15) |

## Local additions in template directories

| File or target | Purpose | Since |
| --- | --- | --- |
| scripts/check-licenses.py | Enforces the dependency licence allowlist from a syft JSON SBOM on stdin | bootstrap |
| scripts/licence-exceptions.json | Named per-component exceptions to the licence allowlist, each with a reason | 9d49270 (#39) |
| scripts/licence-table.tmpl | syft output template producing a name/version/type/licence table | 73653b8 (#41) |
| scripts/test_check_licenses.py | Unit tests for check-licenses.py, run by `make check` | 9d49270 (#39) |
| Makefile: `lint-licenses` target | Runs syft + check-licenses.py against the repository and each image in `SBOM_IMAGES` | fcd885a (#23) |
| Makefile: `sbom` target | Writes an SPDX SBOM and a readable third-party licence list to `dist/`, for the repository and each image in `SBOM_IMAGES` | 73653b8 (#41) |

Neither target is wired into `lint` or `ci-pr` yet: there are no dependencies
to scan, and syft is not in `scripts/install-ci-tools.sh`'s pinned tool list.
| docs/template-drift.md | This inventory of deliberate differences from the template | #43 |

## Local rules that conflict with kit files

| Rule | Where it is stated | Kit files affected | Why not patched locally |
| --- | --- | --- | --- |
| Every source file starts with `SPDX-License-Identifier: Apache-2.0` | AGENTS.md, "Repository policy" | Kit scripts under `scripts/` (from template v0.5.7 they carry `SPDX-License-Identifier: MIT`) | Resolved upstream in v0.5.7: the kit scripts are MIT-licensed template scaffolding (see NOTICE), so an MIT identifier is the correct header and they stay byte-identical. The rule's wording names Apache-2.0 because it is written for this library's own source |

## Updating this file

- Add a row in the same PR that introduces a deliberate difference from the template.
- Remove the row when the difference is dropped (the file goes back to matching the template).
- Re-baseline the tag (and the date in the intro line) when a new template release is taken.
