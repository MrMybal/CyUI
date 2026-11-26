# CyUI version notes

[Français](VERSION.fr.md) · [README](README.md) · [Published releases](https://github.com/MrMybal/CyUI/releases)

## Versions and status

| Component | Version | Availability |
| --- | --- | --- |
| C++ library and Python package | 1.3.0 | Version of the C++ sources and the Python package built from this repository. |
| Standalone gallery demo | 1.2.0 | Published portable Windows x64 / DirectX 11 build. |
| Bundled CyDoc reader | 0.1.0 | Documentation dependency in current source builds, with image and gallery modules. |

**CyUI is in active development.** The project does not declare an Alpha, Beta
or Stable product stage. The library's version number and the published demo's
release flag do not establish a stable API or platform validation.

## Library 1.3.0 — changes since 1.2.0

- **Code and Diff.** The shared catalogue has 104 syntax modes, with native support for Verse, Zig (`.zig`) and ZON (`.zon`). The reader, editor and Diff use the same highlighting rules. Code views gain bundled JetBrains Mono, zoom controls and updated syntax colors. A new paged editor reads file ranges supplied by the host. The two columns of a split diff now scroll together.
- **Audio.** New views display LUFS/LRA loudness, true peak, correlation, EQ response, scrolling spectrograms and spatial traces with persistence. `drawCurveFill` fills curves and `envelope` displays ADSR, AHDSR and custom envelopes. Hosts can set colors, margins, frame appearance and phase positions. A handle can adjust decay and sustain together in one undoable host gesture.
- **Inspectors and graphs.** Hierarchy, DataView and FilePicker provide reusable views. The inspector supports custom fields, virtualized nested arrays, typed maps and sets. Graph clipboard operations preserve selected groups, links and reroute points.
- **Markdown and chat.** The native editor offers live, source, split and reading modes, with tables, attachments and support for `.md` and `.cymd` documents. The virtualized transcript can load messages on demand. Text selection and line wrapping at the scrollbar have been fixed. A standalone SDL3 demo shows these workflows.
- **Grids and office documents.** DataGrid, Spreadsheet and RichDocument add cell editing, spreadsheet formulas and paginated documents. OfficeIO reads and writes a subset of ODT, DOCX, ODS, XLSX, FODT and FODS in memory and reports recognized conversion losses.
- **Terminal and inspection tools.** The interactive VT terminal supports text selection and scrollback. The reference host connects it to a shell through ConPTY on Windows or a pseudo-terminal on Linux and macOS. New widgets include a paged hex viewer, image comparison, a gradient editor, grouped search results and task progress.
- **Documentation.** The English/French CyDoc manual indexes the public API and works offline. Current demo builds, Python and Unreal packages, and the bilingual static showcase include the manual.

### Other fixes

- Toggle switches can receive focus with Tab and activate with Space. Enter and Escape act on the topmost dialog when modals are nested.
- The Markdown editor preserves caret and selection positions around multibyte UTF-8 characters, including emoji, in source and split modes.
- On large grids, undo history for a cell edit stores only the changed cell’s data. Cross-sheet references now follow row and column insertions and sheet renames correctly.

## Published demo 1.2.0

The [v1.2.0 release](https://github.com/MrMybal/CyUI/releases/tag/v1.2.0) provides
the Windows gallery and its SHA-256 checksum. Its documented contents include:

- Native widgets, layouts, five themes and live English/French selection.
- Audio displays, plots, editable curves and timelines.
- Node graphs with synthetic image previews, a content browser and property inspector.
- Code editing and syntax modes, Diff views, commands and menu/panel filters.
- Dependency licenses and source revision information.

[Download the Windows x64 ZIP](https://github.com/MrMybal/CyUI/releases/download/v1.2.0/CyUI-Demo-1.2.0-windows-x64.zip)
and [its checksum](https://github.com/MrMybal/CyUI/releases/download/v1.2.0/CyUI-Demo-1.2.0-windows-x64.zip.sha256).
Extract the whole archive before running the executable.

## Documented limitations

- **Platforms:** the published executable is Windows x64 only. Linux/macOS library
  sources and CI configuration exist; local Windows checks do not validate
  execution on those systems. CEF deployment requires validation for each target.
- **Host responsibilities:** graphs and views display application-supplied data.
  Audio decoding, FFTs, DSP, shader execution, file operations, repository access
  and language services remain in the application.
- **Native code editor:** CodeEditor requires C++20. Syntax modes provide lexical
  highlighting, not a compiler, debugger or language server. The Python editor
  uses multiline editing with a syntax preview and does not expose the full
  native TextEditor backend.
- **Optional integrations:** the ordinary demo bundles no CEF or Unreal runtime.
  The Unreal connector targets Editor C++/Slate, not runtime/UMG. External SDKs,
  fonts and dependency notices remain the host's responsibility.
- **Documentation:** keep the entire `Documentation/` folder together. The
  reader uses JavaScript; saved preferences and personal annotations depend on
  browser storage permissions. External references and downloads need a connection.

See the [platforms guide](Documentation/Documentation.html#/en/overview/platforms),
[Web guide](Documentation/Documentation.html#/en/web) and
[Unreal guide](Documentation/Documentation.html#/en/unreal) for the documented scope.

## Bugs and feedback

Report bugs or discuss them in the [issue tracker](https://github.com/MrMybal/CyUI/issues).
