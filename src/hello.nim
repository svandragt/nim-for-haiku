# Step-1 proof: a plain Nim program that compiles and runs on Haiku.
# Prints host info and does a trivial computation so a broken toolchain fails loudly.
import std/strformat

proc fib(n: int): int =
  if n < 2: n else: fib(n - 1) + fib(n - 2)

when isMainModule:
  echo &"hello from Nim {NimVersion} on {hostOS}/{hostCPU}"
  let f = fib(20)
  doAssert f == 6765, "fib(20) wrong — compiler/runtime broken"
  echo &"fib(20) = {f}  (ok)"
