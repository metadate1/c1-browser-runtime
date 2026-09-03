# Privacy

C1 Browser Runtime uses game files locally in your browser.

## Game files that you select

When you select a disc image or NSD and NSF streams, the browser reads them for the
current session. It copies known streams into the Wasm in-memory file system. The
application has no game-file upload endpoint. It also has no telemetry, analytics,
advertising, or user accounts.

The browser does not save selected game files. You must select them again after a page
reload.

## Progress saved by the browser

The runtime can save two small, versioned records in origin-specific `localStorage`:

- `c1.virtual-memory-card.v1` can contain up to fifteen 128-byte virtual memory-card
  payloads.
- `c1.browser-resume.v1` can contain one 128-byte automatic-resume payload.

If a resume record is invalid, the runtime can quarantine it under a timestamped key
that starts with `c1.browser-resume.v1.invalid.`. These records stay in the current
browser profile and origin until you clear the site's data.

Different hosts, ports, and protocols have separate browser storage. Clearing site
data removes both the virtual card and automatic resume records.

## Network boundary

The included local server only serves static files from the build directory that you
select. The application source has no API client or upload service.

A third party can change this behavior in a modified build or deployment. Review the
deployment before you trust it. If a future version adds a server feature, analytics,
or another data flow, the project must update this document before release.

## Reports

Do not attach a disc image, BIOS, extracted stream, save, screenshot, recording, or
other game data to a public issue. Use the private process in
[SECURITY.md](SECURITY.md) to report a possible data leak.
