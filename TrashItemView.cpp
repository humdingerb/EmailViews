/*
 * TrashItemView.cpp - Fixed trash item view at bottom of query pane
 * Distributed under the terms of the MIT License.
 */

#include "TrashItemView.h"

#include <stdio.h>
#include <Application.h>
#include <Bitmap.h>
#include <Catalog.h>
#include <ControlLook.h>
#include <IconUtils.h>
#include <MenuItem.h>
#include <Messenger.h>
#include <PopUpMenu.h>
#include <Resources.h>
#include <Window.h>

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "TrashItemView"

TrashItemView::TrashItemView(BHandler* target)
	:
	BView("trashItem", B_WILL_DRAW | B_NAVIGABLE),
	fTarget(target),
	fIconEmpty(NULL),
	fIconFull(NULL),
	fLabel(B_TRANSLATE("Emails in Trash")),
	fCount(0),
	fSelected(false),
	fMouseOver(false)
{
	SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	// Size is controlled by the containing BScrollView

	// Derive icon size from font metrics — same size as the other built-in
	// sidebar query icons (ComposeIconSize(31)).
	fIconSize = (float)be_control_look->ComposeIconSize(31).width + 1;

	// Load trash icons from resources
	BResources* resources = be_app->AppResources();
	if (resources) {
		size_t size;
		BRect iconRect(0, 0, fIconSize - 1, fIconSize - 1);

		// Load empty trash icon
		const void* data = resources->LoadResource(B_VECTOR_ICON_TYPE, "MailTrashEmpty", &size);
		if (data && size > 0) {
			fIconEmpty = new BBitmap(iconRect, B_RGBA32);
			if (BIconUtils::GetVectorIcon((const uint8*)data, size, fIconEmpty) != B_OK) {
				delete fIconEmpty;
				fIconEmpty = NULL;
			}
		}

		// Load full trash icon
		data = resources->LoadResource(B_VECTOR_ICON_TYPE, "MailTrashFull", &size);
		if (data && size > 0) {
			fIconFull = new BBitmap(iconRect, B_RGBA32);
			if (BIconUtils::GetVectorIcon((const uint8*)data, size, fIconFull) != B_OK) {
				delete fIconFull;
				fIconFull = NULL;
			}
		}
	}
}


TrashItemView::~TrashItemView()
{
	delete fIconEmpty;
	delete fIconFull;
}


void
TrashItemView::Draw(BRect updateRect)
{
	BRect bounds = Bounds();

	rgb_color backgroundColor;
	rgb_color textColor;

	// Use panel background (slightly different from list background) so
	// the trash area is visually distinct from the scrollable query list
	rgb_color baseColor = tint_color(ui_color(B_PANEL_BACKGROUND_COLOR), B_NO_TINT);

	bool enabled = (fCount > 0);

	if (fSelected && enabled) {
		backgroundColor = ui_color(B_LIST_SELECTED_BACKGROUND_COLOR);
		textColor = ui_color(B_LIST_SELECTED_ITEM_TEXT_COLOR);
	} else if (fMouseOver && enabled) {
		backgroundColor = tint_color(baseColor, B_DARKEN_1_TINT);
		textColor = ui_color(B_LIST_ITEM_TEXT_COLOR);
	} else {
		backgroundColor = baseColor;
		if (enabled) {
			textColor = ui_color(B_LIST_ITEM_TEXT_COLOR);
		} else {
			// Disabled: use lighter text color
			textColor = tint_color(ui_color(B_LIST_ITEM_TEXT_COLOR), B_LIGHTEN_1_TINT);
		}
	}

	SetLowColor(backgroundColor);
	SetHighColor(backgroundColor);
	FillRect(bounds);

	// Draw icon (use full or empty based on count)
	BBitmap* icon = (fCount > 0) ? fIconFull : fIconEmpty;
	float spacing = be_control_look->DefaultLabelSpacing();
	float iconLeft = spacing * 2;
	if (icon) {
		float iconTop = (bounds.Height() - fIconSize) / 2;
		BRect iconRect(iconLeft, iconTop, iconLeft + fIconSize - 1, iconTop + fIconSize - 1);
		SetDrawingMode(B_OP_ALPHA);
		if (!enabled) {
			// Draw icon with reduced opacity when disabled
			SetBlendingMode(B_CONSTANT_ALPHA, B_ALPHA_OVERLAY);
			SetHighColor(0, 0, 0, 128);  // 50% opacity
		}
		DrawBitmap(icon, iconRect);
		SetDrawingMode(B_OP_COPY);
	}

	// Draw text
	SetHighColor(textColor);
	font_height fh;
	GetFontHeight(&fh);
	float textY = (bounds.Height() + fh.ascent - fh.descent) / 2;
	float textX = iconLeft + fIconSize + spacing;

	BString text = fLabel;
	if (fCount > 0) {
		text << " (" << fCount << ")";
	}
	DrawString(text.String(), BPoint(textX, textY));
}


void
TrashItemView::MouseDown(BPoint where)
{
	// Ignore clicks when trash is empty
	if (fCount == 0)
		return;

	// Check for right-click
	BMessage* currentMsg = Window()->CurrentMessage();
	int32 buttons = 0;
	currentMsg->FindInt32("buttons", &buttons);

	if (buttons & B_SECONDARY_MOUSE_BUTTON) {
		// Right-click: switch to trash view first, then show context menu
		if (!fSelected) {
			BMessage msg(MSG_TRASH_SELECTED);
			BMessenger(fTarget).SendMessage(&msg);
		}

		BPopUpMenu* menu = new BPopUpMenu("context", false, false);
		menu->SetAsyncAutoDestruct(true);
		menu->AddItem(new BMenuItem(B_TRANSLATE("Delete emails in Trash"), new BMessage(MSG_EMPTY_TRASH)));

		// Set target
		BMenuItem* item = menu->ItemAt(0);
		if (item)
			item->SetTarget(fTarget);

		// Convert to screen coordinates and show
		BPoint screenPoint = where;
		ConvertToScreen(&screenPoint);
		menu->Go(screenPoint, true, false, true);
	} else {
		// Left-click: select trash
		BMessage msg(MSG_TRASH_SELECTED);
		BMessenger(fTarget).SendMessage(&msg);
	}
}


void
TrashItemView::MouseMoved(BPoint where, uint32 transit, const BMessage* dragMessage)
{

	bool wasOver = fMouseOver;
	fMouseOver = (transit == B_INSIDE_VIEW || transit == B_ENTERED_VIEW);
	if (wasOver != fMouseOver)
		Invalidate();
}


void
TrashItemView::SetSelected(bool selected)
{
	if (fSelected != selected) {
		fSelected = selected;
		Invalidate();
	}
}


void
TrashItemView::SetCount(int32 count)
{
	if (fCount != count) {
		fCount = count;
		// Auto-deselect when trash becomes empty — the main window will
		// switch to the default view (All Emails) in response
		if (count == 0 && fSelected) {
			fSelected = false;
		}
		Invalidate();
	}
}
