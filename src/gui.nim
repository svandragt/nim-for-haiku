# Step-2/3 proof: a Nim GUI app on Haiku. Nim owns the entry point and drives
# the BeAPI through the C++ shim (gui_shim.cpp). Both directions are proven:
#   Nim -> BeAPI : runTestApp drives BApplication/BWindow, BScreen returns data.
#   BeAPI -> Nim : each button click calls onClick() below.
# Build with the C++ backend: nim cpp. Run with --hold to keep the window open
# for real clicks instead of the automated self-quitting test.
{.passL: "-lbe".}
{.compile: "gui_shim.cpp".}
import std/os

# Mutated from the BeAPI looper thread, so the callback must stay
# allocation-free (no echo/string ops here) — a plain global int is safe to
# touch from a thread Nim didn't create; GC-managed work is not.
var nimClicks: cint = 0

proc onClick(count: cint) {.cdecl.} =
  nimClicks = count

proc runTestApp(signature, title: cstring,
  cb: proc(count: cint) {.cdecl.}, autoDrive: cint): cint
  {.importc: "run_test_app", cdecl.}

when isMainModule:
  echo "hello from Nim ", NimVersion, " on ", hostOS, "/", hostCPU
  let hold = "--hold" in commandLineParams()
  let width = runTestApp("application/x-vnd.nim-haiku-test", "Nim on Haiku",
    onClick, if hold: 0 else: 1)
  echo "[nim] BScreen width came back as ", width
  doAssert width > 0, "BScreen API returned nothing — interop broken"
  echo "[nim] button->Nim callback fired ", nimClicks, " time(s)"
  if not hold:
    doAssert nimClicks == 3, "button->Nim callback did not fire 3x"
  echo "[nim] BeAPI round-trip ok"
