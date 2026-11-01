# Build the documentation

[Русская версия](README.ru.md) · [Developer documentation](../../docs/developer.md)

The public builder produces one HTML site with a user entry, EN/RU guides, C++ API and a separate
developer path. It uses CMake 3.25+ and Doxygen 1.18+; it does not need Python, a web framework,
a compiler or network access. No global settings are changed.

From the repository root:

```text
cmake -DHELP=ON -P tools/docs/Build.cmake
cmake -DDOXYGEN=/absolute/path/to/doxygen -DOUTPUT=/absolute/path/to/new-docs -P tools/docs/Build.cmake
```

On Windows, quote the whole argument when the executable path contains spaces, for example
`"-DDOXYGEN=C:/Program Files/doxygen/bin/doxygen.exe"`. Open `OUTPUT/html/index.html` in a browser.
The [main page](../../docs/index.md) is the same entry in source Markdown. Use the theme toggle to
switch between light and dark rendering. An older documentation directory is not modified when you
choose a fresh `OUTPUT`.

## Parameters and results

| Parameter | Default | Meaning |
| --- | --- | --- |
| `HELP=ON` | OFF | Print help and exit before path checks, discovery or file/process work |
| `SOURCE` | This checkout | Explicit source root containing `docs/index.md` |
| `OUTPUT` | `SOURCE/.work/docs` | Empty directory or a directory marked by this builder for the same source |
| `DOXYGEN` | Find `doxygen` on PATH | Native generator executable |

Exit 0 means successful generation or help. Exit 1 means invalid configuration, rejected reference,
staging or generator failure; CMake launcher failures retain their native status. Output contains
HTML/XML, `warnings.log`, generated `Doxyfile`, `sources.tsv`, `assets.tsv` and staged public inputs.
Existing owned generated files may be overwritten; the builder deletes no directories. Use a fresh
output when obsolete files from a previous generation must be absent.

## Reading path and source assets

The standard [Doxygen layout](layout.xml) selects the main navigation. Only pages reachable by explicit
local Markdown links from the two documentation landing pages are staged, together with public qd
headers and referenced regular assets. This keeps conformance reports available from the developer
path without putting an alphabetical report catalog on the user entry.

Doxygen leaves relative raw-file links unchanged when flattening Markdown pages. The builder stages
those links under `html/assets` while retaining repository-relative asset paths. It supports local inline links, Markdown/header reference definitions, single-backtick code spans and fenced code.
Raw-asset reference definitions, local images and nested raw-link labels fail explicitly; use an inline file link. Fenced example code
is preserved. Unsupported local syntax or missing files fail explicitly. Raw-file fragments such as
`#L20` are preserved as source hints; they are not claimed as verified HTML line anchors.

Default limits are `MAX_PAGES=1024`, `MAX_ASSETS=4096`, `MAX_FILE_BYTES=8388608` (8 MiB),
`MAX_ASSET_BYTES=67108864` and `MAX_SOURCE_BYTES=67108864` (64 MiB each). Override them with `-DNAME=value`;
each accepts an integer in 1..2147483647. Aggregate limits count raw asset bytes and Markdown/header input bytes
separately; generated HTML/XML may be larger. Private
working directories, path escape, symlink/reparse aliases and private-key files are rejected. The two
explicit public TLS test server keys are the only key-file exception. Directory links are limited to
the finite expectation/log directories in `AssetDirectories.cmake`; other links must name a file.

Run the [standalone builder checks](Tests.cmake) with the command documented there. Generator warnings
and local-link verification have different purposes; a zero generator exit alone does not prove that
every link or example works. Specifications and conformance captures remain tracked at their existing paths.

## Known generator behavior

Doxygen 1.18.0 retains an extra, empty `docs/index.md` file-reference entry in global search.
The site's Home links target the actual documentation landing page. The extra search entry remains
a known generator limitation.
