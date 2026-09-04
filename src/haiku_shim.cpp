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
#include <StringView.h>
#include <Button.h>
#include <Screen.h>
#include <GroupLayout.h>
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
		B_TITLED_WINDOW, B_QUIT_ON_WINDOW_CLOSE | B_AUTO_UPDATE_SIZE_LIMITS);
	BGroupLayout* layout = new BGroupLayout(B_VERTICAL, 8);
	layout->SetInsets(12, 12, 12, 12);
	win->SetLayout(layout);
	return win;
}

extern "C" void* haiku_label_add(void* win, const char* text)
{
	BStringView* v = new BStringView("label", text);
	((BWindow*)win)->AddChild(v);
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
	((BWindow*)win)->AddChild(b);
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
