# Rights and licenses

This page explains the current publication limits in plain language. It is not legal
advice.

## Current status

C1 Browser Runtime is source-available research software. It is not open source and has
no project-wide redistribution license. [LICENSE.md](LICENSE.md) reserves the rights in
the maintainer's original work. Imported and derived work remains subject to the rights
of its authors.

Public access does not give general permission to copy, change, redistribute, host,
package, or sell this repository. Do not call it MIT, Apache-2.0, GPL, public domain, or
open source.

## Repository contents

Tracked files include:

- C, JavaScript, HTML, and CSS source;
- tests, build tools, and technical documents;
- an imported C1 engine snapshot and later browser runtime changes; and
- TinySoundFont and TinyMidiLoader headers with separate notices.

Tracked files do not include:

- a game disc image, ROM, BIOS, or retail executable;
- extracted NSD or NSF streams;
- retail textures, models, levels, music, speech, or sound effects;
- a retail screenshot, recording, replay, save, or memory-card file; or
- a link to download any such file.

Users must supply their own supported game data. The browser reads selected files
locally. The project does not provide or upload them. This repository gives no right to
obtain or use the original game.

## C1 engine source

The engine comes directly from `wurlyfox/c1` and `mateusfavarin/c1`.
[engine/UPSTREAM.md](engine/UPSTREAM.md) lists the exact revisions. At those revisions
and during the 2026-09-03 review, neither repository had a stated root license.

A public GitHub repository is not automatically open source. Normal copyright rules
still apply. This project does not claim to relicense the imported engine. It is also
not a clean-room implementation. Browser changes do not settle the rights in the
underlying source.

## Dependencies

[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) lists included third-party files and
their notices. The build fetches Emscripten and SDL from their publishers. They are not
committed here. Each dependency has its own terms.

## Names and trademarks

*Crash Bandicoot*, PlayStation, Sony, Naughty Dog, and other third-party names and marks
belong to their owners. This project uses the names only to state compatibility and
source history.

This project is independent and unofficial. No game, console, developer, or publisher
rights holder has authorized, sponsored, or endorsed it.

## Source access is not a binary release

Making this source repository public is different from distributing a compiled native
program, Wasm bundle, hosted playable site, application package, or commercial product.
The current plan does not include any of those forms of distribution.

A future hosted site must keep the user-supplied-file boundary unless the owner records
a new decision. The owner must first review the added rights, privacy, security,
trademark, and dependency duties.

## Unresolved issue

The project has not proved a redistribution or open-source license chain for the C1
engine. These documents disclose that issue. They do not solve it or promise that
publication is lawful in every place.

A broader license requires authority to license every covered contribution. Until the
project has that authority, keep this repository within the restricted source-available
boundary. Do not claim broader permission.
