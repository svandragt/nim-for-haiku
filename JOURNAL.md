# Journal

Findings from the Nim-on-Haiku viability spike. Oldest first — read top to bottom.

## 2026-09-04 — Setup and approach

- Greenfield repo. Goal: can Nim be a higher-level language for Haiku apps?
- Decided the interop model: Nim's **C++ backend** binds the BeAPI; a thin C++
  shim subclasses `BWindow`/`BView` and forwards virtuals to Nim callbacks
  (Nim can't subclass a C++ class cleanly). See `CLAUDE.md`.
- Decided compilation happens **on Haiku** by default (Nim is packaged there via
  `pkgman`; gcc + `libbe` already present). Cross-compiling from Linux deferred.
- Local devbox toolchain up: Nim 2.2.10 + rsync. `devbox run check` runs
  `nim check` on `src/` for fast local type-checking (no linking to `libbe`).

### Proof steps (in order, each de-risks the next)
1. [x] CLI on Haiku — plain Nim program compiles + runs. Proves the toolchain.
2. [x] Minimal GUI — one `BWindow` via the C++ shim. Proves the interop model.
3. [x] Event direction (button → Nim callback) + generic pure-Nim API.
4. [x] Ship it: package app.nim as a native `.hpkg`, installed + in Deskbar.

## 2026-09-04 — Haiku VM on libvirt/KVM (working config)

Installing R1/beta6 under virt-manager. The default "closest equivalent" template
wires everything as virtio, which Haiku half-supports — cost real time. Working
device config:

- **Disk: SATA**, not virtio. virtio-blk disk is *invisible* to the installer
  (only the ATAPI CD showed up). Set `<target dev="sda" bus="sata"/>`.
- **Keyboard: VirtIO**, not PS/2. The auto-created PS/2 keyboard didn't work;
  deleting it and adding a VirtIO keyboard fixed input. (So virtio is fine for
  input, broken for the boot disk — don't blanket-switch either way.)
- **NIC: e1000** (Haiku-friendly; avoid virtio-net).

Install flow that worked once the disk was on SATA: DriveSetup → select raw disk
→ Partition → Format → **Intel Partition Map** (MBR, BIOS boot) → select empty
space → Create → **Be File System** → back to Installer → Onto: that partition.

## 2026-09-04 — SSH login rejected every password (root cause)

sshd advertised `password` auth but rejected every correct password for `user`,
even after `passwd` + reboot. **Cause: Haiku's default `user` account is UID 0
(it *is* root).** OpenSSH's default `PermitRootLogin prohibit-password` blocks
password login for UID 0 — so the password is checked against the root-login
policy, not just the credential, and refused.

Fix: edit `/system/settings/ssh/sshd_config`, set `PermitRootLogin yes`
(default is the commented `#PermitRootLogin prohibit-password`), then restart
sshd (`kill` it — launch_daemon respawns — or reboot).

For key auth, `PermitRootLogin prohibit-password` is fine *once the key is
installed*; the chicken-and-egg is that installing the key over ssh needs a
working login first. Authorized keys live at `~/config/settings/ssh/`
(see the next entry).

Refs: Haiku netservices guide; forum "SSH woes - Password rejected?".

## 2026-09-04 — Key auth ignored: Haiku overrides AuthorizedKeysFile

`ssh-copy-id` succeeded and perms were correct (`.ssh` 700, `authorized_keys`
600), yet sshd offered-and-refused the key. Cause: Haiku's shipped
`sshd_config` **overrides the default path** —

    AuthorizedKeysFile      config/settings/ssh/authorized_keys

so sshd reads `~/config/settings/ssh/authorized_keys`, *not* `~/.ssh/…` where
`ssh-copy-id` writes. Fix: copy the key there —

    mkdir -p ~/config/settings/ssh
    cp ~/.ssh/authorized_keys ~/config/settings/ssh/authorized_keys

No restart needed (read per-connection). `ssh-copy-id` is the wrong tool on
Haiku; a `ship.sh` bootstrap should target `~/config/settings/ssh/`.

## 2026-09-04 — Step 1 PROVEN: Nim CLI runs on Haiku ✅

Toolchain works end to end. `./ship.sh hello` → rsync `src/` → `nim c -r` on
Haiku → output back:

    hello from Nim 2.2.8 on haiku/amd64
    fib(20) = 6765  (ok)

Facts established on the box (R1~beta6, hrev59866, x86_64):
- `pkgman install nim` → **Nim 2.2.8** (local devbox is 2.2.10; fine).
- **gcc 13.3.0** present; `libbe.so` at `/boot/system/lib/`. C++ backend has what
  it needs for step 2.
- Nim's `hostOS` reports `haiku` — platform is first-class, no `--os` flag needed.
- rsync isn't preinstalled: `pkgman install rsync` on the box (needed both ends).
- `ship.sh` needs `--mkpath` (rsync won't create intermediate remote dirs).

Remote build dir: `/boot/home/nim-haiku` (override via `HAIKU_DIR`).

Next: step 2 — one `BWindow` via a C++ shim, built with `./ship.sh <t> cpp`.

## 2026-09-04 — Step 2 PROVEN: Nim GUI + BeAPI works ✅ (viability: yes)

`./test-gui.sh` builds and runs a Nim-owned GUI app that drives the native
BeAPI through a C++ shim. Clean output:

    hello from Nim 2.2.8 on haiku/amd64
    [shim] BScreen.Frame = 1024x768      # Haiku API called from the shim
    [nim] BScreen width came back as 1024 # value returned into Nim
    [nim] BeAPI round-trip ok             # doAssert passed

Screenshot confirms a real `BWindow` ("Nim on Haiku") with a `BStringView`,
rendered by app_server, `gui` in the Deskbar. GUI apps launch fine over SSH —
app_server is a systemwide service, not a per-session display.

**The interop model holds** (`src/gui_shim.cpp` + `src/gui.nim`):
- Nim owns `main`; the shim is the *only* C++, ~40 lines.
- The shim subclasses `BWindow` and overrides `QuitRequested` — the one thing
  Nim can't do — and exposes an `extern "C"` entry Nim calls via `{.importc.}`.
- Nim links `libbe` with `{.passL: "-lbe".}` and pulls the shim in with
  `{.compile.}`. Build with the **C++ backend** (`nim cpp`), not `nim c`.

Gotchas found:
- C++ `printf` is block-buffered to a pipe — `fflush(stdout)` or you lose it
  over SSH.
- Don't `kill` the app to end it: that aborts `app.Run()` before it returns and
  before trailing output flushes. Let it self-quit (a `BMessageRunner` posting
  `B_QUIT_REQUESTED`) so the run is clean and repeatable.
- `screenshot -s -f png <file>` for silent capture (returns exit 80 but writes
  the file fine).

Verdict: Nim-on-Haiku is viable. Next if continuing: bind a BeAPI callback
*back into Nim* (e.g. a `BButton` invocation → Nim closure) to prove the
event-handling direction, then decide whether to generalise the shim into a
reusable binding.

## 2026-09-04 — Step 3 PROVEN + pure-Nim API: apps need no C++ ✅

Two results:

1. **Event direction (BeAPI → Nim).** A `BButton` click routes through the shim
   into a Nim callback. Headless test (`./test-gui.sh`, synthetic clicks):

       [nim] button->Nim callback fired 3 time(s)

   Key design fix: buttons target `be_app`, so clicks are handled on the
   **application (main) thread** — the thread Nim started on — not a window's own
   thread. That makes Nim callbacks GC-safe (can `echo`, allocate, capture
   closures). Handling on a window thread would risk cross-thread GC.

2. **A reusable pure-Nim API.** `src/haiku_shim.cpp` is now a *generic,
   write-once* binding (`app_new`, `window_new`, `label_add/set`, `button_add`,
   `run`). `src/haiku.nim` wraps it in an idiomatic API. `src/app.nim` is a
   complete counter app in **100% Nim, zero C++**:

       let win = newWindow("Counter")
       let label = win.addLabel("Clicks: 0")
       win.addButton("Click me", proc() =
         inc clicks; label.text = "Clicks: " & $clicks)

   Compiles clean and renders natively (vertical layout via `BGroupLayout`,
   label + button stacked). This is the answer to "maintainable, high-level, and
   I don't know C++": the C++ is a fixed library seam; all app code is Nim.

Answers the viability question fully: **yes, and app authors write only Nim.**
Next: package `app.nim` as a shippable `.hpkg` (Haiku's format), so it installs
and appears in the Deskbar like any native app.

## 2026-09-04 — Step 4 PROVEN: shipped as a native .hpkg ✅

`./ship-pkg.sh` builds an installable Haiku package from the pure-Nim app:
release build → embed app signature + version resource (`packaging/counter.rdef`
via `rc`/`xres`/`mimeset`) → assemble a package tree (`apps/Counter` +
`.PackageInfo`) → `package create`. Output `dist/counter-1.0.0-1-x86_64.hpkg`
(~36 KB). Installed, it runs from `/boot/system/apps/Counter` and shows in the
Deskbar with signature `application/x-vnd.nim-counter`.

Packaging gotchas (cost real time):
- **`requires` must be resolvable in the target volume.** First tried
  `haiku >= r1~beta6_x86_64` — malformed (arch glued onto the version); nothing
  provides it, so activation is refused. Use `requires { haiku }`.
- **Install to the system volume, not `~/config/packages`.** A home-volume
  package resolves `requires haiku` awkwardly and silently fails to activate
  (`_PackagesEntryCreated` with no following `activated` in syslog). Dropping the
  `.hpkg` in `/boot/system/packages/` (same volume as `haiku`) activates it.
- **A failed activation pops a GUI "Package problems" dialog** on the desktop and
  **blocks the package daemon** until dismissed — over SSH this looks like every
  later `pkgman`/drop hanging. Screenshot the desktop when package ops stall.
- `.PackageInfo` goes in the tree root as `.PackageInfo`; `package create`
  packages the current directory.

All four steps done. Nim-on-Haiku: viable, pure-Nim app code, shippable package.

## 2026-09-04 — Bug: "Looper must be locked" on click, + update gotcha

First install crashed on the first real button click:
`Debugger call: 'Looper must be locked.'`. Root cause: clicks are handled on
the app thread (by design, for Nim GC safety), but the handler called `SetText`
on a `BStringView` owned by the *window's* looper. **BeAPI forbids touching a
view from another thread without locking that view's looper.** Fix: in
`haiku_label_set`, `Looper()->Lock()` / `Unlock()` around `SetText`.

The threading choice cuts both ways: routing events to the app thread makes the
Nim side safe (closures/GC) but means any view mutation must lock the window.

Lesson: the earlier headless test passed with a *different* callback shape
(`gui.nim`, a cdecl→global-int), so the closure path in `haiku.nim` was never
actually clicked before shipping. Exercise the real event — `hey <app-sig>
<4-char-what>` posts a button's message headlessly — before calling a GUI proven.

Update gotcha: **packagefs dedupes by version.** Re-dropping a same-named
`1.0.0-1` `.hpkg` with new content does nothing (old binary stays). Bump the
version (→ `1.0.0-2`) to force reactivation; `ship-pkg.sh` now derives the
filename from `PackageInfo` so it can't drift. Verified: installed v1.0.0-2
takes clicks with no crash.

## 2026-09-04 — First real app: a todo list (`src/todo.nim`)

The counter proved the mechanics; a todo list is the first app with *state that
grows at runtime* — the real test of whether the pure-Nim API is enough to build
against. It is. `todo.nim` has no C++ in it, only the two things the API was
missing: a text input and runtime row insertion.

Two small additions to the shim (`haiku_shim.cpp`), both write-once library
plumbing:

- **`BTextControl` input** — `addTextField` / `.text` / `.clear`. Reading it
  from the app thread needs the window-looper lock, same rule as
  `haiku_label_set`. Gave the input an explicit 240px min width; without it the
  group layout collapses the whole window to the input's tiny preferred size and
  clips every todo.
- **Runtime `addTodo`** — a native `BCheckBox` per row, `AddChild`ed *after*
  `Show()` under `Window()->Lock()`. A checkbox *is* the done state — Haiku owns
  the tick, so a todo row needs **zero** Nim callback. That deleted the whole
  message path I'd have written for "mark done".

Verified the real path, not a shortcut (heeding the earlier `hey` lesson):
`hey`'s view specifiers wouldn't resolve through the group layout, so instead
`clickButton(idx)` posts the button's own message to `be_app` — byte-for-byte a
click. Headless run logged `field reads: Milk, eggs, bread` then `added: Milk,
eggs, bread`, i.e. input read → handler → row inserted → field cleared, exactly
as a user would drive it. Screenshot confirms two rendered, tickable rows.

Test loop: `./test-todo.sh` (build + `--demo` + screenshot + quit). Non-self-
quitting app, so it uses `quit <app-sig>` rather than `gui.nim`'s autoDrive.

Verdict on the bet: **holds.** A stateful, interactive app dropped out in ~40
lines of Nim plus ~4 generic shim functions the next app reuses unchanged.

## 2026-09-04 — Window polish: panel background + resizable

Two fixes in the shared shim (`haiku_shim.cpp`), so the counter app gets them too.

**White background.** A `BWindow` doesn't paint — its top view is white. Fill it
with a `BView` set to `B_PANEL_BACKGROUND_COLOR` and hang the widgets off that.
Use `SetViewUIColor`, not `SetViewColor`: the UI variant tracks live
colour-scheme changes, so the app follows the system theme for free.

**Not resizable.** `B_AUTO_UPDATE_SIZE_LIMITS` pins the window's max size to the
layout's preferred size (min == max → no drag room), and re-clamps on every
layout pass — which also kept snapping the window to content width. Dropped the
flag, set explicit `SetSizeLimits(200, 100000, 120, 100000)`.

**Where the slack goes (the fiddly bit).** Enlarging the window spread the rows
apart evenly. The rows weren't stretching — they're fixed height (min == max,
confirmed by logging `MinSize`/`MaxSize`) — so a flat group `[input, Add, todo…,
glue]` distributes the surplus *between all items* when nothing can grow. A
single trailing glue with a big weight didn't win it back. What worked: **two
levels.** Widgets pack into a `content` view that sizes exactly to its rows; a
glue sits below it at the root. The root then sees only `[content, glue]`, so
surplus can land only in the glue. Rows stay top-anchored; the bottom gap grows.

Lesson for the binding: layout surplus distribution is the non-obvious trap —
"fixed-size children + one glue" is not enough on its own; isolate the growable
region in its own container.

Verified headless: `hey … set Frame of Window 0 to "BRect(…)"` enlarges the
window via scripting, then a screenshot confirms the packing.
