/*
 * AboutWindow.h - About dialog for EmailViews
 * Distributed under the terms of the MIT License.
 */

#ifndef ABOUT_WINDOW_H
#define ABOUT_WINDOW_H

#include <Window.h>

class BBitmap;
class BTextView;

class AboutWindow : public BWindow {
public:
	explicit AboutWindow(BBitmap* icon);
	virtual ~AboutWindow();
	virtual void MessageReceived(BMessage* message);
	virtual bool QuitRequested();

private:
	void _SaveFrame();
	void _LoadFrame();

	static const uint32 MSG_OK = 'okok';
	BBitmap* fIcon;
	BTextView* fTextView;
};

#endif // ABOUT_WINDOW_H
