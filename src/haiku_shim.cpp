// Generic, reusable BeAPI binding. This is the ONLY C++ in an app — write-once
// library plumbing you never edit. App code lives entirely in Nim (see
// haiku.nim / app.nim).
//
// Key design choice: buttons target be_app, so every click is handled on the
// application (main) thread — the thread Nim started on. That means Nim
// callbacks can allocate/echo/use the GC freely, instead of running on a
// window's own thread where cross-thread GC would be unsafe.
#include <Application.h>
#include <Window.h>
#include <View.h>
#include <StringView.h>
#include <Button.h>
#include <TextControl.h>
#include <CheckBox.h>
#include <Screen.h>
#include <GroupLayout.h>
#include <ScrollView.h>
#include <Size.h>
#include <Looper.h>

typedef void (*DispatchCb)(int idx);

static const uint32 MSG_BASE = 'B000';

class HaikuApp : public BApplication {
public:
	HaikuApp(const char* sig, DispatchCb d)
		: BApplication(sig), fDispatch(d) {}

	void MessageReceived(BMessage* m) override
	{
		if (m->what >= MSG_BASE && fDispatch != NULL) {
			fDispatch((int)(m->what - MSG_BASE));  // on the main thread
			return;
		}
		BApplication::MessageReceived(m);
	}
private:
	DispatchCb fDispatch;
};

extern "C" void* haiku_app_new(const char* sig, DispatchCb dispatch)
{
	return new HaikuApp(sig, dispatch);
}

extern "C" void* haiku_window_new(float x, float y, float w, float h,
	const char* title)
{
	BWindow* win = new BWindow(BRect(x, y, x + w, y + h), title,
		B_TITLED_WINDOW, B_QUIT_ON_WINDOW_CLOSE);
	// A sane floor and a generous ceiling; without B_AUTO_UPDATE_SIZE_LIMITS the
	// window keeps its frame and the user can drag it.
	win->SetSizeLimits(200, 100000, 120, 100000);
	win->SetLayout(new BGroupLayout(B_VERTICAL));

	// The whole window scrolls as one: every widget goes in `content`, which is
	// wrapped in a borderless vertical scroll view filling the window. When the
	// content outgrows the window a scrollbar appears and scrolls everything —
	// input included. Panel-coloured views over the window's white top view
	// (SetViewUIColor tracks the live colour scheme); the scroll view is grey
	// too, so the area below short content stays grey rather than flashing white.
	BGroupLayout* contentLayout = new BGroupLayout(B_VERTICAL, 8);
	contentLayout->SetInsets(12, 12, 12, 12);
	BView* content = new BView("content", B_WILL_DRAW, contentLayout);
	content->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);

	BScrollView* scroll =
		new BScrollView("scroll", content, 0, false, true, B_NO_BORDER);
	scroll->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	// Fill the window, else the window's white top view shows around a
	// preferred-sized scroll view. Stretching it also makes BScrollView size the
	// content to the viewport width.
	scroll->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNLIMITED));
	win->AddChild(scroll);
	return win;
}

// Add a widget to a named container ("content" for the fixed top area, "list"
// for the scrolling todos). Pin its height so it can't stretch to fill; keep
// its own max width so buttons stay natural and inputs fill.
static void add_to(void* win, const char* container, BView* v)
{
	v->SetExplicitMaxSize(BSize(v->MaxSize().width, v->MinSize().height));
	((BWindow*)win)->FindView(container)->GetLayout()->AddView(v);
}

extern "C" void* haiku_label_add(void* win, const char* text)
{
	BStringView* v = new BStringView("label", text);
	add_to(win, "content", v);
	return v;
}

extern "C" void haiku_label_set(void* label, const char* text)
{
	// The click handler runs on the app thread, but this view belongs to the
	// window's looper — BeAPI requires locking that looper before touching the
	// view from another thread ("Looper must be locked." debugger call).
	BStringView* v = (BStringView*)label;
	BLooper* looper = v->Looper();
	if (looper != NULL && looper->Lock()) {
		v->SetText(text);
		looper->Unlock();
	} else {
		v->SetText(text);  // no looper yet (pre-Show): safe to set directly
	}
}

// idx is assigned by Nim (its handler-table index) and encoded into the
// button's message 'what' as MSG_BASE + idx.
extern "C" void haiku_button_add(void* win, const char* text, int idx)
{
	BButton* b = new BButton("btn", text, new BMessage(MSG_BASE + idx));
	b->SetTarget(be_app);
	add_to(win, "content", b);
}

extern "C" void* haiku_textfield_add(void* win, const char* initial)
{
	BTextControl* tc = new BTextControl("input", NULL, initial, NULL);
	// The layout shrinks the window to its widest child; without this the input
	// (and window) collapse too narrow to read typical todos.
	tc->SetExplicitMinSize(BSize(240, B_SIZE_UNSET));
	add_to(win, "content", tc);
	return tc;
}

// Read from the app thread but the control lives on the window looper, so lock
// it (same rule as haiku_label_set). Text() returns storage the control owns;
// Nim copies it into a Nim string on return, before it can change.
extern "C" const char* haiku_textfield_text(void* ctrl)
{
	BTextControl* tc = (BTextControl*)ctrl;
	BLooper* looper = tc->Looper();
	if (looper != NULL && looper->Lock()) {
		const char* t = tc->Text();
		looper->Unlock();
		return t;
	}
	return tc->Text();
}

extern "C" void haiku_textfield_clear(void* ctrl)
{
	BTextControl* tc = (BTextControl*)ctrl;
	BLooper* looper = tc->Looper();
	if (looper != NULL && looper->Lock()) {
		tc->SetText("");
		looper->Unlock();
	} else {
		tc->SetText("");
	}
}

// A todo row is a native checkbox: Haiku owns its checked ("done") state, so no
// Nim callback is needed. Added at runtime after Show(), so lock the window
// looper before mutating its view tree.
extern "C" void haiku_checkbox_add(void* win, const char* text)
{
	BWindow* w = (BWindow*)win;
	BCheckBox* cb = new BCheckBox("todo", text, NULL);
	if (w->Lock()) {
		add_to(win, "content", cb);
		w->Unlock();
	} else {
		add_to(win, "content", cb);
	}
}

// Post a button's message to be_app — byte-for-byte what app_server sends on a
// real click. Used by tests to drive the click path without a mouse.
extern "C" void haiku_click(int idx)
{
	be_app->PostMessage(MSG_BASE + idx);
}

extern "C" void haiku_window_show(void* win)
{
	((BWindow*)win)->Show();
}

extern "C" void haiku_app_run(void* app)
{
	((BApplication*)app)->Run();
}

extern "C" int haiku_screen_width()
{
	BScreen screen;
	return (int)(screen.Frame().Width() + 1);
}
