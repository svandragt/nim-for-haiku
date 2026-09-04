# Journal

Findings from the Nim-on-Haiku viability spike. Newest first.

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
working login first. Authorized keys live at `/boot/home/.ssh/authorized_keys`
(also reachable as `~/config/settings/ssh/`? — the docs point at `~/.ssh`).
Diagnosis refs below.

Refs: Haiku netservices guide; forum "SSH woes - Password rejected?".

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

## 2026-09-04 — Haiku VM on libvirt/KVM (working config)

Installing R1/beta6 under virt-manager. The default "closest equivalent" template
wires everything as virtio, which Haiku half-supports — cost real time. Working
device config:

- **Disk: SATA**, not virtio. virtio-blk disk is *invisible* to the installer
  (only the ATAPI CD showed up). Set `<target dev="sda" bus="sata"/>`.
- **Keyboard: VirtIO**, not PS/2. The auto-created PS/2 keyboard didn't work;
  deleting it and adding a VirtIO keyboard fixed input. (So virtio is fine for
  input, broken for the boot disk — don't blanket-switch either way.)
- NIC: TBD — e1000 or rtl8139 expected to be the safe choice (avoid virtio-net).

Install flow that worked once the disk was on SATA: DriveSetup → select raw disk
→ Partition → Format → **Intel Partition Map** (MBR, BIOS boot) → select empty
space → Create → **Be File System** → back to Installer → Onto: that partition.

## 2026-09-04 — Setup and approach

- Greenfield repo. Goal: can Nim be a higher-level language for Haiku apps?
- Decided the interop model: Nim's **C++ backend** binds the BeAPI; a thin C++
  shim subclasses `BWindow`/`BView` and forwards virtuals to Nim callbacks
  (Nim can't subclass a C++ class cleanly). See `CLAUDE.md`.
- Decided compilation happens **on Haiku** by default (Nim is packaged there via
  `pkgman`; gcc + `libbe` already present). Cross-compiling from Linux deferred.
- Local devbox toolchain up: Nim 2.2.10 + rsync. `devbox run check` runs
  `nim check` on `src/` for fast local type-checking (no linking to `libbe`).
- **Open, not yet answered:** no Haiku host configured. Next: bring up Haiku over
  SSH, add it as `Host haiku` in `~/.ssh/config`, confirm `pkgman install nim`,
  then step 1 — compile and run a plain Nim CLI program on Haiku.

### Proof steps (in order, each de-risks the next)
1. [x] CLI on Haiku — plain Nim program compiles + runs. Proves the toolchain.
2. [x] Minimal GUI — one `BWindow` via the C++ shim. Proves the interop model.
3. [ ] Generalise the binding pattern only after 2 works.
