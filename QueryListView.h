/*
 * QueryListView.h - Custom BOutlineListView for query sidebar
 * Distributed under the terms of the MIT License.
 */

#ifndef QUERY_LIST_VIEW_H
#define QUERY_LIST_VIEW_H

#include <OutlineListView.h>

class BWindow;

// Custom BOutlineListView that handles right-clicks on queries
class QueryListView : public BOutlineListView {
public:
	QueryListView(const char* name, BWindow* target);
	virtual void MouseDown(BPoint point);
	virtual void MakeFocus(bool focus = true);
	virtual void FrameResized(float newWidth, float newHeight);

private:
	BWindow* fTarget;
};

#endif // QUERY_LIST_VIEW_H
