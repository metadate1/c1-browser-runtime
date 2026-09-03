# Engine source record

The engine snapshot comes from:

- [`wurlyfox/c1`](https://github.com/wurlyfox/c1) commit
  `256fdcef59f15a190290cc19db3fa9a707843b69`; and
- the `windows` branch of [`mateusfavarin/c1`](https://github.com/mateusfavarin/c1) at
  commit `408d6409afadc1202230ac1183d4d7f40292b87c`.

The Mateus branch has 67 commits after the Wurlyfox snapshot. They change layout, crash
handling, game behavior, collision, controller support, Windows support, and Clang
support. This repository did not import the bundled Windows libraries or binaries.

The project imported the engine snapshot as one commit. It did not preserve upstream
Git history. A blob comparison between initial commit
`e9ab8c72f4364b4b0192af10c14affaadda08d15` and the fixed Mateus revision found:

- 94 byte-identical engine files;
- no differences at paths that both trees contain;
- 10 additions specific to this repository or browser work; and
- 157 omitted upstream paths.

Most omitted paths contain desktop dependencies and Visual Studio files.

The named GitHub repositories list Wurly Fox, Mateus Favarin, and ManDude as
contributors. Their source history remains the controlling record.

Neither C1 repository has a stated root license for the engine. Public source does not
give an open-source license. The respective authors and rights holders keep their
rights.

Before you use or redistribute this source, read the project
[license notice](../LICENSE.md), [rights explanation](../RIGHTS_AND_LICENSES.md), and
[NOTICE](../NOTICE.md).
