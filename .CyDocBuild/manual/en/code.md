---
id: code
title: C++ code syntax and extensions
description: C++ code syntax and extensions for CyUI integrators.
section: Guides
parent: modules
order: 30
revision: 1
---

Link `CyUI::Code` (C++17) for the catalogue, lexer service and read-only views;
`CyUI::CodeEditor` (C++20) adds the common editable widget. Core-only builds do
not compile or link any lexer. CMake resolves dependencies automatically and
exports `CYUI_CODE_LANGUAGE_CATALOGUE=1`. No network fetch or Python is needed
to build the committed catalogue. Exactly one compatible ImGui instance is used.

## Built-in modes {#built-in-modes}

`codeLanguages()` returns 102 owned metadata snapshots. The canonical list,
file associations and original shader profiles are in `data/code-languages.json`.
It covers the Notepad++ syntax-mode list used by the host migration, plus HLSL,
GLSL, Markdown, TSX and an Unreal C++ profile. Each mode has its own fixture.
NFO intentionally displays as plain text: decoding OEM code pages belongs to the
host. Internal Search uses a diagnostic-list lexer; it is not a search engine.
Shared engines do not include Notepad++'s UI, plugin ABI or configuration format.

`codeLanguageFromName(idOrName)` accepts catalogue IDs/display names and a few
aliases; unknown names return plain. `codeLanguageForPath(path)` checks exact
filenames, then case-insensitive extensions. A host may add project context
(Unreal headers are ambiguous without it) or let the user select a mode manually.
Plugin associations take precedence; removing a definition restores built-ins.
Ambiguous associations between plugins are deterministic by stable numeric ID.

## Add a syntax from an application plugin {#add-a-syntax-from-an-application-plugin}

```cpp
#include <cyui/modules/Code.h>
cyui::CodeLanguageDefinition syntax;
syntax.id = "my-plugin/config";  // stable, namespaced ID
syntax.name = "My configuration";
syntax.extensions = {"mycfg"}; // no leading dot
cyui::CodeSyntaxRules rules;
rules.keywords = {"option", "section"};
rules.types = {"number", "vector"};
rules.lineComment = "#";
rules.blockCommentStart = "/*";
rules.blockCommentEnd = "*/";
syntax.rules = rules;
const auto language = cyui::registerCodeLanguage(syntax);
// view.setText(source, language, documentRevision);
// editor.setLanguage(language);
// On plugin removal, after stopping calls and before unloading its code:
cyui::unregisterCodeLanguage(syntax.id);
```

Alternatively set `base` to a built-in language and `wordLists` (0-based Lexilla
sets) and optional lexer `properties`. Empty word-list entries inherit the base.
Rules support one line-comment delimiter, one block pair, quote characters,
escaping and case-sensitive/insensitive word sets. They are deliberately simpler
than a grammar. For nested constructs, embedded languages or a custom parser,
provide `highlight(std::string_view) -> std::vector<CodeToken>` instead. Callback
results take precedence over rules; rules take precedence over the base engine.

Tokens use raw UTF-8 byte offsets, never display columns, UTF-16 positions or tab
expansion. Ranges must be nonempty, sorted, nonoverlapping, within the snapshot,
and start/end on code-point boundaries. Gaps remain neutral. Unknown roles and
invalid spans throw before replacing a prepared CodeDocument. Themes map roles
through palette colors; plugins do not supply literal UI colors. Bounds apply to
definition sizes and 32 MiB source snapshots. Trusted plugin code still needs
host error handling and time limits where its architecture permits them.

Registration updates an existing plugin ID without changing its numeric value.
Built-in values 0..13 are preserved; dynamic values use the high bit. Do not
persist dynamic numbers: persist the string ID. Built-in IDs cannot be replaced;
hash collisions are rejected. Registry snapshots are synchronized, and callbacks
run outside its mutex. Callback capture/library lifetime belongs to the host;
unregister does not wait for already running callbacks. Stop/join those calls
before unloading a library or destroying captured services.

## Caching, editing and limits {#caching-editing-and-limits}

CodeDocument caches document and registry revisions. CodeEditor refreshes syntax
only when the native text revision or registry changes. Lexilla currently lexes
the complete bounded snapshot after an edit; this is not an incremental parser.
A glyph edit made during rendering receives updated shared colors on the next
frame. `refreshSyntax()` prepares explicitly, including in headless tests.
Syntax preparation never changes text bytes, cursor state or Undo history.
`setSyntaxEnabled(false)` lets hosts retain richer existing lexical/semantic
adapters on the same native editor without a second widget implementation.

This is lexical coloring, not language-server completion, refactoring or compiler
semantics. Legacy Python modules keep their existing implementation; the new
native registry/callback API described here is C++ only. A Python native binding
is not supplied by this change.

## Bundled dependencies {#bundled-dependencies}

The exact Lexilla, Scintilla-header and SciTE-property revisions and archive hashes are recorded in `third_party/code-syntax.lock.json`. The committed catalogue requires no network or Python to build. Only the selected Lexilla engines and support library compile; the Scintilla editing UI is excluded. Retain all three licenses and THIRD_PARTY_NOTICES.md.

## Log files {#log-files}

`CodeLanguage::log` is selected automatically for `.log` / `.logs` files. It
colors the complete line using semantic palette roles, with normal INFO and
untagged lines left in the default text color:

| Level word (case-insensitive) | Role / default appearance |
| --- | --- |
| Error, Fatal, Critical, Panic, Exception | error / red |
| Warning, Warn | warning / orange |
| Debug, Trace, Verbose, VeryVerbose | comment / subdued |
| Everything else, including Info | text / normal |

Matching uses complete words, so ErrorCode, WarningCount and traceback are not
severity markers. Error takes priority over Warning, then debug verbosity. This
supports bracketed levels, Unreal `LogTemp: Error:`, JSON level fields and common
timestamp/category prefixes. ANSI CSI sequences are skipped when detecting level
words; source bytes remain intact. Wrapped visual rows inherit the same color.
Stack-trace continuation lines without a level remain normal; inferred multiline
events and stripping escape sequences belong to a dedicated log-viewing host.
Plugins can inherit `base = CodeLanguage::log` for other extensions, or supply a
callback for a logger with different level names or classification rules.

## Verse {#verse}

`CodeLanguage::verse` / `verse` selects the original native Verse scanner;
`.verse` matching is case-insensitive, including `*.digest.verse`. No UEFN SDK,
new Lexilla engine or language server is linked. CodeEditor and CodeDocument use
the same tokens. Keywords, built-in types, effect/access specifiers, attributes,
call names, decimal/hexadecimal/exponent numbers, strings and characters have
semantic palette roles. `#` comments, nested `<# ... #>` comments and `<#>`
indented comments preserve their state across lines. Double-quoted interpolation
colors expressions, including nested braces, strings and comments; escaped braces
stay string text. Unterminated comments/strings remain colored through EOF.
Interpolation/string nesting is bounded to 63 frames; a deeper remainder is
displayed as string text. Syntax preparation never edits source or Undo history.

Plugins can inherit `base = CodeLanguage::verse`; word sets 0/1/2 replace keyword,
type and function families. Empty entries inherit defaults. Rules/callbacks still
take precedence; associations still restore after plugin removal.
This is lexical support, not Verse compilation, UEFN completion or semantic analysis.

References: [Book of Verse](https://verselang.github.io/book/00_overview/) and
[Epic's Verse reference](https://dev.epicgames.com/documentation/en-us/fortnite/verse-language-reference).

```image
{"id": "code-view", "src": "assets/code.webp", "alt": "Source view and shared syntax", "controls": "minimal"}
```
