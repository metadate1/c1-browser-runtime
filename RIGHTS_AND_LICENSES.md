# Rights and licenses

This page explains the repository's current publication boundary in plain language. It is not
legal advice.

## Current status

C1 Browser Runtime is source-available research software. It is not open source and has no
project-wide redistribution license. The maintainer's original contributions are covered by the
all-rights-reserved notice in [LICENSE.md](LICENSE.md). Imported and derived work remains subject
to the rights of its respective authors.

Public visibility does not grant a general right to copy, modify, redistribute, host, package, or
sell this repository. Do not describe it as MIT, Apache-2.0, GPL, public domain, or otherwise open
source.

## What is in the repository

Tracked files contain:

- C, JavaScript, HTML, and CSS source;
- tests, build scripts, and technical documentation;
- an imported C1 engine snapshot and later browser-runtime changes; and
- TinySoundFont and TinyMidiLoader headers under their separate notices.

Tracked files do **not** contain:

- a game disc image, ROM, BIOS, or retail executable;
- extracted NSD or NSF streams;
- retail textures, models, levels, music, dialogue, or sound effects;
- a retail screenshot, recording, replay, save, or memory-card file; or
- a link to download any of those files.

Users must supply their own supported game data. The browser reads selected files locally and the
project does not provide or upload them. This repository grants no right to obtain or use the
original game.

## C1 engine lineage

The engine is directly derived from `wurlyfox/c1` and `mateusfavarin/c1`. Exact source revisions
are listed in [engine/UPSTREAM.md](engine/UPSTREAM.md). Neither upstream repository has an express
root license at the recorded revisions or at the 2026-09-03 review.

A public GitHub repository is not automatically open source. Default copyright still applies.
This repository therefore does not claim to relicense the imported engine or to be a clean-room
implementation. Browser-specific changes do not settle the rights in the underlying source.

## Dependencies

Vendored third-party files and their notices are listed in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). Emscripten and SDL are build dependencies fetched
from their publishers; they are not committed here. Each dependency remains subject to its own
terms.

## Names and trademarks

*Crash Bandicoot*, PlayStation, Sony, Naughty Dog, and other third-party names and marks belong to
their owners. They are used only to identify compatibility and provenance.

This project is independent and unofficial. It is not affiliated with, authorized by, sponsored
by, or endorsed by any game, console, developer, or publisher rights holder.

## Source publication is not a binary release

Making the source repository publicly readable is separate from distributing a compiled native
program, Wasm bundle, hosted playable site, app package, or commercial product. The current source
publication plan excludes all of those forms of distribution.

Any future hosted site must preserve the user-supplied-file boundary unless the owner makes and
records a new decision after reviewing the additional rights, privacy, security, trademark, and
dependency obligations.

## What remains unresolved

The project has not established a redistribution or open-source licensing chain for the C1 engine.
These documents disclose that problem; they do not solve it or guarantee that publication is
lawful in every jurisdiction.

A broader license would require authority to license every covered contribution. Until that
authority exists, keep this repository under the restricted source-available boundary and do not
advertise broader permission.
