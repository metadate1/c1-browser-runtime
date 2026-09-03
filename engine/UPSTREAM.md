# Engine provenance

The engine snapshot is derived from:

- [`wurlyfox/c1`](https://github.com/wurlyfox/c1), upstream commit
  `256fdcef59f15a190290cc19db3fa9a707843b69`
- [`mateusfavarin/c1`](https://github.com/mateusfavarin/c1), `windows` branch commit
  `408d6409afadc1202230ac1183d4d7f40292b87c`

The Mateus branch contains 67 commits of layout, crash, gameplay, collision, controller, and Windows/Clang fixes beyond the Wurlyfox snapshot. Its bundled Windows libraries and binaries were deliberately not imported.

The snapshot was imported into this repository as a single commit. A blob-level comparison of
that initial commit (`e9ab8c72f4364b4b0192af10c14affaadda08d15`) with the Mateus revision found
94 byte-identical engine files, no differing files at shared paths, 10 browser-specific or
repository-specific additions, and 157 omitted upstream paths. The omitted paths are mainly
bundled desktop dependencies and Visual Studio files.

The original upstream commit history is not preserved here. GitHub's contributor records for the
named repositories identify Wurly Fox, Mateus Favarin, and ManDude; the upstream histories remain
authoritative.

Neither C1 repository provides an express root license for the engine code. Public availability
of those repositories does not grant an open-source license. Default copyright remains with the
respective authors and rights holders.

See the repository-level [license notice](../LICENSE.md),
[rights explanation](../RIGHTS_AND_LICENSES.md), and [NOTICE](../NOTICE.md) before relying on or
redistributing this source.
