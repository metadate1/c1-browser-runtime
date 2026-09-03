# Public source-release checklist

Use this checklist before changing the GitHub repository from private to public.

This checklist covers source publication only. It does not approve an open-source license, hosted
game, compiled release, app package, or commercial product.

## 1. Confirm the publication model

- [x] Describe the project as **source-available research software**, not open source.
- [x] Keep [LICENSE.md](../LICENSE.md) explicit that there is no project-wide license.
- [x] Publish source and documentation only.
- [x] Exclude `dist/`, local game data, screenshots, videos, replays, profiles, and save files.
- [x] Keep the user-supplied-file model: users select their own supported game data locally.
- [x] Do not provide or link to a game disc image, BIOS, extracted streams, or other game data.

## 2. Review the unresolved rights risk

- [x] Read [RIGHTS_AND_LICENSES.md](../RIGHTS_AND_LICENSES.md).
- [x] Read the factual [legal-review brief](LEGAL_REVIEW_BRIEF.md).
- [x] Confirm that [NOTICE.md](../NOTICE.md) and
  [engine/UPSTREAM.md](../engine/UPSTREAM.md) identify known source references.
- [x] The owner's chosen boundary is recorded in
  [PUBLICATION_DECISION.md](PUBLICATION_DECISION.md).
- [ ] Obtain advice from a qualified lawyer if the owner wants a legal opinion. Repository checks
  cannot provide one.

Do not add an open-source license unless the project has authority to license every covered file.

## 3. Audit files and Git history

```bash
git fetch --prune origin
bash scripts/check-public-release.sh --remote origin
```

- [x] The audit exits with status zero.
- [x] `engine/doc/memmap.xlsx` is the only binary blob and matches its pinned hash.
- [x] No reachable commit contains game data, executables, generated Wasm, saves, captures,
  archives, secrets, or another unexpected binary.
- [x] `git status --ignored` places local data and generated output under ignored paths.
- [x] Every remote branch and tag is intentional.

## 4. Run the release checks

```bash
npm test
npm run build
git diff --check
```

- [x] Every command exits with status zero.
- [x] Generated `dist/` output remains ignored and uncommitted.
- [x] Public documentation describes current behavior and does not overstate retail parity.

## 5. Configure GitHub

- [x] Keep `main` as the default branch.
- [ ] Require the CI `verify` job before merge when the repository plan supports it.
- [ ] Enable secret scanning and push protection when available.
- [ ] Enable private vulnerability reporting after visibility changes.
- [x] Keep GitHub Pages and release-package automation disabled until hosted or binary
  distribution is reviewed separately.
- [x] Use **source-available research runtime** in the repository description.
- [x] Do not select an open-source license in GitHub settings.
- [x] Point contribution requests to [CONTRIBUTING.md](../CONTRIBUTING.md).

## 6. Check the public repository

After visibility changes:

- [ ] Inspect the public file list and downloadable source archive.
- [ ] Clone the public repository into a new directory.
- [ ] Run `bash scripts/check-public-release.sh --remote origin` from that fresh clone.
- [ ] Confirm that GitHub does not label the project MIT, Apache, GPL, or another open-source
  license.
- [ ] Confirm that the README links to the license, rights, notices, privacy, security, and
  contribution documents.

Treat a credible rights complaint or takedown request as a release issue. Preserve the relevant
records and handle it privately; do not debate ownership in a public issue thread.

## Later: changing the distribution model

A hosted playable site, compiled release, package, or open-source license is a new decision. Before
making that change:

- identify the new files and data flows;
- review upstream, game, dependency, trademark, and privacy obligations;
- update the rights, privacy, security, and contribution documents; and
- repeat the history and release audits.
