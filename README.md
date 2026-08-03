# micro

A small, fast, vim-inspired terminal text editor written in C (kilo-style).
No external dependencies — just a C compiler and a terminal.

![version](https://img.shields.io/badge/version-1.2-blue)

## Features

- **Vim-like modes** — `NORMAL`, `INSERT`, and `VISUAL` (character / line)
- **Multiple tabs** — up to 16; `Tab` opens/switches forward, `Alt+Tab` goes backward (rotates)
- **Custom keybindings** — fully configurable via `config.json` (JSON)
- **Color themes** — truecolor (24-bit) themes, default is the **opencode** palette
- **Syntax highlighting** — keywords, types, strings, numbers, comments, match highlights (C/C++ built in)
- **`~micro.<file>.rec` session files** — every keystroke is mirrored to a recovery file; saved on quit
- **`/` and `?` search**, `n` / `N` next/previous match
- **Line numbers**, current-line highlight, configurable tab width
- **Clipboard** — `yy` / `dd` yank & delete, `p` / `P` paste
- **Undo / redo** — `u` / `Ctrl-r` (each insert session or command is one undo step)
- **Word motions** — `w` `b` `e`, plus `^` (first non-blank), `gg` / `G` (first / last line)
- **Vim ex-commands** — `:w` `:q` `:q!` `:wq` `:tabnew` `:e <file>` `:<line>`
- **Blinking solid-block cursor**
- **Live resize** — handles `SIGWINCH` and redraws on terminal resize
- **`:` command line** — `:e <file>` opens/creates a file, `:<line>` jumps to a line

## Build

```sh
make
```

Produces a single binary at `build/micro`. Works on any POSIX system (Linux, macOS, BSD).

```sh
make clean   # remove the build directory
```

## Usage

```sh
./build/micro [file...]
```

The editor starts in `NORMAL` mode.

## Project structure

```
micro/
├── Makefile
├── config.json          # keybindings, themes, settings (loaded from cwd)
├── src/
│   ├── main.c           # globals, initEditor, main loop
│   ├── terminal.c       # raw mode, key reading, resize (SIGWINCH)
│   ├── buffer.c         # rows, editing, clipboard
│   ├── highlight.c      # syntax highlighting + language data
│   ├── fileio.c         # open / save / rec files / quit
│   ├── tabs.c           # multi-tab state (ownership transfer)
│   ├── find.c           # search (/ ? n N)
│   ├── input.c          # prompt, key processor, modes, visual, : commands
│   ├── screen.c         # rendering, append buffer, status bar
│   ├── config.c/.h      # JSON config loader + key mapping
│   └── micro.h          # shared types, globals, prototypes
└── build/               # generated binary (gitignored)
```

### Normal-mode keybindings

| Key | Action |
|---|---|
| `h` `j` `k` `l` | Move cursor |
| `w` `b` `e` | Word motions (next / back / end) |
| `^` | First non-blank character of line |
| `gg` / `G` | Go to first / last line |
| `i` / `a` | Insert before / after cursor |
| `I` / `A` | Insert at line start / end |
| `o` / `O` | Insert new line below / above |
| `0` / `$` | Go to line start / end |
| `x` | Delete character |
| `dd` / `yy` | Delete / copy line |
| `p` / `P` | Paste after / before |
| `u` / `Ctrl-r` | Undo / redo |
| `/` / `?` | Search forward / backward |
| `n` / `N` | Next / previous match |
| `:` | Command line (`:e`, `:w`, `:q`, `:wq`, `:q!`, `:tabnew`, `:<line>`) |
| `v` / `V` | Enter visual mode (char / line) |
| `Tab` | New tab / next tab |
| `Alt+Tab` | Previous tab |
| `Ctrl-s` | Save |
| `Ctrl-q` | Close current tab (last tab quits) |

In `INSERT` mode: `Esc` or `Ctrl-c` returns to `NORMAL`. `Tab` inserts a tab character.

Closing a tab with unsaved changes asks `Save before closing this tab? (y/n)`.

## Configuration

All keybindings, colors, and editor settings live in **`config.json`** (shipped at the
project root). The editor loads it from the current working directory and falls back
to sensible defaults if the file is missing.

```jsonc
{
  "keybindings": { "normal_mode": { "...": "..." }, "insert_mode": { "...": "..." } },
  "colors":      { "opencode": { "keyword1": "#9d7cd8", "..." }, "light": { "..." } },
  "settings":    { "tab_stop": 4, "theme": "opencode", "show_line_numbers": true, "..." }
}
```

### Themes

- The default theme is `opencode` (peach accent on near-black), matching the
  [opencode](https://opencode.ai) TUI palette.
- Add your own theme as a new block under `colors` and select it with `"theme": "<name>"`.
- Colors are truecolor hex strings (`#rrggbb`). Your terminal must support 24-bit color
  (`COLORTERM=truecolor`) for the full palette.

### Session / recovery files

While a file is open, the editor keeps a `~micro.<name>.rec` file next to the working
directory. Every change is written there. On save the content is read back from this
file, and the rec file is removed when the editor exits.

## Cleanup

```sh
make clean
```

## License

MIT
