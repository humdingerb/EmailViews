/*
 * EmailColumnHeader.cpp - Column header view implementation
 * Distributed under the terms of the MIT License.
 */

#include "EmailColumnHeader.h"
#include "EmailListView.h"

#include <Catalog.h>
#include <ControlLook.h>
#include <MenuItem.h>
#include <Message.h>
#include <PopUpMenu.h>
#include <Window.h>

#include <algorithm>
#include <cstdio>

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "EmailColumnHeader"


EmailColumnHeaderView::EmailColumnHeaderView(const char* name)
    :
    BView(name, B_WILL_DRAW | B_FRAME_EVENTS | B_FULL_UPDATE_ON_RESIZE),
    fColumns(kColumnCount),
    fHeaderHeight(20.0f),
    fSortColumn(kColumnDate),
    fSortOrder(kSortDescending),
    fPressedColumn(-1),
    fHoverColumn(-1),
    fResizing(false),
    fResizeColumn(-1),
    fResizeStartX(0),
    fResizeStartWidth(0),
    fDragging(false),
    fDragColumn(-1),
    fDragStartX(0),
    fDragCurrentX(0),
    fDragColumnWidth(0),
    fInitialWidthsSet(false),
    fStateRestored(false),
    fListView(NULL)
{
    // Set up columns with default proportions
    // Icon columns have fixed width (proportion 0), text columns share remaining space
    // | Status (5%) | ★ (fixed 24) | 📎 (fixed 24) | From (15%) | To (15%) | Subject (35%) | Date (15%) | Account (15%) |
    fColumns.AddItem(new ColumnInfo(kColumnStatus, "Status", 0.05f, 25.0f, true));
    fColumns.AddItem(new ColumnInfo(kColumnStar, "", 0.0f, 24.0f, true, true));
    fColumns.AddItem(new ColumnInfo(kColumnAttachment, "", 0.0f, 24.0f, true, true));
    fColumns.AddItem(new ColumnInfo(kColumnFrom, "From", 0.15f, 60.0f, true));
    fColumns.AddItem(new ColumnInfo(kColumnTo, "To", 0.15f, 60.0f, true));
    fColumns.AddItem(new ColumnInfo(kColumnSubject, "Subject", 0.35f, 80.0f, true));
    fColumns.AddItem(new ColumnInfo(kColumnDate, "Date", 0.15f, 80.0f, true));
    fColumns.AddItem(new ColumnInfo(kColumnAccount, "Account", 0.15f, 60.0f, true));
}


EmailColumnHeaderView::~EmailColumnHeaderView()
{
}


void
EmailColumnHeaderView::AttachedToWindow()
{
    BView::AttachedToWindow();
    
    // Calculate header height based on font
    font_height fontHeight;
    GetFontHeight(&fontHeight);
    fHeaderHeight = ceilf(fontHeight.ascent + fontHeight.descent + fontHeight.leading + 8.0f);
    
    // Set view color
    SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
    
    // Calculate initial column widths (unless state was restored)
    if (!fStateRestored)
        _RecalculateWidths();
}


void
EmailColumnHeaderView::Draw(BRect updateRect)
{
    BRect bounds = Bounds();
    
    // Draw each column header
    float columnsEndX = 0;
    for (int32 i = 0; i < fColumns.CountItems(); i++) {
        ColumnInfo* info = fColumns.ItemAt(i);
        if (info == NULL || !info->visible)
            continue;
        BRect columnRect = _ColumnRect(i);
        columnsEndX = columnRect.right + 1;
        if (columnRect.Intersects(updateRect)) {
            bool pressed = (i == fPressedColumn) && !fDragging;
            _DrawColumn(i, columnRect, pressed);
        }
    }
    
    // Draw empty header area to the right of the last column
    if (columnsEndX < bounds.right) {
        BRect emptyRect(columnsEndX, bounds.top, bounds.right, bounds.bottom);
        if (emptyRect.Intersects(updateRect)) {
            rgb_color base = ui_color(B_PANEL_BACKGROUND_COLOR);
            be_control_look->DrawButtonBackground(this, emptyRect, emptyRect, base, 0,
                                                  BControlLook::B_TOP_BORDER |
                                                  BControlLook::B_BOTTOM_BORDER);
        }
    }
    
    // Draw bottom border
    SetHighColor(tint_color(ui_color(B_PANEL_BACKGROUND_COLOR), B_DARKEN_2_TINT));
    StrokeLine(BPoint(bounds.left, bounds.bottom),
               BPoint(bounds.right, bounds.bottom));
    
    // Draw floating dragged column on top
    if (fDragging) {
        _DrawDraggedColumn();
    }
}


void
EmailColumnHeaderView::_DrawColumn(int32 index, BRect rect, bool pressed)
{
    ColumnInfo* column = fColumns.ItemAt(index);
    if (column == NULL)
        return;
    
    // Use BControlLook for native appearance
    rgb_color base = ui_color(B_PANEL_BACKGROUND_COLOR);
    uint32 flags = 0;
    
    if (pressed)
        flags |= BControlLook::B_ACTIVATED;
    
    // Draw header button background
    be_control_look->DrawButtonBackground(this, rect, rect, base, flags,
                                          BControlLook::B_TOP_BORDER |
                                          BControlLook::B_BOTTOM_BORDER);
    
    // Draw column separator on right edge
    SetHighColor(tint_color(base, B_DARKEN_1_TINT));
    StrokeLine(BPoint(rect.right, rect.top + 2),
               BPoint(rect.right, rect.bottom - 2));
    
    // Icon columns: draw header icon centered, no title text or sort indicator
    if (column->isIcon) {
        if (column->headerIcon != NULL) {
            float iconW = column->headerIcon->Bounds().Width() + 1;
            float iconH = column->headerIcon->Bounds().Height() + 1;
            float iconX = rect.left + (rect.Width() - iconW) / 2.0f;
            float iconY = rect.top + (rect.Height() - iconH) / 2.0f;
            SetDrawingMode(B_OP_ALPHA);
            SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);
            DrawBitmap(column->headerIcon, BPoint(iconX, iconY));
            SetDrawingMode(B_OP_COPY);
        }
        return;
    }
    
    // Draw title text
    font_height fontHeight;
    GetFontHeight(&fontHeight);
    
    float textY = rect.top + (rect.Height() - fontHeight.ascent - fontHeight.descent) / 2.0f
                  + fontHeight.ascent;
    
    SetHighColor(ui_color(B_PANEL_TEXT_COLOR));
    SetLowColor(base);
    
    float padding = 5.0f;
    BString title = column->title;
    
    // Reserve space for sort indicator if this is the sort column
    float sortIndicatorSpace = 0.0f;
    if (column->id == fSortColumn) {
        sortIndicatorSpace = 12.0f;
    }
    
    // Truncate title if needed
    float availableWidth = rect.Width() - padding * 2 - sortIndicatorSpace;
    TruncateString(&title, B_TRUNCATE_END, availableWidth);
    
    MovePenTo(rect.left + padding, textY);
    DrawString(title.String());
    
    // Draw sort indicator
    if (column->id == fSortColumn) {
        float indicatorX = rect.right - padding - 8.0f;
        float indicatorY = rect.top + rect.Height() / 2.0f;
        
        SetHighColor(ui_color(B_PANEL_TEXT_COLOR));
        
        if (fSortOrder == kSortAscending) {
            // Up arrow
            FillTriangle(BPoint(indicatorX, indicatorY + 3),
                        BPoint(indicatorX + 6, indicatorY + 3),
                        BPoint(indicatorX + 3, indicatorY - 3));
        } else if (fSortOrder == kSortDescending) {
            // Down arrow
            FillTriangle(BPoint(indicatorX, indicatorY - 3),
                        BPoint(indicatorX + 6, indicatorY - 3),
                        BPoint(indicatorX + 3, indicatorY + 3));
        }
    }
}


void
EmailColumnHeaderView::MouseDown(BPoint where)
{
    // Check for right-click - show column visibility menu
    BMessage* currentMsg = Window()->CurrentMessage();
    int32 buttons = 0;
    currentMsg->FindInt32("buttons", &buttons);
    
    if (buttons & B_SECONDARY_MOUSE_BUTTON) {
        _ShowColumnContextMenu(where);
        return;
    }
    
    // Check if clicking on a resize border first
    int32 resizeColumn = _ResizeBorderAt(where.x);
    if (resizeColumn >= 0) {
        // Start resizing
        fResizing = true;
        fResizeColumn = resizeColumn;
        fResizeStartX = where.x;
        ColumnInfo* info = fColumns.ItemAt(resizeColumn);
        if (info != NULL) {
            fResizeStartWidth = info->width;
        }
        SetMouseEventMask(B_POINTER_EVENTS, B_LOCK_WINDOW_FOCUS);
        return;
    }
    
    // Otherwise, handle column click (potential drag or sort)
    int32 column = _ColumnAt(where.x);
    if (column >= 0) {
        ColumnInfo* info = fColumns.ItemAt(column);
        if (info != NULL) {
            fPressedColumn = column;
            fDragStartX = where.x;
            fDragColumn = column;
            SetMouseEventMask(B_POINTER_EVENTS, B_LOCK_WINDOW_FOCUS);
            Invalidate(_ColumnRect(column));
        }
    }
}


void
EmailColumnHeaderView::MouseMoved(BPoint where, uint32 transit,
                                   const BMessage* dragMessage)
{
    if (fResizing) {
        // Handle resize drag
        float delta = where.x - fResizeStartX;
        float newWidth = fResizeStartWidth + delta;
        
        ColumnInfo* info = fColumns.ItemAt(fResizeColumn);
        if (info != NULL) {
            // Enforce minimum width
            newWidth = std::max(newWidth, info->minWidth);
            
            // Update column width
            info->width = newWidth;
            
            // Notify list view to update (this also updates scrollbars)
            if (fListView != NULL) {
                fListView->ColumnsResized();
            }
            
            Invalidate();
        }
        return;
    }
    
    if (fDragging) {
        // Update floating column position
        fDragCurrentX = where.x;
        
        // Check if we should swap with neighbor
        // Check left neighbor
        if (fDragColumn > 0) {
            BRect leftRect = _ColumnRect(fDragColumn - 1);
            float leftMid = leftRect.left + leftRect.Width() / 2;
            if (where.x < leftMid) {
                _SwapColumns(fDragColumn, fDragColumn - 1);
                fDragColumn--;
            }
        }
        
        // Check right neighbor
        if (fDragColumn < fColumns.CountItems() - 1) {
            BRect rightRect = _ColumnRect(fDragColumn + 1);
            float rightMid = rightRect.left + rightRect.Width() / 2;
            if (where.x > rightMid) {
                _SwapColumns(fDragColumn, fDragColumn + 1);
                fDragColumn++;
            }
        }
        
        Invalidate();
        return;
    }
    
    // Check if we should start dragging (mouse moved beyond threshold while pressed)
    if (fDragColumn >= 0 && !fDragging) {
        float distance = fabs(where.x - fDragStartX);
        if (distance >= kDragThreshold) {
            // Start dragging
            fDragging = true;
            fPressedColumn = -1;  // No longer "pressed" visually
            fDragCurrentX = where.x;
            
            ColumnInfo* info = fColumns.ItemAt(fDragColumn);
            if (info != NULL) {
                fDragColumnWidth = info->width;
            }
            
            Invalidate();
            return;
        }
    }
    
    // Check if over a resize border and update cursor
    int32 resizeColumn = _ResizeBorderAt(where.x);
    _SetResizeCursor(resizeColumn >= 0);
    
    if (fPressedColumn >= 0) {
        int32 column = _ColumnAt(where.x);
        // While pressed, track if still over the pressed column
        if (column != fPressedColumn) {
            // Moved outside - "unpress" visually but keep tracking
            int32 oldPressed = fPressedColumn;
            fPressedColumn = -1;
            Invalidate(_ColumnRect(oldPressed));
        }
    }
    
    // Update hover state
    int32 column = _ColumnAt(where.x);
    if (column != fHoverColumn) {
        fHoverColumn = column;
        // Could add hover highlighting here
    }
}


void
EmailColumnHeaderView::MouseUp(BPoint where)
{
    if (fResizing) {
        // End resize operation
        fResizing = false;
        fResizeColumn = -1;
        _SetResizeCursor(false);
        
        // Notify that column was resized
        if (Window() != NULL) {
            BMessage msg(kMsgColumnResized);
            Window()->PostMessage(&msg, Window());
        }
        return;
    }
    
    if (fDragging) {
        // End drag operation - reordering already happened during drag
        fDragging = false;
        fDragColumn = -1;
        Invalidate();
        return;
    }
    
    int32 column = _ColumnAt(where.x);
    
    // Only trigger sort if we didn't drag (fDragColumn is still set if we started tracking)
    if (fPressedColumn >= 0 && column == fPressedColumn && fDragColumn >= 0) {
        // Click completed on the same column (no drag occurred)
        ColumnInfo* info = fColumns.ItemAt(column);
        if (info != NULL && info->sortable) {
            // Toggle sort order if same column, otherwise set new column
            if (info->id == fSortColumn) {
                // Toggle between ascending and descending
                fSortOrder = (fSortOrder == kSortAscending) 
                             ? kSortDescending : kSortAscending;
            } else {
                fSortColumn = info->id;
                fSortOrder = kSortDescending;  // Default to descending for new column
            }
            
            // Notify listener
            BMessage msg(kMsgColumnClicked);
            msg.AddInt32("column", info->id);
            msg.AddInt32("order", fSortOrder);
            
            if (fListView != NULL && fListView->Looper() != NULL)
                fListView->Looper()->PostMessage(&msg, fListView);
            
            printf("Column clicked: %s, order: %s\n", 
                   info->title.String(),
                   fSortOrder == kSortAscending ? "ascending" : "descending");
            
            Invalidate();
        }
    }
    
    if (fPressedColumn >= 0) {
        int32 oldPressed = fPressedColumn;
        fPressedColumn = -1;
        Invalidate(_ColumnRect(oldPressed));
    }
    
    // Reset drag tracking
    fDragColumn = -1;
}


void
EmailColumnHeaderView::FrameResized(float width, float height)
{
    BView::FrameResized(width, height);
    
    // Set initial column widths on first resize (when we have actual dimensions)
    if (!fInitialWidthsSet && width > 0) {
        if (!fStateRestored)
            _RecalculateWidths();
        fInitialWidthsSet = true;
    }
    
    // Update list view's scrollbars since view width changed
    if (fListView != NULL) {
        fListView->ColumnsResized();
    }
    
    Invalidate();
}


void
EmailColumnHeaderView::GetPreferredSize(float* _width, float* _height)
{
    if (_width != NULL)
        *_width = 400.0f;  // Reasonable default
    if (_height != NULL)
        *_height = fHeaderHeight;
}


BSize
EmailColumnHeaderView::MinSize()
{
    return BSize(100.0f, fHeaderHeight);
}


BSize
EmailColumnHeaderView::MaxSize()
{
    return BSize(B_SIZE_UNLIMITED, fHeaderHeight);
}


void
EmailColumnHeaderView::SetColumnWidth(EmailColumn column, float width)
{
    for (int32 i = 0; i < fColumns.CountItems(); i++) {
        ColumnInfo* info = fColumns.ItemAt(i);
        if (info != NULL && info->id == column) {
            info->width = std::max(width, info->minWidth);
            Invalidate();
            break;
        }
    }
}


float
EmailColumnHeaderView::ColumnWidth(EmailColumn column) const
{
    for (int32 i = 0; i < fColumns.CountItems(); i++) {
        ColumnInfo* info = fColumns.ItemAt(i);
        if (info != NULL && info->id == column)
            return info->width;
    }
    return 0.0f;
}


float
EmailColumnHeaderView::ColumnPosition(EmailColumn column) const
{
    float x = 0;
    for (int32 i = 0; i < fColumns.CountItems(); i++) {
        ColumnInfo* info = fColumns.ItemAt(i);
        if (info == NULL || !info->visible)
            continue;
        if (info->id == column)
            return x;
        x += info->width;
    }
    return 0.0f;
}


float
EmailColumnHeaderView::TotalWidth() const
{
    float total = 0;
    for (int32 i = 0; i < fColumns.CountItems(); i++) {
        ColumnInfo* info = fColumns.ItemAt(i);
        if (info != NULL && info->visible)
            total += info->width;
    }
    return total;
}


void
EmailColumnHeaderView::SetColumnHeaderIcon(EmailColumn column, BBitmap* icon)
{
    for (int32 i = 0; i < fColumns.CountItems(); i++) {
        ColumnInfo* info = fColumns.ItemAt(i);
        if (info != NULL && info->id == column) {
            info->headerIcon = icon;
            Invalidate(_ColumnRect(i));
            break;
        }
    }
}


void
EmailColumnHeaderView::ScrollToH(float x)
{
    // Scroll the header view horizontally to match the list
    ScrollTo(BPoint(x, 0));
}


void
EmailColumnHeaderView::SetSortColumn(EmailColumn column, enum SortOrder order)
{
    fSortColumn = column;
    fSortOrder = order;
    Invalidate();
}


void
EmailColumnHeaderView::SetListView(EmailListView* listView)
{
    fListView = listView;
}


status_t
EmailColumnHeaderView::SaveState(BMessage* into) const
{
    if (into == NULL)
        return B_BAD_VALUE;

    // Save column order, widths, and visibility
    for (int32 i = 0; i < fColumns.CountItems(); i++) {
        ColumnInfo* info = fColumns.ItemAt(i);
        if (info == NULL)
            continue;
        into->AddInt32("column_id", (int32)info->id);
        into->AddFloat("column_width", info->width);
        into->AddBool("column_visible", info->visible);
    }

    // Save sort state
    into->AddInt32("sort_column", (int32)fSortColumn);
    into->AddInt32("sort_order", (int32)fSortOrder);

    return B_OK;
}


status_t
EmailColumnHeaderView::RestoreState(const BMessage* from)
{
    if (from == NULL)
        return B_BAD_VALUE;

    // Restore column order and widths
    int32 count = 0;
    type_code type;
    from->GetInfo("column_id", &type, &count);

    if (count > 0 && count <= (int32)kColumnCount) {
        // Build new column order from saved state
        BObjectList<ColumnInfo, false> newOrder(count);

        for (int32 i = 0; i < count; i++) {
            int32 colId;
            float width;
            if (from->FindInt32("column_id", i, &colId) != B_OK ||
                from->FindFloat("column_width", i, &width) != B_OK)
                continue;

            // Visibility is optional (backwards compatible with old settings)
            bool visible = true;
            if (from->FindBool("column_visible", i, &visible) != B_OK)
                visible = true;

            // Find this column in current list
            for (int32 j = 0; j < fColumns.CountItems(); j++) {
                ColumnInfo* info = fColumns.ItemAt(j);
                if (info != NULL && (int32)info->id == colId) {
                    if (!info->isIcon)
                        info->width = width;
                    info->visible = visible;
                    newOrder.AddItem(info);
                    break;
                }
            }
        }

        // If we found all columns, apply the new order
        if (newOrder.CountItems() == fColumns.CountItems()) {
            // Detach items from fColumns without deleting (owning list)
            // then re-add in new order
            BObjectList<ColumnInfo, false> temp(fColumns.CountItems());
            for (int32 i = 0; i < fColumns.CountItems(); i++)
                temp.AddItem(fColumns.ItemAt(i));

            fColumns.MakeEmpty(false);  // Don't delete items
            for (int32 i = 0; i < newOrder.CountItems(); i++)
                fColumns.AddItem(newOrder.ItemAt(i));
        }
    }

    // Restore sort state
    int32 sortCol, sortOrd;
    if (from->FindInt32("sort_column", &sortCol) == B_OK &&
        from->FindInt32("sort_order", &sortOrd) == B_OK) {
        fSortColumn = (EmailColumn)sortCol;
        fSortOrder = (enum SortOrder)sortOrd;
    }

    // Safety: if no columns are visible (e.g. old settings without visibility data),
    // make all columns visible
    bool anyVisible = false;
    for (int32 i = 0; i < fColumns.CountItems(); i++) {
        ColumnInfo* info = fColumns.ItemAt(i);
        if (info != NULL && info->visible) {
            anyVisible = true;
            break;
        }
    }
    if (!anyVisible) {
        for (int32 i = 0; i < fColumns.CountItems(); i++) {
            ColumnInfo* info = fColumns.ItemAt(i);
            if (info != NULL)
                info->visible = true;
        }
    }

    fStateRestored = true;
    Invalidate();
    return B_OK;
}


void
EmailColumnHeaderView::_RecalculateWidths()
{
    float totalWidth = Bounds().Width();
    
    // Two-pass layout: icon columns (Status, Star, Attachment) get fixed
    // pixel widths; text columns (From, Subject, Date, etc.) share the
    // remaining space proportionally. This ensures icons never stretch
    // while text columns scale with window size.
    float fixedWidth = 0;
    float totalProportion = 0;
    for (int32 i = 0; i < fColumns.CountItems(); i++) {
        ColumnInfo* info = fColumns.ItemAt(i);
        if (info == NULL || !info->visible)
            continue;
        if (info->isIcon) {
            info->width = info->minWidth;
            fixedWidth += info->width;
        } else {
            totalProportion += info->proportion;
        }
    }
    
    // Second pass: distribute remaining width to visible proportional columns
    float availableWidth = totalWidth - fixedWidth;
    if (availableWidth < 0)
        availableWidth = 0;
    
    for (int32 i = 0; i < fColumns.CountItems(); i++) {
        ColumnInfo* info = fColumns.ItemAt(i);
        if (info == NULL || !info->visible || info->isIcon)
            continue;
        if (totalProportion > 0) {
            info->width = std::max(availableWidth * info->proportion / totalProportion,
                                   info->minWidth);
        } else {
            info->width = info->minWidth;
        }
    }
}


int32
EmailColumnHeaderView::_ColumnAt(float x) const
{
    float pos = 0;
    for (int32 i = 0; i < fColumns.CountItems(); i++) {
        ColumnInfo* info = fColumns.ItemAt(i);
        if (info == NULL || !info->visible)
            continue;
        if (x >= pos && x < pos + info->width)
            return i;
        pos += info->width;
    }
    return -1;
}


BRect
EmailColumnHeaderView::_ColumnRect(int32 index) const
{
    BRect bounds = Bounds();
    float x = 0;
    
    for (int32 i = 0; i < index && i < fColumns.CountItems(); i++) {
        ColumnInfo* info = fColumns.ItemAt(i);
        if (info != NULL && info->visible)
            x += info->width;
    }
    
    ColumnInfo* info = fColumns.ItemAt(index);
    if (info != NULL && info->visible) {
        return BRect(x, bounds.top, x + info->width - 1, bounds.bottom);
    }
    
    return BRect();
}


int32
EmailColumnHeaderView::_ResizeBorderAt(float x) const
{
    // Check if x is within kResizeZone pixels of a column border
    // Returns the index of the column to the LEFT of the border, or -1
    float pos = 0;
    for (int32 i = 0; i < fColumns.CountItems(); i++) {
        ColumnInfo* info = fColumns.ItemAt(i);
        if (info == NULL || !info->visible)
            continue;
        pos += info->width;
        
        // Check if within resize zone of this border
        if (x >= pos - kResizeZone && x <= pos + kResizeZone) {
            // Don't allow resizing icon columns (they have fixed width)
            if (info->isIcon)
                continue;
            return i;
        }
    }
    return -1;
}


void
EmailColumnHeaderView::_DrawDraggedColumn()
{
    if (fDragColumn < 0)
        return;
    
    ColumnInfo* info = fColumns.ItemAt(fDragColumn);
    if (info == NULL)
        return;
    
    BRect bounds = Bounds();
    
    // Calculate floating rect centered on mouse X
    float halfWidth = fDragColumnWidth / 2;
    BRect floatRect(fDragCurrentX - halfWidth, bounds.top + 1,
                    fDragCurrentX + halfWidth, bounds.bottom - 1);
    
    // Draw semi-transparent background
    rgb_color fillColor = tint_color(ui_color(B_PANEL_BACKGROUND_COLOR), B_LIGHTEN_1_TINT);
    fillColor.alpha = 200;
    SetHighColor(fillColor);
    SetDrawingMode(B_OP_ALPHA);
    FillRect(floatRect);
    SetDrawingMode(B_OP_COPY);
    
    // Draw border using keyboard navigation color (standard for drag feedback)
    SetHighColor(ui_color(B_KEYBOARD_NAVIGATION_COLOR));
    SetPenSize(2.0f);
    StrokeRect(floatRect);
    SetPenSize(1.0f);
    
    // Draw column title
    SetHighColor(ui_color(B_PANEL_TEXT_COLOR));
    font_height fontHeight;
    GetFontHeight(&fontHeight);
    float textY = floatRect.top + (floatRect.Height() - fontHeight.ascent - fontHeight.descent) / 2.0f
                  + fontHeight.ascent;
    float textX = floatRect.left + 8.0f;
    
    BString title(info->title);
    TruncateString(&title, B_TRUNCATE_END, floatRect.Width() - 16.0f);
    MovePenTo(textX, textY);
    DrawString(title.String());
}


void
EmailColumnHeaderView::_SwapColumns(int32 fromIndex, int32 toIndex)
{
    if (fromIndex < 0 || fromIndex >= fColumns.CountItems())
        return;
    if (toIndex < 0 || toIndex >= fColumns.CountItems())
        return;
    if (fromIndex == toIndex)
        return;
    
    // Swap by removing and reinserting
    ColumnInfo* item = fColumns.RemoveItemAt(fromIndex);
    if (item != NULL) {
        fColumns.AddItem(item, toIndex);
        
        // Update list view
        if (fListView != NULL) {
            fListView->ColumnsResized();
        }
        
        // Notify that columns were reordered
        if (Window() != NULL) {
            BMessage msg(kMsgColumnReordered);
            Window()->PostMessage(&msg, Window());
        }
    }
}


void
EmailColumnHeaderView::_SetResizeCursor(bool resize)
{
    if (resize) {
        // Use system horizontal resize cursor
        BCursor cursor(B_CURSOR_ID_RESIZE_EAST_WEST);
        SetViewCursor(&cursor);
    } else {
        // Restore default cursor
        BCursor cursor(B_CURSOR_ID_SYSTEM_DEFAULT);
        SetViewCursor(&cursor);
    }
}


void
EmailColumnHeaderView::GetColumnWidths(float* status, float* from,
                                        float* subject, float* date) const
{
    for (int32 i = 0; i < fColumns.CountItems(); i++) {
        ColumnInfo* info = fColumns.ItemAt(i);
        if (info == NULL)
            continue;
        
        switch (info->id) {
            case kColumnStatus:
                if (status != NULL) *status = info->width;
                break;
            case kColumnFrom:
                if (from != NULL) *from = info->width;
                break;
            case kColumnSubject:
                if (subject != NULL) *subject = info->width;
                break;
            case kColumnDate:
                if (date != NULL) *date = info->width;
                break;
            default:
                break;
        }
    }
}


EmailColumn
EmailColumnHeaderView::ColumnAt(int32 index) const
{
    ColumnInfo* info = fColumns.ItemAt(index);
    if (info != NULL)
        return info->id;
    return kColumnStatus;  // Fallback
}


float
EmailColumnHeaderView::ColumnWidthAt(int32 index) const
{
    ColumnInfo* info = fColumns.ItemAt(index);
    if (info != NULL && info->visible)
        return info->width;
    return 0;
}


void
EmailColumnHeaderView::SetColumnVisible(EmailColumn column, bool visible)
{
    for (int32 i = 0; i < fColumns.CountItems(); i++) {
        ColumnInfo* info = fColumns.ItemAt(i);
        if (info != NULL && info->id == column) {
            if (info->visible != visible) {
                info->visible = visible;
                Invalidate();
                
                // Notify list view to update
                if (fListView != NULL)
                    fListView->ColumnsResized();
            }
            return;
        }
    }
}


bool
EmailColumnHeaderView::IsColumnVisible(EmailColumn column) const
{
    for (int32 i = 0; i < fColumns.CountItems(); i++) {
        ColumnInfo* info = fColumns.ItemAt(i);
        if (info != NULL && info->id == column)
            return info->visible;
    }
    return false;
}


int32
EmailColumnHeaderView::CountVisibleColumns() const
{
    int32 count = 0;
    for (int32 i = 0; i < fColumns.CountItems(); i++) {
        ColumnInfo* info = fColumns.ItemAt(i);
        if (info != NULL && info->visible)
            count++;
    }
    return count;
}


void
EmailColumnHeaderView::_ShowColumnContextMenu(BPoint where)
{
    BPopUpMenu* menu = new BPopUpMenu("ColumnMenu", false, false);
    
    // Internal message for toggling column visibility
    static const uint32 kMsgToggleColumn = 'tgcl';
    
    // Count visible columns to enforce minimum of one
    int32 visibleCount = CountVisibleColumns();
    
    // Add a checkmark item for each column in fixed order (not affected by
    // per-view column reordering — consistent UI regardless of column arrangement)
    static const EmailColumn kMenuOrder[] = {
        kColumnStatus, kColumnStar, kColumnAttachment,
        kColumnFrom, kColumnTo, kColumnSubject,
        kColumnDate, kColumnAccount
    };
    
    for (int32 m = 0; m < (int32)(sizeof(kMenuOrder) / sizeof(kMenuOrder[0])); m++) {
        EmailColumn colId = kMenuOrder[m];
        
        // Find this column's info
        ColumnInfo* info = NULL;
        for (int32 j = 0; j < fColumns.CountItems(); j++) {
            ColumnInfo* candidate = fColumns.ItemAt(j);
            if (candidate != NULL && candidate->id == colId) {
                info = candidate;
                break;
            }
        }
        if (info == NULL)
            continue;
        
        // Build display name for the column
        BString label;
        switch (info->id) {
            case kColumnStatus:
                label = B_TRANSLATE("Status");
                break;
            case kColumnStar:
                label = B_TRANSLATE("Star");
                break;
            case kColumnAttachment:
                label = B_TRANSLATE("Attachment");
                break;
            case kColumnFrom:
                label = B_TRANSLATE("From");
                break;
            case kColumnTo:
                label = B_TRANSLATE("To");
                break;
            case kColumnSubject:
                label = B_TRANSLATE("Subject");
                break;
            case kColumnDate:
                label = B_TRANSLATE("Date");
                break;
            case kColumnAccount:
                label = B_TRANSLATE("Account");
                break;
            default:
                continue;
        }
        
        BMessage* msg = new BMessage(kMsgToggleColumn);
        msg->AddInt32("column_id", (int32)info->id);
        BMenuItem* item = new BMenuItem(label.String(), msg);
        item->SetMarked(info->visible);
        
        // Don't allow hiding the last visible column
        if (info->visible && visibleCount <= 1)
            item->SetEnabled(false);
        
        menu->AddItem(item);
    }
    
    // Show the menu synchronously and handle result
    ConvertToScreen(&where);
    BMenuItem* selected = menu->Go(where, false, true, false);
    
    if (selected != NULL) {
        BMessage* msg = selected->Message();
        int32 colId;
        if (msg != NULL && msg->FindInt32("column_id", &colId) == B_OK) {
            for (int32 i = 0; i < fColumns.CountItems(); i++) {
                ColumnInfo* info = fColumns.ItemAt(i);
                if (info != NULL && (int32)info->id == colId) {
                    SetColumnVisible((EmailColumn)colId, !info->visible);
                    break;
                }
            }
        }
    }
    
    delete menu;
}
