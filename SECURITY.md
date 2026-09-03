# Security and privacy reports

Treat every disc image, NSD or NSF stream, virtual card record, and resume record as
untrusted input.

After the repository becomes public, use GitHub's private vulnerability reporting
feature for security reports. Useful reports include these cases:

- a malformed file crashes or escapes the parser;
- an input uses much more memory, CPU time, or storage than expected;
- the browser reveals game data, credentials, or local paths; or
- the browser sends selected game data over the network.

Do not attach proprietary game data to a public issue. If a report needs a test file,
start with a small synthetic file. You can also explain how the maintainer can create a
test file locally.

The current application has no telemetry, analytics, or game-file upload endpoint.
Selected game files stay in the local browser session. [PRIVACY.md](PRIVACY.md)
describes the two small progress records in `localStorage`.

These controls and repository checks reduce risk. They cannot prove that the project
has no security defects.
