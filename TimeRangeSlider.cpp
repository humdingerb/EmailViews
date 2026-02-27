/*
 * TimeRangeSlider.cpp - Dual-handle slider for selecting a time range
 * Distributed under the terms of the MIT License.
 *
 * Uses a non-linear step table (kTimeSteps) to map slider positions to
 * time offsets. Recent time periods (days) get more slider real estate
 * than distant ones (years), matching the intuition that users care more
 * about fine-grained control over recent emails. Both handles snap to
 * discrete steps to avoid meaningless in-between positions.
 */

#include "TimeRangeSlider.h"

#include <Catalog.h>
#include <ControlLook.h>
#include <Window.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "TimeRangeSlider"


// Layout constants
static const float kMinTrackWidth = 100.0f;
static const float kLabelGap = 2.0f;		// Gap between label and handle
static const float kLabelPadding = 4.0f;	// Horizontal padding for labels

// Time constants (in seconds)
static const time_t kSecondsPerDay = 24 * 60 * 60;
static const time_t kSecondsPerWeek = 7 * kSecondsPerDay;
static const time_t kSecondsPerMonth = 30 * kSecondsPerDay;		// Approximate
static const time_t kSecondsPerYear = 365 * kSecondsPerDay;		// Approximate

// Discrete time steps: slider snaps to these positions only.
// Position 0.0 = "Today" (newest), 1.0 = "All" (oldest).
// Spacing is logarithmic-ish: days are closely spaced, months/years sparse.
// offsetSeconds of 0 has special meaning at the endpoints: "no limit".
struct TimeStep {
	float		position;
	time_t		offsetSeconds;	// 0 means "no limit" for that end
	const char*	label;
};

static const TimeStep kTimeSteps[] = {
	{ 0.00f,  0,                     "Today" },
	{ 0.05f,  1 * kSecondsPerDay,    "-1 day" },
	{ 0.09f,  2 * kSecondsPerDay,    "-2 days" },
	{ 0.13f,  3 * kSecondsPerDay,    "-3 days" },
	{ 0.17f,  4 * kSecondsPerDay,    "-4 days" },
	{ 0.21f,  5 * kSecondsPerDay,    "-5 days" },
	{ 0.25f,  6 * kSecondsPerDay,    "-6 days" },
	{ 0.29f,  1 * kSecondsPerWeek,   "-1 week" },
	{ 0.35f,  2 * kSecondsPerWeek,   "-2 weeks" },
	{ 0.41f,  3 * kSecondsPerWeek,   "-3 weeks" },
	{ 0.47f,  1 * kSecondsPerMonth,  "-1 month" },
	{ 0.53f,  2 * kSecondsPerMonth,  "-2 months" },
	{ 0.59f,  3 * kSecondsPerMonth,  "-3 months" },
	{ 0.67f,  6 * kSecondsPerMonth,  "-6 months" },
	{ 0.76f,  1 * kSecondsPerYear,   "-1 year" },
	{ 0.84f,  2 * kSecondsPerYear,   "-2 years" },
	{ 0.92f,  5 * kSecondsPerYear,   "-5 years" },
	{ 1.00f,  0,                     "All" },	// 0 offset means oldest/no limit
};

static const int kNumTimeSteps = sizeof(kTimeSteps) / sizeof(kTimeSteps[0]);


TimeRangeSlider::TimeRangeSlider(const char* name, BMessage* message)
	:
	BView(name, B_WILL_DRAW | B_FRAME_EVENTS),
	fLeftValue(0.0f),
	fRightValue(1.0f),
	fNewestTime(time(NULL)),
	fOldestTime(0),
	fDragging(kDragNone),
	fHoveredHandle(0),
	fDragOffset(0.0f),
	fModificationMessage(message),
	fHandleWidth(9.0f),
	fHandleHeight(16.0f),
	fTrackHeight(8.0f),
	fTrackHorizInset(24.0f),
	fLabelHeight(0.0f)
{
	SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
}


TimeRangeSlider::~TimeRangeSlider()
{
	delete fModificationMessage;
}


void
TimeRangeSlider::AttachedToWindow()
{
	BView::AttachedToWindow();
	
	// Derive all layout dimensions from font metrics so the slider
	// scales correctly at any system font size.
	font_height fh;
	GetFontHeight(&fh);
	fLabelHeight = ceilf(fh.ascent + fh.descent);

	// Handle height proportional to font, width keeps a comfortable aspect ratio
	fHandleHeight = ceilf(fLabelHeight * 1.0f);
	fHandleWidth  = ceilf(fHandleHeight * 0.55f);

	// Track is half the handle height
	fTrackHeight  = ceilf(fHandleHeight * 0.5f);

	// Horizontal inset wide enough for the widest label (value 0.0 = "Today")
	// so labels never get clipped regardless of font size or language.
	fTrackHorizInset = StringWidth(_FormatTimeLabel(0.0f).String()) + kLabelPadding * 2;
}


void
TimeRangeSlider::Draw(BRect updateRect)
{
	BRect bounds = Bounds();
	
	// Fill background
	SetLowUIColor(B_PANEL_BACKGROUND_COLOR);
	FillRect(bounds, B_SOLID_LOW);
	
	// Draw track
	BRect trackRect = _TrackRect();
	_DrawTrack(trackRect);
	
	// Draw handles
	BRect leftHandle = _LeftHandleRect();
	BRect rightHandle = _RightHandleRect();
	
	_DrawHandle(leftHandle, true, fHoveredHandle == kDragLeft, fDragging == kDragLeft);
	_DrawHandle(rightHandle, false, fHoveredHandle == kDragRight, fDragging == kDragRight);
	
	// Draw labels above handles
	BString leftLabel = _FormatTimeLabel(fLeftValue);
	BString rightLabel = _FormatTimeLabel(fRightValue);
	
	_DrawLabel(leftLabel.String(), leftHandle.LeftTop(), true);
	_DrawLabel(rightLabel.String(), rightHandle.LeftTop(), false);
}


void
TimeRangeSlider::MouseDown(BPoint where)
{
	BRect leftHandle = _LeftHandleRect();
	BRect rightHandle = _RightHandleRect();
	
	// Expand hit areas slightly for easier grabbing
	leftHandle.InsetBy(-2, -2);
	rightHandle.InsetBy(-2, -2);
	
	if (leftHandle.Contains(where)) {
		fDragging = kDragLeft;
		fDragOffset = where.x - _ValueToX(fLeftValue);
		SetMouseEventMask(B_POINTER_EVENTS, B_LOCK_WINDOW_FOCUS);
	} else if (rightHandle.Contains(where)) {
		fDragging = kDragRight;
		fDragOffset = where.x - _ValueToX(fRightValue);
		SetMouseEventMask(B_POINTER_EVENTS, B_LOCK_WINDOW_FOCUS);
	}
	
	Invalidate();
}


void
TimeRangeSlider::MouseMoved(BPoint where, uint32 transit, const BMessage* message)
{
	if (fDragging != kDragNone) {
		float newValue = _XToValue(where.x - fDragOffset);
		newValue = std::max(0.0f, std::min(1.0f, newValue));
		
		// Snap to nearest discrete step
		newValue = _SnapToStep(newValue);
		
		// Remember old values to detect actual changes
		float oldLeft = fLeftValue;
		float oldRight = fRightValue;
		
		if (fDragging == kDragLeft) {
			// Left handle must stay at least one step before right handle
			float maxLeft = _PrevStepBefore(fRightValue);
			fLeftValue = std::min(newValue, maxLeft);
		} else {
			// Right handle must stay at least one step after left handle
			float minRight = _NextStepAfter(fLeftValue);
			fRightValue = std::max(newValue, minRight);
		}
		
		Invalidate();
		
		// Notify if the step actually changed
		if (fLeftValue != oldLeft || fRightValue != oldRight) {
			_NotifyChanged();
		}
	} else {
		// Update hover state
		int oldHover = fHoveredHandle;
		fHoveredHandle = 0;
		
		BRect leftHandle = _LeftHandleRect();
		BRect rightHandle = _RightHandleRect();
		leftHandle.InsetBy(-2, -2);
		rightHandle.InsetBy(-2, -2);
		
		if (leftHandle.Contains(where))
			fHoveredHandle = kDragLeft;
		else if (rightHandle.Contains(where))
			fHoveredHandle = kDragRight;
		
		if (oldHover != fHoveredHandle)
			Invalidate();
	}
}


void
TimeRangeSlider::MouseUp(BPoint where)
{
	if (fDragging != kDragNone) {
		fDragging = kDragNone;
		Invalidate();
		// Note: _NotifyChanged() is already called in MouseMoved when step changes
	}
}


void
TimeRangeSlider::GetPreferredSize(float* width, float* height)
{
	font_height fh;
	GetFontHeight(&fh);
	float labelHeight = ceilf(fh.ascent + fh.descent);
	
	*width = kMinTrackWidth + fHandleWidth;
	*height = labelHeight + kLabelGap + fHandleHeight;
}


BSize
TimeRangeSlider::MinSize()
{
	float width, height;
	GetPreferredSize(&width, &height);
	return BSize(width, height);
}


BSize
TimeRangeSlider::PreferredSize()
{
	float width, height;
	GetPreferredSize(&width, &height);
	// Prefer wider for better usability
	return BSize(250.0f, height);
}


BSize
TimeRangeSlider::MaxSize()
{
	float width, height;
	GetPreferredSize(&width, &height);
	return BSize(B_SIZE_UNLIMITED, height);
}


void
TimeRangeSlider::SetTimeRange(time_t oldest, time_t newest)
{
	fOldestTime = oldest;
	fNewestTime = newest;
	Invalidate();
}


time_t
TimeRangeSlider::FromTime() const
{
	return _ValueToTime(fLeftValue);
}


time_t
TimeRangeSlider::ToTime() const
{
	return _ValueToTime(fRightValue);
}


void
TimeRangeSlider::SetFromTime(time_t from)
{
	fLeftValue = _TimeToValue(from);
	Invalidate();
}


void
TimeRangeSlider::SetToTime(time_t to)
{
	fRightValue = _TimeToValue(to);
	Invalidate();
}


void
TimeRangeSlider::SetLeftValue(float value)
{
	value = _SnapToStep(value);
	// Left handle must stay at least one step before right handle
	float maxLeft = _PrevStepBefore(fRightValue);
	fLeftValue = std::max(0.0f, std::min(value, maxLeft));
	Invalidate();
}


void
TimeRangeSlider::SetRightValue(float value)
{
	value = _SnapToStep(value);
	// Right handle must stay at least one step after left handle
	float minRight = _NextStepAfter(fLeftValue);
	fRightValue = std::max(minRight, std::min(1.0f, value));
	Invalidate();
}


bool
TimeRangeSlider::IsFullRange() const
{
	return fLeftValue <= 0.001f && fRightValue >= 0.999f;
}


bool
TimeRangeSlider::IsLeftAtTodayOrNow() const
{
	// "Now" is at value 0
	if (fLeftValue <= 0.001f)
		return true;
	
	// Check if left handle is within "Today" (offset < 1 day from newest)
	time_t time = _ValueToTime(fLeftValue);
	time_t offset = fNewestTime - time;
	return offset < kSecondsPerDay;
}


void
TimeRangeSlider::SetModificationMessage(BMessage* message)
{
	delete fModificationMessage;
	fModificationMessage = message;
}


float
TimeRangeSlider::LabelAreaHeight() const
{
	return fLabelHeight + kLabelGap;
}


float
TimeRangeSlider::_ValueToX(float value) const
{
	BRect track = _TrackRect();
	float usableWidth = track.Width() - fHandleWidth;
	return track.left + fHandleWidth / 2 + value * usableWidth;
}


float
TimeRangeSlider::_XToValue(float x) const
{
	BRect track = _TrackRect();
	float usableWidth = track.Width() - fHandleWidth;
	return (x - track.left - fHandleWidth / 2) / usableWidth;
}


BRect
TimeRangeSlider::_TrackRect() const
{
	BRect bounds = Bounds();
	float top = fLabelHeight + kLabelGap + (fHandleHeight - fTrackHeight) / 2;
	return BRect(bounds.left + fTrackHorizInset, top, 
	             bounds.right - fTrackHorizInset, top + fTrackHeight);
}


BRect
TimeRangeSlider::_LeftHandleRect() const
{
	float x = _ValueToX(fLeftValue);
	float top = fLabelHeight + kLabelGap;
	return BRect(x - fHandleWidth / 2, top, x + fHandleWidth / 2, top + fHandleHeight);
}


BRect
TimeRangeSlider::_RightHandleRect() const
{
	float x = _ValueToX(fRightValue);
	float top = fLabelHeight + kLabelGap;
	return BRect(x - fHandleWidth / 2, top, x + fHandleWidth / 2, top + fHandleHeight);
}


time_t
TimeRangeSlider::_ValueToTime(float value) const
{
	// Find the step that matches this value (should already be snapped)
	// But handle edge cases gracefully
	
	if (value <= 0.001f)
		return fNewestTime;	// "Today" - no upper limit
	if (value >= 0.999f)
		return fOldestTime;	// "All" - no lower limit
	
	// Find the matching step
	for (int i = 0; i < kNumTimeSteps; i++) {
		if (fabs(value - kTimeSteps[i].position) < 0.01f) {
			time_t offset = kTimeSteps[i].offsetSeconds;
			if (offset == 0) {
				// Special case: 0 offset at position 0 means "today", at 1.0 means "oldest"
				if (value < 0.5f)
					return fNewestTime;
				else
					return fOldestTime;
			}
			time_t result = fNewestTime - offset;
			if (result < fOldestTime)
				result = fOldestTime;
			return result;
		}
	}
	
	// Fallback: shouldn't happen if values are properly snapped
	// Find nearest step
	float nearestPos = _SnapToStep(value);
	for (int i = 0; i < kNumTimeSteps; i++) {
		if (fabs(nearestPos - kTimeSteps[i].position) < 0.01f) {
			time_t offset = kTimeSteps[i].offsetSeconds;
			if (offset == 0) {
				if (nearestPos < 0.5f)
					return fNewestTime;
				else
					return fOldestTime;
			}
			time_t result = fNewestTime - offset;
			if (result < fOldestTime)
				result = fOldestTime;
			return result;
		}
	}
	
	return fNewestTime;
}


float
TimeRangeSlider::_TimeToValue(time_t time) const
{
	if (time >= fNewestTime)
		return 0.0f;
	if (time <= fOldestTime)
		return 1.0f;
	
	time_t offset = fNewestTime - time;
	
	// Find the step with the closest offset (without going under)
	for (int i = kNumTimeSteps - 2; i >= 0; i--) {
		if (kTimeSteps[i].offsetSeconds > 0 && offset >= kTimeSteps[i].offsetSeconds) {
			return kTimeSteps[i].position;
		}
	}
	
	return 0.0f;  // Default to "Today"
}


BString
TimeRangeSlider::_FormatTimeLabel(float value) const
{
	// Find the matching step index
	int stepIndex = -1;
	for (int i = 0; i < kNumTimeSteps; i++) {
		if (fabs(value - kTimeSteps[i].position) < 0.01f) {
			stepIndex = i;
			break;
		}
	}
	
	// Fallback: find nearest step
	if (stepIndex < 0) {
		float nearestPos = _SnapToStep(value);
		for (int i = 0; i < kNumTimeSteps; i++) {
			if (fabs(nearestPos - kTimeSteps[i].position) < 0.01f) {
				stepIndex = i;
				break;
			}
		}
	}
	
	// Return translated label based on step index
	// Comment for translators: keep labels short to fit above slider handles
	#define TIME_LABEL_COMMENT "Time range slider label - keep as short as possible"
	switch (stepIndex) {
		case 0:  return B_TRANSLATE_COMMENT("Today", TIME_LABEL_COMMENT);
		case 1:  return B_TRANSLATE_COMMENT("-1 day", TIME_LABEL_COMMENT);
		case 2:  return B_TRANSLATE_COMMENT("-2 days", TIME_LABEL_COMMENT);
		case 3:  return B_TRANSLATE_COMMENT("-3 days", TIME_LABEL_COMMENT);
		case 4:  return B_TRANSLATE_COMMENT("-4 days", TIME_LABEL_COMMENT);
		case 5:  return B_TRANSLATE_COMMENT("-5 days", TIME_LABEL_COMMENT);
		case 6:  return B_TRANSLATE_COMMENT("-6 days", TIME_LABEL_COMMENT);
		case 7:  return B_TRANSLATE_COMMENT("-1 week", TIME_LABEL_COMMENT);
		case 8:  return B_TRANSLATE_COMMENT("-2 weeks", TIME_LABEL_COMMENT);
		case 9:  return B_TRANSLATE_COMMENT("-3 weeks", TIME_LABEL_COMMENT);
		case 10: return B_TRANSLATE_COMMENT("-1 month", TIME_LABEL_COMMENT);
		case 11: return B_TRANSLATE_COMMENT("-2 months", TIME_LABEL_COMMENT);
		case 12: return B_TRANSLATE_COMMENT("-3 months", TIME_LABEL_COMMENT);
		case 13: return B_TRANSLATE_COMMENT("-6 months", TIME_LABEL_COMMENT);
		case 14: return B_TRANSLATE_COMMENT("-1 year", TIME_LABEL_COMMENT);
		case 15: return B_TRANSLATE_COMMENT("-2 years", TIME_LABEL_COMMENT);
		case 16: return B_TRANSLATE_COMMENT("-5 years", TIME_LABEL_COMMENT);
		case 17: return B_TRANSLATE_COMMENT("All", TIME_LABEL_COMMENT);
		default: return B_TRANSLATE_COMMENT("Today", TIME_LABEL_COMMENT);
	}
	#undef TIME_LABEL_COMMENT
}


float
TimeRangeSlider::_SnapToStep(float value) const
{
	// Find the nearest step position
	float nearestPos = kTimeSteps[0].position;
	float nearestDist = fabs(value - nearestPos);
	
	for (int i = 1; i < kNumTimeSteps; i++) {
		float dist = fabs(value - kTimeSteps[i].position);
		if (dist < nearestDist) {
			nearestDist = dist;
			nearestPos = kTimeSteps[i].position;
		}
	}
	
	return nearestPos;
}


float
TimeRangeSlider::_NextStepAfter(float value) const
{
	// Find the first step position greater than value
	for (int i = 0; i < kNumTimeSteps; i++) {
		if (kTimeSteps[i].position > value + 0.001f) {
			return kTimeSteps[i].position;
		}
	}
	return 1.0f;  // Return max if at end
}


float
TimeRangeSlider::_PrevStepBefore(float value) const
{
	// Find the last step position less than value
	for (int i = kNumTimeSteps - 1; i >= 0; i--) {
		if (kTimeSteps[i].position < value - 0.001f) {
			return kTimeSteps[i].position;
		}
	}
	return 0.0f;  // Return min if at start
}


void
TimeRangeSlider::_DrawTrack(BRect rect)
{
	rgb_color base = ui_color(B_PANEL_BACKGROUND_COLOR);
	
	// Draw groove (recessed track) with light grey fill
	rgb_color trackFill = tint_color(base, B_DARKEN_1_TINT);
	be_control_look->DrawSliderBar(this, rect, rect, base, 
		trackFill, 0, B_HORIZONTAL);
	
	// Draw filled portion between handles
	float leftX = _ValueToX(fLeftValue);
	float rightX = _ValueToX(fRightValue);
	
	if (rightX > leftX) {
		BRect fillRect = rect;
		fillRect.left = leftX;
		fillRect.right = rightX;
		fillRect.InsetBy(1, 1);
		
		// Use a tinted control color for the fill
		rgb_color fillColor = tint_color(ui_color(B_CONTROL_MARK_COLOR), 0.8);
		SetHighColor(fillColor);
		FillRect(fillRect);
	}
}


void
TimeRangeSlider::_DrawHandle(BRect rect, bool isLeft, bool isHovered, bool isDragging)
{
	rgb_color base = ui_color(B_PANEL_BACKGROUND_COLOR);
	
	uint32 flags = 0;
	if (isDragging)
		flags |= BControlLook::B_ACTIVATED;
	if (isHovered && !isDragging)
		flags |= BControlLook::B_HOVER;
	
	// Draw handle as a small slider thumb
	be_control_look->DrawSliderThumb(this, rect, rect, base, flags, B_HORIZONTAL);
}


void
TimeRangeSlider::_DrawLabel(const char* text, BPoint handleTop, bool isLeft)
{
	font_height fh;
	GetFontHeight(&fh);
	
	float textWidth = StringWidth(text);
	
	// Position label above the handle
	// "Today" and "All" are centered (at extreme positions)
	// Other labels: left grows left, right grows right (to avoid overlap)
	float x;
	if (strcmp(text, "Today") == 0 || strcmp(text, "All") == 0) {
		// Center the label above the handle
		x = handleTop.x + fHandleWidth / 2 - textWidth / 2;
	} else if (isLeft) {
		// Left label: align right edge of text with right edge of handle
		x = handleTop.x + fHandleWidth - textWidth;
	} else {
		// Right label: align left edge of text with left edge of handle
		x = handleTop.x;
	}
	float y = handleTop.y - kLabelGap - fh.descent;
	
	SetHighUIColor(B_PANEL_TEXT_COLOR);
	SetLowUIColor(B_PANEL_BACKGROUND_COLOR);
	DrawString(text, BPoint(x, y));
}


void
TimeRangeSlider::_NotifyChanged()
{
	if (fModificationMessage == NULL)
		return;
	
	BMessage message(*fModificationMessage);
	message.AddInt64("from", FromTime());
	message.AddInt64("to", ToTime());
	message.AddFloat("leftValue", fLeftValue);
	message.AddFloat("rightValue", fRightValue);
	message.AddBool("fullRange", IsFullRange());
	
	BMessenger target(Window());
	target.SendMessage(&message);
}
