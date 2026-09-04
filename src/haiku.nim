## High-level Haiku GUI API for Nim. Write your app against this — never the
## C++ shim behind it. Build with the C++ backend: `nim cpp`.
##
## Example (see app.nim):
##   let app = newApp("application/x-vnd.me-counter")
##   let win = newWindow("Counter")
##   let label = win.addLabel("Clicks: 0")
##   win.addButton("Click me", proc() = label.text = "clicked")
##   win.show(); app.run()
{.passL: "-lbe".}
{.compile: "haiku_shim.cpp".}

type
  App* = object
    p: pointer
  Window* = object
    p: pointer
  Label* = object
    p: pointer
  TextField* = object
    p: pointer

# Click handlers, indexed by the id encoded in each button's message. The shim
# dispatches on the main thread, so these run where Nim's GC is happy.
var handlers: seq[proc()] = @[]

proc dispatch(idx: cint) {.cdecl.} =
  if idx >= 0 and idx.int < handlers.len:
    handlers[idx.int]()

proc c_app_new(sig: cstring, d: proc(idx: cint) {.cdecl.}): pointer
  {.importc: "haiku_app_new", cdecl.}
proc c_window_new(x, y, w, h: cfloat, title: cstring): pointer
  {.importc: "haiku_window_new", cdecl.}
proc c_label_add(win: pointer, text: cstring): pointer
  {.importc: "haiku_label_add", cdecl.}
proc c_label_set(label: pointer, text: cstring)
  {.importc: "haiku_label_set", cdecl.}
proc c_button_add(win: pointer, text: cstring, idx: cint)
  {.importc: "haiku_button_add", cdecl.}
proc c_textfield_add(win: pointer, initial: cstring): pointer
  {.importc: "haiku_textfield_add", cdecl.}
proc c_textfield_text(ctrl: pointer): cstring
  {.importc: "haiku_textfield_text", cdecl.}
proc c_textfield_clear(ctrl: pointer) {.importc: "haiku_textfield_clear", cdecl.}
proc c_checkbox_add(win: pointer, text: cstring)
  {.importc: "haiku_checkbox_add", cdecl.}
proc c_click(idx: cint) {.importc: "haiku_click", cdecl.}
proc c_scroll_end(win: pointer) {.importc: "haiku_scroll_end", cdecl.}
proc c_window_show(win: pointer) {.importc: "haiku_window_show", cdecl.}
proc c_app_run(app: pointer) {.importc: "haiku_app_run", cdecl.}
proc c_screen_width(): cint {.importc: "haiku_screen_width", cdecl.}

proc newApp*(signature: string): App =
  ## Create the application. Call this first — widgets need it to exist.
  App(p: c_app_new(signature.cstring, dispatch))

proc newWindow*(title: string, width = 300.0, height = 120.0): Window =
  ## A titled window that quits the app when closed. Widgets stack vertically.
  Window(p: c_window_new(100, 100, width.cfloat, height.cfloat, title.cstring))

proc addLabel*(win: Window, text: string): Label =
  Label(p: c_label_add(win.p, text.cstring))

proc `text=`*(label: Label, s: string) =
  ## Update a label's text, e.g. `label.text = "Clicks: 3"`.
  c_label_set(label.p, s.cstring)

proc addButton*(win: Window, text: string, onClick: proc()) =
  ## Add a button; onClick runs on the main thread on every press.
  handlers.add(onClick)
  c_button_add(win.p, text.cstring, (handlers.len - 1).cint)

proc addTextField*(win: Window, initial = ""): TextField =
  ## A single-line text input. Read it with `.text`, empty it with `.clear`.
  TextField(p: c_textfield_add(win.p, initial.cstring))

proc text*(field: TextField): string = $c_textfield_text(field.p)
proc clear*(field: TextField) = c_textfield_clear(field.p)

proc addTodo*(win: Window, text: string) =
  ## Append a checkable todo row at runtime. Safe to call after `show`.
  c_checkbox_add(win.p, text.cstring)

proc scrollToEnd*(win: Window) =
  ## Scroll the task list to the bottom, revealing the newest row.
  c_scroll_end(win.p)

proc show*(win: Window) = c_window_show(win.p)

proc clickButton*(idx: int) =
  ## Fire the idx-th button as if clicked (buttons are numbered in add order).
  ## Mainly for headless testing — drives the real message path.
  c_click(idx.cint)

proc run*(app: App) =
  ## Enter the event loop. Blocks until the app quits.
  c_app_run(app.p)

proc screenWidth*(): int = c_screen_width().int
