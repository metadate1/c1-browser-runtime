# Privacy

C1 Browser Runtime is designed to use game files locally in the browser.

## Selected game files

When a user selects a disc image or extracted NSD/NSF streams, the browser reads those files for
the current session and copies recognized streams into the Wasm in-memory filesystem. The
application has no game-file upload endpoint and does not include telemetry, analytics,
advertising, or user accounts.

Selected game files are not written to browser persistence. They must be selected again after a
reload.

## Saved browser data

The runtime can store two small, versioned records in origin-scoped `localStorage`:

- `c1.virtual-memory-card.v1`, containing up to fifteen 128-byte virtual card payloads; and
- `c1.browser-resume.v1`, containing one 128-byte automatic-resume payload.

Invalid resume records may be quarantined under a timestamped key beginning with
`c1.browser-resume.v1.invalid.`. These records stay in that browser profile and origin until the
user clears the site's data.

Different hosts, ports, and protocols have separate browser storage. Clearing site data removes
the virtual card and automatic-resume records.

## Network boundary

The included local server serves static files from the selected build directory. The application
source contains no API client or upload service. A third party that deploys or modifies this code
can change that behavior; users should review the deployment they choose to trust.

If a future deployment adds a server feature, analytics, or another data flow, this document and
the application must be updated before that feature is published.

## Reports

Do not attach a disc image, BIOS, extracted stream, save, screenshot, recording, or other game data
to a public issue. Report suspected disclosure privately as described in
[SECURITY.md](SECURITY.md).
