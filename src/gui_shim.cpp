// C++ shim: the only C++ in the project. It subclasses the BeAPI classes
// (which expect subclassing + virtual overrides) and exposes a plain
// extern "C" entry point that Nim calls. This is the interop pattern the
// whole viability question turns on.
#include <Application.h>
#include <Window.h>
#include <StringView.h>
#include <Screen.h>
#include <MessageRunner.h>
#include <Messenger.h>
#include <stdio.h>

class TestWindow : public BWindow {
public:
	TestWindow(BRect frame, const char* title)
		: BWindow(frame, title, B_TITLED_WINDOW,
			B_ASYNCHRONOUS_CONTROLS | B_QUIT_ON_WINDOW_CLOSE)
	{
		BStringView* label = new BStringView(Bounds(), "label",
			"Nim → BeAPI ✓", B_FOLLOW_ALL);
		label->SetAlignment(B_ALIGN_CENTER);
		AddChild(label);
	}

	// Overriding a BeAPI virtual from the shim — this is what Nim can't do
	// directly, and why the shim exists.
	bool QuitRequested() override
	{
		be_app->PostMessage(B_QUIT_REQUESTED);
		return true;
	}
};

// Called from Nim. Returns the screen width so Nim can prove a Haiku API
// round-tripped real data back into Nim land.
extern "C" int run_test_app(const char* signature, const char* title)
{
	BApplication app(signature);

	// Exercise a real Haiku API and hand the result back to the caller.
	BScreen screen;
	BRect sf = screen.Frame();
	int width = (int)(sf.Width() + 1);
	printf("[shim] BScreen.Frame = %dx%d\n", width, (int)(sf.Height() + 1));
	fflush(stdout);  // printf is block-buffered to a pipe; flush so ssh sees it

	TestWindow* w = new TestWindow(BRect(120, 120, 460, 260), title);
	w->Show();

	// Self-quit after 3s so the app exits cleanly on its own: app.Run()
	// returns, the caller gets its result, and no external kill is needed
	// (a kill would abort before the return value and trailing output).
	BMessenger self(&app);
	new BMessageRunner(self, new BMessage(B_QUIT_REQUESTED), 3000000LL, 1);

	app.Run();
	return width;
}
