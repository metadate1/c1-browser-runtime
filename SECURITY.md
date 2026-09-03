# Security and privacy reports

Treat every disc image, NSD/NSF stream, virtual card record, and resume record as untrusted input.

After the repository becomes public, report vulnerabilities through GitHub's private vulnerability
reporting feature. Useful reports include:

- a malformed file crashes or escapes the parser;
- an input causes unexpectedly large memory, CPU, or storage use;
- game data, credentials, or local paths are disclosed; or
- the browser sends selected game data over the network.

Do not attach proprietary game data to a public issue. If a report needs a reproducer, begin with a
small synthetic file or explain how the maintainer can create one locally.

The current application has no telemetry, analytics, or game-file upload endpoint. Selected game
files remain local to the browser session. [PRIVACY.md](PRIVACY.md) describes the two small
progress records stored in `localStorage`.

These design choices and repository checks reduce risk; they do not guarantee that the project is
free of security defects.
