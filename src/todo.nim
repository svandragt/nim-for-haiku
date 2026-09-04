## A basic todo list, in pure Nim — no C++ in this file. Type a task, click Add,
## and it appears as a checkbox you tick off. Built on the haiku.nim GUI API.
## Build + run:  ./ship.sh todo cpp     (or on Haiku: nim cpp -r src/todo.nim)
import std/[os, strutils]
import haiku

let app = newApp("application/x-vnd.nim-todo")
let win = newWindow("Todo (pure Nim)", 320, 240)
let demo = "--demo" in commandLineParams()
let field = win.addTextField(if demo: "Milk, eggs, bread" else: "")

win.addButton("Add", proc() =
  let task = field.text.strip()
  if task.len > 0:
    win.addTodo(task)
    field.clear()
    echo "[nim] added: ", task)

win.show()

# --demo: drive the *real* click path headlessly for the screenshot test —
# clickButton posts the same message a mouse click does, so this reads the
# field, runs the handler, and inserts a row exactly as a user would.
if demo:
  echo "[nim] field reads: ", field.text
  clickButton(0)
  win.addTodo("Ship the Nim app")   # a second row, added directly at runtime

app.run()
