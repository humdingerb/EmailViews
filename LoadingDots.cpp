/*
 * LoadingDots.cpp - Animated three-dot loading indicator
 * Distributed under the terms of the MIT License.
 */

#include "LoadingDots.h"

#include <InterfaceDefs.h>
#include <Message.h>
#include <Messenger.h>


LoadingDots::LoadingDots(const char* name)
	:
	BView(name, B_WILL_DRAW),
	fActive(false),
	fCurrentDot(0),
	fRunner(NULL)
{
	SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
}


LoadingDots::~LoadingDots()
{
	delete fRunner;
}


void
LoadingDots::Start()
{
	if (fActive)
		return;
	fActive = true;
	fCurrentDot = 0;

	delete fRunner;
	BMessage msg(kMsgDotsPulse);
	fRunner = new BMessageRunner(BMessenger(this), &msg, 450000);
	Invalidate();
}


void
LoadingDots::Stop()
{
	fActive = false;
	fCurrentDot = 0;
	delete fRunner;
	fRunner = NULL;
	Invalidate();
}


void
LoadingDots::Draw(BRect updateRect)
{
	BRect bounds = Bounds();
	rgb_color bg = ui_color(B_PANEL_BACKGROUND_COLOR);
	SetHighColor(bg);
	FillRect(bounds);

	if (!fActive)
		return;

	rgb_color dimColor = tint_color(bg, B_DARKEN_2_TINT);
	rgb_color litColor = ui_color(B_STATUS_BAR_COLOR);

	float dotSize = floorf(bounds.Height() * 0.35f);
	float spacing = dotSize * 1.8f;
	float totalWidth = kDotCount * dotSize + (kDotCount - 1) * (spacing - dotSize);
	float startX = bounds.left + (bounds.Width() - totalWidth) / 2.0f;
	float centerY = bounds.top + bounds.Height() / 2.0f;

	for (int32 i = 0; i < kDotCount; i++) {
		float cx = startX + i * spacing + dotSize / 2.0f;
		SetHighColor(i == fCurrentDot ? litColor : dimColor);
		FillEllipse(BPoint(cx, centerY), dotSize / 2.0f, dotSize / 2.0f);
	}
}


void
LoadingDots::MessageReceived(BMessage* message)
{
	if (message->what == kMsgDotsPulse && fActive) {
		fCurrentDot = (fCurrentDot + 1) % kDotCount;
		Invalidate();
	} else {
		BView::MessageReceived(message);
	}
}
