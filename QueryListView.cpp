/*
 * QueryListView.cpp - Custom BOutlineListView for query sidebar
 * Distributed under the terms of the MIT License.
 */

#include "QueryListView.h"
#include "QueryItem.h"
#include "EmailViews.h"

#include <Catalog.h>
#include <MenuItem.h>
#include <PopUpMenu.h>
#include <Window.h>

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "QueryListView"


QueryListView::QueryListView(const char* name, BWindow* target)
	:
	BOutlineListView(name, B_SINGLE_SELECTION_LIST),
	fTarget(target)
{
}


void
QueryListView::MouseDown(BPoint point)
{
	// Check if point is within our bounds
	if (!Bounds().Contains(point)) {
		return;
	}

	// Find item under mouse first
	int32 index = IndexOf(point);

	// If clicking on empty space, do nothing at all
	if (index < 0) {
		return;
	}

	uint32 buttons = 0;
	BMessage* currentMsg = Window()->CurrentMessage();
	if (currentMsg)
		currentMsg->FindInt32("buttons", (int32*)&buttons);

	// Check for secondary (right) mouse button
	if (buttons & B_SECONDARY_MOUSE_BUTTON) {
		BListItem* listItem = ItemAt(index);
		QueryItem* queryItem = dynamic_cast<QueryItem*>(listItem);

		// Skip context menu for "Unread emails" view (rarely has content worth backing up)
		if (queryItem && strcmp(queryItem->GetBaseName(), B_TRANSLATE("Unread emails")) == 0) {
			Select(index);
			return;
		}

		if (listItem) {
			// Select the item first
			Select(index);

			// Create context menu
			BPopUpMenu* menu = new BPopUpMenu("context", false, false);
			menu->SetAsyncAutoDestruct(true);

			// Add custom query specific items
			if (queryItem && queryItem->IsCustomQuery()) {
				menu->AddItem(new BMenuItem(B_TRANSLATE("Open query in Tracker" B_UTF8_ELLIPSIS),
					new BMessage(MSG_EDIT_QUERY)));
				menu->AddSeparatorItem();
				menu->AddItem(new BMenuItem(B_TRANSLATE("Remove query"), new BMessage(MSG_REMOVE_FILTER)));
			}

			// Set target for all items
			for (int32 i = 0; i < menu->CountItems(); i++) {
				BMenuItem* menuItem = menu->ItemAt(i);
				if (menuItem)
					menuItem->SetTarget(fTarget);
			}

			// Only show menu if it has items
			if (menu->CountItems() > 0) {
				// Convert point to screen coordinates
				BPoint screenPoint = point;
				ConvertToScreen(&screenPoint);

				// Show menu
				menu->Go(screenPoint, true, false, true);
			} else {
				delete menu;
			}
		}
		return;
	}

	// For left clicks on items, let the base class handle selection
	BOutlineListView::MouseDown(point);
}


void
QueryListView::MakeFocus(bool focus)
{
	BOutlineListView::MakeFocus(focus);
	// BOutlineListView doesn't repaint the selected item when focus changes,
	// so the selection highlight can look stale. Force a redraw.
	int32 selected = CurrentSelection();
	if (selected >= 0) {
		InvalidateItem(selected);
	}
}


void
QueryListView::FrameResized(float newWidth, float newHeight)
{
	BOutlineListView::FrameResized(newWidth, newHeight);
	// Separator items draw a horizontal line to a fixed right margin;
	// they need to redraw when the view width changes.
	for (int32 i = 0; i < CountItems(); i++) {
		SeparatorItem* separator = dynamic_cast<SeparatorItem*>(ItemAt(i));
		if (separator != NULL)
			InvalidateItem(i);
	}
}
