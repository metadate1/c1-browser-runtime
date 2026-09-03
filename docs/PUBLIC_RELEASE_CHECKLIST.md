# Public source-release checklist

Use this checklist before you change the GitHub repository from private to public.

This checklist covers source access only. It does not approve an open-source license,
hosted game, compiled release, application package, or commercial product.

## 1. Confirm the publication model

- [x] Call the project **source-available research software**, not open source.
- [x] Keep [LICENSE.md](../LICENSE.md) clear that there is no project-wide license.
- [x] Publish only source and documents.
- [x] Exclude `dist/`, local game data, screenshots, videos, replays, profiles, and save
  files.
- [x] Require users to select their own supported game data locally.
- [x] Do not provide or link to a game disc image, BIOS, extracted streams, or other
  game data.

## 2. Review the unresolved rights issue

- [x] Read [RIGHTS_AND_LICENSES.md](../RIGHTS_AND_LICENSES.md).
- [x] Read the factual [legal-review brief](LEGAL_REVIEW_BRIEF.md).
- [x] Confirm that [NOTICE.md](../NOTICE.md) and
  [engine/UPSTREAM.md](../engine/UPSTREAM.md) name the known source references.
- [x] Record the owner's chosen boundary in
  [PUBLICATION_DECISION.md](PUBLICATION_DECISION.md).
- [ ] Ask a qualified lawyer if the owner wants legal advice. Repository checks cannot
  give legal advice.

Do not add an open-source license unless the project has authority to license every
covered file.

## 3. Audit the files and Git history

```bash
git fetch --prune origin
bash scripts/check-public-release.sh --remote origin
```

- [x] The audit exits with status zero.
- [x] `engine/doc/memmap.xlsx` is the only binary blob and has the expected hash.
- [x] No reachable commit contains game data, an executable, generated Wasm, a save,
  capture, archive, secret, or other unexpected binary.
- [x] `git status --ignored` puts local data and generated output under ignored paths.
- [x] Every remote branch and tag is intentional.

## 4. Run the release checks

```bash
npm test
npm run build
git diff --check
```

- [x] Each command exits with status zero.
- [x] Generated files in `dist/` remain ignored and uncommitted.
- [x] Public documents describe current behavior and do not claim full retail parity.

## 5. Configure GitHub

- [x] Keep `main` as the default branch.
- [ ] Require the CI `verify` job before merge when the repository plan supports it.
- [ ] Enable secret scanning and push protection when GitHub makes them available.
- [ ] Enable private vulnerability reporting after the visibility change.
- [x] Keep GitHub Pages and release-package automation off until the project reviews
  hosted or binary distribution.
- [x] Use **source-available research runtime** in the repository description.
- [x] Do not select an open-source license in GitHub settings.
- [x] Send contribution requests to [CONTRIBUTING.md](../CONTRIBUTING.md).

## 6. Check the public repository

After the visibility change:

- [ ] Inspect the public file list and downloadable source archive.
- [ ] Clone the public repository into a new directory.
- [ ] Run `bash scripts/check-public-release.sh --remote origin` in the new clone.
- [ ] Confirm that GitHub does not label the project as MIT, Apache, GPL, or another
  open-source license.
- [ ] Confirm that the README links to the license, rights, notices, privacy, security,
  and contribution documents.

Treat a credible rights complaint or removal request as a release issue. Keep relevant
records. Handle the report in private instead of debating ownership in a public issue.

## Later changes to distribution

A hosted playable site, compiled release, package, or open-source license needs a new
decision. Before such a change:

- list the new files and data flows;
- review source, game, dependency, trademark, and privacy duties;
- update the rights, privacy, security, and contribution documents; and
- repeat the history and release audits.
