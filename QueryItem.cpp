/*
 * QueryItem.cpp - List items for query sidebar
 * Distributed under the terms of the MIT License.
 *
 * Two item types live in the sidebar QueryListView:
 * - QueryItem: a selectable row with icon, label, and optional count badge.
 *   Built-in queries (All Emails, Unread, etc.) use system-provided icons;
 *   custom queries (user-saved filters) use the query icon.
 * - SeparatorItem: a thin, non-selectable horizontal line divider.
 */

#include "QueryItem.h"

#include <Bitmap.h>
#include <ControlLook.h>
#include <Font.h>
#include <View.h>


// QueryItem implementation

QueryItem::QueryItem(const char* name, const char* path, BBitmap* icon,
	bool isQuery, bool isCustomQuery, float iconSize)
	:
	BStringItem(name),
	fPath(path),
	fBaseName(name),
	fIcon(icon),
	fIconSize(iconSize),
	fIsQuery(isQuery),
	fIsCustomQuery(isCustomQuery)
{
}


QueryItem::~QueryItem()
{
	delete fIcon;
}


void
QueryItem::SetIcon(BBitmap* icon)
{
	if (icon == fIcon)
		return;
	delete fIcon;
	fIcon = icon;
}


void
QueryItem::Update(BView* owner, const BFont* font)
{
	// Call parent to do default width calculation
	BStringItem::Update(owner, font);

	// Use icon size plus padding for items with icons
	if (fIcon) {
		SetHeight(fIconSize + 4.0);  // Icon height + padding
	} else {
		// Calculate height based on font
		font_height fh;
		font->GetHeight(&fh);
		SetHeight(fh.ascent + fh.descent + fh.leading + 6);  // Font height + padding
	}
}


void
QueryItem::DrawItem(BView* owner, BRect frame, bool complete)
{
	rgb_color backgroundColor;
	rgb_color textColor;

	if (IsSelected()) {
		backgroundColor = ui_color(B_LIST_SELECTED_BACKGROUND_COLOR);
		textColor = ui_color(B_LIST_SELECTED_ITEM_TEXT_COLOR);
	} else {
		backgroundColor = ui_color(B_LIST_BACKGROUND_COLOR);
		textColor = ui_color(B_LIST_ITEM_TEXT_COLOR);
	}

	owner->SetLowColor(backgroundColor);
	owner->SetHighColor(backgroundColor);

	// Extend fill to the view's left edge (0) so the selection highlight
	// isn't inset by the BListView's default item margins
	BRect fillRect = frame;
	fillRect.left = 0;
	owner->FillRect(fillRect);

	// Draw icon if available (at fixed position from left edge)
	if (fIcon) {
		// Center the icon vertically in the frame
		float iconTop = frame.top + (frame.Height() - fIconSize) / 2;
		BRect iconRect(12, iconTop, 12 + fIconSize - 1, iconTop + fIconSize - 1);
		owner->SetDrawingMode(B_OP_ALPHA);
		owner->DrawBitmap(fIcon, iconRect);
		owner->SetDrawingMode(B_OP_COPY);
	}

	// Draw text at position after icon
	owner->SetHighColor(textColor);
	font_height fh;
	owner->GetFontHeight(&fh);
	float textY = frame.top + (frame.Height() + fh.ascent - fh.descent) / 2;
	BPoint textPoint(fIcon ? (12 + fIconSize + 4) : 12, textY);
	owner->DrawString(Text(), textPoint);
}


// SeparatorItem implementation

SeparatorItem::SeparatorItem()
	:
	BListItem()
{
	SetEnabled(false);  // Make it non-selectable
}


void
SeparatorItem::Update(BView* owner, const BFont* font)
{
	BListItem::Update(owner, font);
	SetHeight(8.0);  // Thin separator
}


void
SeparatorItem::DrawItem(BView* owner, BRect frame, bool complete)
{
	rgb_color backgroundColor = ui_color(B_LIST_BACKGROUND_COLOR);
	owner->SetLowColor(backgroundColor);
	owner->SetHighColor(backgroundColor);

	// Extend to fill from left edge
	BRect fillRect = frame;
	fillRect.left = 0;
	owner->FillRect(fillRect);

	// Draw a subtle horizontal line in the middle
	rgb_color lineColor = tint_color(backgroundColor, B_DARKEN_1_TINT);
	owner->SetHighColor(lineColor);
	float middle = frame.top + frame.Height() / 2;
	owner->StrokeLine(BPoint(12, middle), BPoint(frame.right - 12, middle));
}
