// C++ shim: the only C++ in the project. It subclasses the BeAPI classes
// (which expect subclassing + virtual overrides) and exposes a plain
// extern "C" entry point that Nim calls. This is the interop pattern the
// whole viability question turns on.
//
// Two directions are proven here:
//   Nim -> BeAPI : Nim calls run_test_app, which drives BApplication/BWindow.
//   BeAPI -> Nim : a BButton click routes through MessageReceived into a Nim
//                  callback (function pointer passed in from Nim).
#include <Application.h>
#include <Window.h>
#include <StringView.h>
#include <Button.h>
#include <Screen.h>
#include <MessageRunner.h>
#include <Messenger.h>
#include <stdio.h>

typedef void (*ClickCb)(int count);

static const uint32 MSG_CLICK = 'BTN!';

class TestWindow : public BWindow {
public:
	TestWindow(BRect frame, const char* title, ClickCb cb)
		: BWindow(frame, title, B_TITLED_WINDOW,
			B_ASYNCHRONOUS_CONTROLS | B_QUIT_ON_WINDOW_CLOSE),
		fCb(cb), fCount(0)
	{
		BRect b = Bounds();
		fLabel = new BStringView(BRect(b.left, 12, b.right, 32), "label",
			"Nim \xE2\x86\x90 BeAPI: click below", B_FOLLOW_LEFT_RIGHT);
		fLabel->SetAlignment(B_ALIGN_CENTER);
		AddChild(fLabel);

		float cx = (b.left + b.right) / 2;
		fButton = new BButton(BRect(cx - 55, 55, cx + 55, 85), "btn",
			"Click me", new BMessage(MSG_CLICK));
		AddChild(fButton);
		fButton->SetTarget(this);
	}

	// A real click posts MSG_CLICK to this window; this handler routes it into
	// Nim. Overriding a BeAPI virtual is exactly what the shim is for.
	void MessageReceived(BMessage* msg) override
	{
		if (msg->what == MSG_CLICK) {
			fCount++;
			if (fCb != NULL)
				fCb(fCount);        // <-- crosses into Nim
			char buf[64];
			snprintf(buf, sizeof buf, "Nim handled click #%d", fCount);
			fLabel->SetText(buf);
			return;
		}
		BWindow::MessageReceived(msg);
	}

	bool QuitRequested() override
	{
		be_app->PostMessage(B_QUIT_REQUESTED);
		return true;
	}

private:
	ClickCb      fCb;
	int          fCount;
	BStringView* fLabel;
	BButton*     fButton;
};

// Called from Nim. Returns the screen width (proves a Haiku API round-trips
// real data back into Nim). onClick is invoked on each button click.
// autoDrive != 0: synthesize clicks and self-quit (headless test mode).
// autoDrive == 0: leave the window open for real interaction; the app quits
// when the user closes the window (B_QUIT_ON_WINDOW_CLOSE).
extern "C" int run_test_app(const char* signature, const char* title,
	ClickCb onClick, int autoDrive)
{
	BApplication app(signature);

	BScreen screen;
	BRect sf = screen.Frame();
	int width = (int)(sf.Width() + 1);
	printf("[shim] BScreen.Frame = %dx%d\n", width, (int)(sf.Height() + 1));
	fflush(stdout);  // printf is block-buffered to a pipe; flush so ssh sees it

	TestWindow* w = new TestWindow(BRect(120, 120, 460, 260), title, onClick);
	w->Show();

	if (autoDrive) {
		// Synthesize 3 clicks by posting the button's own message to the
		// window — byte-for-byte what app_server sends on a real click.
		BMessenger win(w);
		new BMessageRunner(win, new BMessage(MSG_CLICK), 700000LL, 3);
		// Self-quit cleanly so app.Run() returns and trailing output flushes.
		BMessenger self(&app);
		new BMessageRunner(self, new BMessage(B_QUIT_REQUESTED), 3000000LL, 1);
	}

	app.Run();
	return width;
}
