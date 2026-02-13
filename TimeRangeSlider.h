/*
 * TimeRangeSlider.h - Dual-handle slider for selecting a time range
 * Distributed under the terms of the MIT License.
 */

#ifndef TIME_RANGE_SLIDER_H
#define TIME_RANGE_SLIDER_H

#include <View.h>
#include <String.h>

// Message sent when range changes (on mouse-up)
const uint32 MSG_TIME_RANGE_CHANGED = 'trch';

class TimeRangeSlider : public BView {
public:
	explicit			TimeRangeSlider(const char* name, BMessage* message = NULL);
	virtual				~TimeRangeSlider() override;

	// BView overrides
	virtual void		AttachedToWindow() override;
	virtual void		Draw(BRect updateRect) override;
	virtual void		MouseDown(BPoint where) override;
	virtual void		MouseMoved(BPoint where, uint32 transit, const BMessage* message) override;
	virtual void		MouseUp(BPoint where) override;
	virtual void		GetPreferredSize(float* width, float* height) override;
	virtual BSize		MinSize() override;
	virtual BSize		PreferredSize() override;
	virtual BSize		MaxSize() override;

	// Set the time range boundaries
	void				SetTimeRange(time_t oldest, time_t newest);
	
	// Get/set the selected range (as timestamps)
	time_t				FromTime() const;
	time_t				ToTime() const;
	void				SetFromTime(time_t from);
	void				SetToTime(time_t to);
	
	// Get/set handle positions (0.0 = Now, 1.0 = Oldest)
	float				LeftValue() const { return fLeftValue; }
	float				RightValue() const { return fRightValue; }
	void				SetLeftValue(float value);
	void				SetRightValue(float value);
	
	// Check if full range is selected (no filtering needed)
	bool				IsFullRange() const;
	
	// Check if left handle is at "Now" or "Today" (no upper bound needed)
	bool				IsLeftAtTodayOrNow() const;
	
	// Set the modification message
	void				SetModificationMessage(BMessage* message);
	
	// Get the height of the label area above the slider track
	float				LabelAreaHeight() const;

private:
	// Coordinate conversion
	float				_ValueToX(float value) const;
	float				_XToValue(float x) const;
	BRect				_TrackRect() const;
	BRect				_LeftHandleRect() const;
	BRect				_RightHandleRect() const;
	
	// Time conversion (non-linear mapping)
	time_t				_ValueToTime(float value) const;
	float				_TimeToValue(time_t time) const;
	BString				_FormatTimeLabel(float value) const;
	
	// Snap value to nearest discrete step
	float				_SnapToStep(float value) const;
	float				_NextStepAfter(float value) const;
	float				_PrevStepBefore(float value) const;
	
	// Drawing helpers
	void				_DrawTrack(BRect rect);
	void				_DrawHandle(BRect rect, bool isLeft, bool isHovered, bool isDragging);
	void				_DrawLabel(const char* text, BPoint handleTop, bool isLeft);
	
	// Notify target of change
	void				_NotifyChanged();

	// Handle positions (0.0 = left/newest, 1.0 = right/oldest)
	// Note: When left handle is at 0.0, IsLeftAtTodayOrNow() returns true and
	// filtering is skipped entirely, so 0.0 effectively means "Today" (no upper bound)
	float				fLeftValue;
	float				fRightValue;
	
	// Time boundaries
	time_t				fNewestTime;	// Current time (but see note above about filtering)
	time_t				fOldestTime;	// Oldest email timestamp
	
	// Interaction state
	enum {
		kDragNone = 0,
		kDragLeft,
		kDragRight
	};
	int					fDragging;
	int					fHoveredHandle;	// 0 = none, kDragLeft or kDragRight
	float				fDragOffset;	// Offset from handle center when drag started
	
	// Message to send on change
	BMessage*			fModificationMessage;
	
	// Cached dimensions
	float				fHandleWidth;
	float				fHandleHeight;
	float				fTrackHeight;
	float				fLabelHeight;
};

#endif // TIME_RANGE_SLIDER_H
