/*
 * LoadingDots.h - Animated three-dot loading indicator
 * Distributed under the terms of the MIT License.
 *
 * A self-contained BView that draws three dots animating in sequence
 * to indicate background activity. Call Start() to begin the animation
 * and Stop() to end it. The view draws nothing when inactive.
 */

#ifndef LOADING_DOTS_H
#define LOADING_DOTS_H

#include <MessageRunner.h>
#include <View.h>

class LoadingDots : public BView {
public:
				LoadingDots(const char* name);
	virtual		~LoadingDots();

	void		Start();
	void		Stop();
	bool		IsActive() const { return fActive; }

	virtual void	Draw(BRect updateRect);
	virtual void	MessageReceived(BMessage* message);

private:
	static const uint32	kMsgDotsPulse = 'dtpl';
	static const int32	kDotCount = 3;

	bool			fActive;
	int32			fCurrentDot;
	BMessageRunner*	fRunner;
};

#endif // LOADING_DOTS_H
