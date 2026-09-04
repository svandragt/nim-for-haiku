## A complete Haiku GUI app in pure Nim — no C++ anywhere in this file.
## Build + run:  ./ship.sh app cpp     (or on Haiku: nim cpp -r src/app.nim)
import haiku

var clicks = 0

let app = newApp("application/x-vnd.nim-counter")
let win = newWindow("Counter (pure Nim)", 240, 110)
let label = win.addLabel("Clicks: 0")

win.addButton("Click me", proc() =
  inc clicks
  label.text = "Clicks: " & $clicks
  echo "[nim] click ", clicks)

win.show()
app.run()
