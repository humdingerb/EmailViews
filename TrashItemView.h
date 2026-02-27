/*
 * TrashItemView.h - Fixed trash item view at bottom of query pane
 * Distributed under the terms of the MIT License.
 *
 * Unlike sidebar queries which are BListItems in the QueryListView, the
 * trash view is a standalone BView pinned below the scrollable list.
 * This ensures it's always visible regardless of how many queries exist.
 * It disables itself (greyed out, ignores clicks) when the trash is empty.
 */

#ifndef TRASH_ITEM_VIEW_H
#define TRASH_ITEM_VIEW_H

#include <Handler.h>
#include <String.h>
#include <View.h>

class BBitmap;

// Message constants
const uint32 MSG_TRASH_SELECTED = 'trsh';
const uint32 MSG_EMPTY_TRASH = 'emtr';

class TrashItemView : public BView {
public:
	explicit TrashItemView(BHandler* target);
	virtual ~TrashItemView();

	virtual void Draw(BRect updateRect);
	virtual void MouseDown(BPoint where);
	virtual void MouseMoved(BPoint where, uint32 transit, const BMessage* dragMessage);

	void SetSelected(bool selected);
	bool IsSelected() const { return fSelected; }
	void SetCount(int32 count);

private:
	BHandler* fTarget;
	BBitmap* fIconEmpty;
	BBitmap* fIconFull;
	float fIconSize;
	BString fLabel;
	int32 fCount;
	bool fSelected;
	bool fMouseOver;
};

#endif // TRASH_ITEM_VIEW_H
