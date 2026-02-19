/*
 * EmailColumnHeader.h - Column header view for EmailListView
 * Distributed under the terms of the MIT License.
 * 
 * Displays clickable column headers with sort indicators.
 * Coordinates with EmailListView for column widths and sort state.
 */

#ifndef EMAIL_COLUMN_HEADER_H
#define EMAIL_COLUMN_HEADER_H

#include <View.h>
#include <Bitmap.h>
#include <Cursor.h>
#include <Size.h>
#include <String.h>
#include <ObjectList.h>


// Column identifier
enum EmailColumn {
    kColumnStatus = 0,
    kColumnStar,
    kColumnAttachment,
    kColumnFrom,
    kColumnTo,
    kColumnSubject,
    kColumnDate,
    kColumnAccount,
    kColumnCount
};


// Sort order
enum SortOrder {
    kSortNone = 0,
    kSortAscending,
    kSortDescending
};


/*
 * ColumnInfo - Information about a single column
 */
struct ColumnInfo {
    EmailColumn     id;
    BString         title;
    float           width;          // Current width in pixels (recalculated on resize)
    float           minWidth;       // Minimum width (prevents collapse)
    float           proportion;     // Share of available space (0.0-1.0) for initial layout
    bool            sortable;
    bool            isIcon;         // Icon column (fixed width, no text)
    bool            visible;        // Whether this column is shown
    BBitmap*        headerIcon;     // Icon to display in header (not owned)
    
    ColumnInfo(EmailColumn _id, const char* _title, float _proportion, 
               float _minWidth = 30.0f, bool _sortable = true, bool _isIcon = false)
        :
        id(_id),
        title(_title),
        width(0),
        minWidth(_minWidth),
        proportion(_proportion),
        sortable(_sortable),
        isIcon(_isIcon),
        visible(true),
        headerIcon(NULL)
    {
    }
};


// Forward declaration
class EmailListView;


/*
 * EmailColumnHeaderView - The header bar above the list
 */
class EmailColumnHeaderView : public BView {
public:
                            EmailColumnHeaderView(const char* name);
    virtual                 ~EmailColumnHeaderView();
    
    // BView overrides
    virtual void            AttachedToWindow();
    virtual void            Draw(BRect updateRect);
    virtual void            MouseDown(BPoint where);
    virtual void            MouseMoved(BPoint where, uint32 transit,
                                        const BMessage* dragMessage);
    virtual void            MouseUp(BPoint where);
    virtual void            FrameResized(float width, float height);
    virtual void            GetPreferredSize(float* _width, float* _height);
    virtual BSize           MinSize();
    virtual BSize           MaxSize();
    
    // Column management
    void                    SetColumnWidth(EmailColumn column, float width);
    float                   ColumnWidth(EmailColumn column) const;
    float                   ColumnPosition(EmailColumn column) const;
    float                   TotalWidth() const;
    void                    SetColumnHeaderIcon(EmailColumn column, BBitmap* icon);
    
    // Column visibility
    void                    SetColumnVisible(EmailColumn column, bool visible);
    bool                    IsColumnVisible(EmailColumn column) const;
    int32                   CountVisibleColumns() const;
    
    // Get all column widths (for list view to use)
    void                    GetColumnWidths(float* status, float* from,
                                            float* subject, float* date) const;
    
    // Get column order (for list view to draw in correct order)
    int32                   CountColumns() const { return fColumns.CountItems(); }
    EmailColumn             ColumnAt(int32 index) const;
    float                   ColumnWidthAt(int32 index) const;
    
    // Horizontal scroll sync
    void                    ScrollToH(float x);
    
    // Sort state
    void                    SetSortColumn(EmailColumn column, SortOrder order);
    EmailColumn             SortColumn() const { return fSortColumn; }
    SortOrder               GetSortOrder() const { return fSortOrder; }
    
    // Persistence
    status_t                SaveState(BMessage* into) const;
    status_t                RestoreState(const BMessage* from);
    
    // Link to list view (for coordinating widths and sort)
    void                    SetListView(EmailListView* listView);
    
    // Message sent when column is clicked, resized, or reordered
    static const uint32     kMsgColumnClicked = 'cclk';
    static const uint32     kMsgColumnResized = 'crsz';
    static const uint32     kMsgColumnReordered = 'cord';
    
private:
    void                    _RecalculateWidths();
    int32                   _ColumnAt(float x) const;
    int32                   _ResizeBorderAt(float x) const;
    BRect                   _ColumnRect(int32 index) const;
    void                    _DrawColumn(int32 index, BRect rect, bool pressed);
    void                    _DrawDraggedColumn();
    void                    _SetResizeCursor(bool resize);
    void                    _SwapColumns(int32 fromIndex, int32 toIndex);
    void                    _ShowColumnContextMenu(BPoint where);

#if B_HAIKU_VERSION > B_HAIKU_VERSION_1_BETA_5
    BObjectList<ColumnInfo, true> fColumns;  // true = owns items
#else
    BObjectList<ColumnInfo> fColumns;
#endif
    float                   fHeaderHeight;
    
    // Sort state
    EmailColumn             fSortColumn;
    enum SortOrder          fSortOrder;
    
    // Mouse tracking for clicks
    int32                   fPressedColumn;
    int32                   fHoverColumn;
    
    // Mouse tracking for resizing
    bool                    fResizing;
    int32                   fResizeColumn;      // Column to the LEFT of the border being dragged
    float                   fResizeStartX;
    float                   fResizeStartWidth;
    
    // Mouse tracking for dragging (reorder)
    bool                    fDragging;
    int32                   fDragColumn;        // Column being dragged (current position)
    float                   fDragStartX;        // Mouse X when drag started
    float                   fDragCurrentX;      // Current mouse X (for drawing floating header)
    float                   fDragColumnWidth;   // Width of dragged column
    static constexpr float  kDragThreshold = 5.0f;  // Pixels before drag starts
    
    // Track if initial widths have been calculated
    bool                    fInitialWidthsSet;
    
    // When true, column widths came from saved settings (via RestoreState)
    // and should be used as-is rather than recalculated proportionally
    bool                    fStateRestored;
    
    // Resize zone width (pixels from border edge)
    static constexpr float  kResizeZone = 5.0f;
    
    // Linked list view
    EmailListView*          fListView;
};


#endif // EMAIL_COLUMN_HEADER_H
