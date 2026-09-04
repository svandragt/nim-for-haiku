# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

A viability spike: can Nim serve as a higher-level language for writing Haiku
(the BeOS-descendant OS) applications? Haiku's native API is C++ (the BeAPI:
`BApplication`, `BWindow`, `BView`, …). The bet is that Nim's C++ backend can
bind to it and give a nicer language on top.

This is exploratory, not a product. Prefer the smallest experiment that answers
the open question over building framework. Record findings in `JOURNAL.md`.

## The core technical bet

Nim compiles to C, C++, or JS. Haiku is a first-class Nim target (`--os:haiku`
has shipped for years, and the stdlib's POSIX layer works there). The GUI is the
hard part, and it drives every decision:

- The BeAPI is C++ with **subclassing as the primary idiom** — you subclass
  `BWindow`/`BView` and override `Draw`, `MessageReceived`, etc. Nim can't cleanly
  subclass a C++ class and override virtuals.
- So the interop model is: a **thin C++ shim** subclasses the BeAPI class and
  forwards each virtual to a Nim callback (function pointer / closure). Nim code
  stays idiomatic; the shim is the only C++.
- Compile GUI code with the **C++ backend** (`nim cpp`), not `nim c`, so
  `{.importcpp.}` / `{.emit.}` bindings and the shim link against `libbe`.

Prove it in this order — each step de-risks the next:
1. **CLI on Haiku** — a plain Nim program compiled and run on Haiku. Proves the
   toolchain end to end.
2. **Minimal GUI** — one `BWindow` via the C++ shim. Proves the interop model.
3. Only then generalise the binding pattern.

## Where compilation happens

Default: **compile on Haiku**. Nim is packaged for Haiku (`pkgman install nim`),
and Haiku already has gcc + BeAPI headers + `libbe`. Ship source, build there.
No cross-toolchain to babysit.

Cross-compiling from Linux (Nim emits C/C++ locally → a Haiku cross-gcc links
against a Haiku sysroot) is possible but a yak-shave. Defer it unless the SSH
round-trip becomes the real bottleneck.

Local devbox Nim's job is **fast checking**, not running: `nim check` /
`nim c` for the platform-independent parts on Linux, so you catch type and logic
errors without an SSH hop. GUI code that touches `libbe` only type-checks
locally — it links and runs on Haiku.

## Two working modes

- **Ship-and-run (default, Claude-managed):** edit locally → rsync to the Haiku
  host → `nim cpp -r` there → stream output back. Source of truth stays in git on
  Linux. Tightest loop that keeps history.
- **Code directly on Haiku:** run the editor/Claude on the Haiku box. Fewer
  moving parts, but weaker dev ergonomics. Use if remote iteration gets painful.

## Commands

```sh
devbox shell            # enter the env (nim + rsync)
devbox run check        # nim check the src/ tree locally (no linking)
./ship.sh hello         # rsync src/ to Haiku, build+run a CLI target (nim c)
./ship.sh <target> cpp  # same, C++ backend for BeAPI/libbe code
./test-gui.sh [out.png] # build+run the GUI app, screenshot it, pull the PNG back
```

The Haiku box is `Host haiku` in `~/.ssh/config` (R1beta6 VM, key auth via
`~/config/settings/ssh/authorized_keys` — see `JOURNAL.md`). Remote build dir is
`/boot/home/nim-haiku` (`HAIKU_DIR` / `HAIKU_HOST` override both scripts). Nim
2.2.8, gcc 13.3.0, and `libbe` are installed there.

Working GUI example: `src/gui.nim` + `src/gui_shim.cpp` — the reference for the
Nim↔BeAPI shim pattern.

## Conventions

- Nim source in `src/`. C++ shims alongside as `.cpp`/`.h`, pulled in with
  `{.compile.}` / `{.passL: "-lbe".}`.
- Every finding — what worked, what didn't, why — goes in `JOURNAL.md`, newest
  first. This is the actual deliverable of a viability spike.
