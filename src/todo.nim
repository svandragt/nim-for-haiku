## A basic todo list, in pure Nim — no C++ in this file. Type a task, click Add,
## and it appears as a checkbox you tick off. Built on the haiku.nim GUI API.
## Build + run:  ./ship.sh todo cpp     (or on Haiku: nim cpp -r src/todo.nim)
import std/[os, strutils]
import haiku

let app = newApp("application/x-vnd.nim-todo")
let win = newWindow("Todo (pure Nim)", 320, 240)
let demo = "--demo" in commandLineParams()
let field = win.addTextField(if demo: "Buy coffee" else: "")

win.addButton("Add", proc() =
  let task = field.text.strip()
  if task.len > 0:
    win.addTodo(task)
    field.clear()
    win.scrollToEnd()   # bring the new task into view
    echo "[nim] added: ", task)

win.show()

# --demo: seed enough rows to overflow a small window, then add one more through
# the real Add click (clickButton posts the same message a mouse does). Its
# scrollToEnd runs with the layout settled, so the screenshot proves the
# scrollbar range recalculates on a runtime add and the tail is reachable.
if demo:
  for t in ["Ship the Nim app", "Write the journal", "Buy stamps",
            "Call the plumber", "Water the plants", "Renew the domain"]:
    win.addTodo(t)
  clickButton(0)

app.run()
