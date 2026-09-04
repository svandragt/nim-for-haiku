# Step-2 proof: a Nim GUI app on Haiku. Nim owns the entry point and calls
# into the BeAPI through the C++ shim (gui_shim.cpp). Build with the C++
# backend: nim cpp.
{.passL: "-lbe".}
{.compile: "gui_shim.cpp".}

proc runTestApp(signature, title: cstring): cint {.importc: "run_test_app", cdecl.}

when isMainModule:
  echo "hello from Nim ", NimVersion, " on ", hostOS, "/", hostCPU
  let width = runTestApp("application/x-vnd.nim-haiku-test", "Nim on Haiku")
  echo "[nim] BScreen width came back as ", width
  doAssert width > 0, "BScreen API returned nothing — interop broken"
  echo "[nim] BeAPI round-trip ok"
