/*
 * AttachmentStripView - Horizontal chip-based attachment display
 * Distributed under the terms of the MIT License.
 */

#include "AttachmentStripView.h"

#include <algorithm>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <Application.h>
#include <Bitmap.h>
#include <Catalog.h>
#include <ControlLook.h>
#include <Cursor.h>
#include <Directory.h>
#include <Entry.h>
#include <FindDirectory.h>
#include <IconUtils.h>
#include <MenuItem.h>
#include <MimeType.h>
#include <Node.h>
#include <NodeInfo.h>
#include <Path.h>
#include <PopUpMenu.h>
#include <Roster.h>
#include <Size.h>
#include <Window.h>

#include <MailAttachment.h>
#include <MailContainer.h>

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "AttachmentStripView"


// Static constants
const float AttachmentStripView::kMaxChipWidth = 200.0f;
const float AttachmentStripView::kChipPadding = 4.0f;


// Static helper function
BString
AttachmentStripView::_BytesToString(off_t bytes)
{
	BString result;
	if (bytes < 1024) {
		result.SetToFormat("%lld B", bytes);
	} else if (bytes < 1024 * 1024) {
		result.SetToFormat("%.1f KB", bytes / 1024.0);
	} else if (bytes < 1024 * 1024 * 1024) {
		result.SetToFormat("%.1f MB", bytes / (1024.0 * 1024.0));
	} else {
		result.SetToFormat("%.1f GB", bytes / (1024.0 * 1024.0 * 1024.0));
	}
	return result;
}


AttachmentStripView::AttachmentStripView(bool composeMode)
	:
	BView("attachmentStrip", B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE | B_FRAME_EVENTS),
	fAttachments(10),
	fEmail(NULL),
	fHtmlAlternative(NULL),
	fHtmlAlternativeSize(0),
	fChipHeight(28.0f),
	fChipSpacing(6.0f),
	fSelectedAttachment(-1),
	fSavePanel(NULL),
	fHoveredAttachment(-1),
	fComposeMode(composeMode),
	fDragStarted(false),
	fMouseDown(false),
	fMouseDownPoint(0, 0),
	fMouseDownIndex(-1),
	fDraggingAttachment(-1)
{
	SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
}


AttachmentStripView::~AttachmentStripView()
{
	free(fHtmlAlternative);
	delete fSavePanel;
	delete fEmail;
}


void
AttachmentStripView::GetPreferredSize(float* width, float* height)
{
	if (fAttachments.CountItems() == 0) {
		*width = 0;
		*height = 0;
	} else {
		*width = B_SIZE_UNSET;
		*height = _CalculateTotalHeight();
	}
}


BSize
AttachmentStripView::MinSize()
{
	if (fAttachments.CountItems() == 0)
		return BSize(0, 0);
	
	return BSize(100, _CalculateTotalHeight());
}


BSize
AttachmentStripView::MaxSize()
{
	if (fAttachments.CountItems() == 0)
		return BSize(B_SIZE_UNLIMITED, 0);
	
	return BSize(B_SIZE_UNLIMITED, _CalculateTotalHeight());
}


void
AttachmentStripView::FrameResized(float newWidth, float newHeight)
{
	BView::FrameResized(newWidth, newHeight);
	// If the correct height differs from the current height, the layout was
	// calculated with incorrect metrics (e.g. Bounds().Width() was -1 at
	// layout time). Trigger a layout recalculation to correct it.
	float correctHeight = _CalculateTotalHeight();
	if (fAttachments.CountItems() > 0 && correctHeight != newHeight)
		InvalidateLayout();
	else
		Invalidate();
}


float
AttachmentStripView::_CalculateChipWidth(int32 index) const
{
	if (index < 0 || index >= fAttachments.CountItems())
		return 0;
	
	AttachmentInfo* info = fAttachments.ItemAt(index);
	BFont font;
	GetFont(&font);
	
	// Calculate width needed for filename only (no size)
	float textWidth = font.StringWidth(info->name.String());
	float chipWidth = textWidth + 36;  // 20px icon + 16px padding
	
	// Cap at max width
	return std::min(chipWidth, kMaxChipWidth);
}


float
AttachmentStripView::_CalculateTotalHeight() const
{
	if (fAttachments.CountItems() == 0)
		return 0;
	
	float viewWidth = Bounds().Width();
	if (viewWidth <= 0)
		viewWidth = 400;  // Default width for initial layout
	
	float x = kChipPadding;
	int32 rows = 1;
	
	for (int32 i = 0; i < fAttachments.CountItems(); i++) {
		float chipWidth = _CalculateChipWidth(i);
		
		// Check if chip fits on current row
		if (x + chipWidth + kChipPadding > viewWidth && x > kChipPadding) {
			// Wrap to next row
			rows++;
			x = kChipPadding;
		}
		
		x += chipWidth + fChipSpacing;
	}
	
	// Height = top padding + rows * chip height + spacing between rows + bottom padding
	return kChipPadding + (rows * fChipHeight) + ((rows - 1) * fChipSpacing) + kChipPadding;
}


BString
AttachmentStripView::_TruncateFilename(const char* filename, float maxWidth) const
{
	BFont font;
	GetFont(&font);
	
	BString result(filename);
	float width = font.StringWidth(result.String());
	
	if (width <= maxWidth)
		return result;
	
	// Find the right length with ellipsis
	const char* ellipsis = B_UTF8_ELLIPSIS;
	float ellipsisWidth = font.StringWidth(ellipsis);
	maxWidth -= ellipsisWidth;
	
	int32 len = result.Length();
	while (len > 0 && font.StringWidth(result.String(), len) > maxWidth) {
		len--;
	}
	
	result.Truncate(len);
	result.Append(ellipsis);
	return result;
}


void
AttachmentStripView::SetAttachments(BEmailMessage* email)
{
	// Clear previous state
	fAttachments.MakeEmpty();
	free(fHtmlAlternative);
	fHtmlAlternative = NULL;
	fHtmlAlternativeSize = 0;
	delete fEmail;
	fEmail = email;  // Take ownership
	
	if (fEmail == NULL) {
		InvalidateLayout();
		Invalidate();
		return;
	}
	
	// Collect attachments using Mail Kit
	BMailComponent* body = fEmail->Body();
	for (int32 i = 0; i < fEmail->CountComponents(); i++) {
		BMailComponent* component = fEmail->GetComponent(i);
		if (component == body)
			continue;
		
		_CollectAttachments(component, body);
	}
	
	InvalidateLayout();
	Invalidate();
}


void
AttachmentStripView::_CollectAttachments(BMailComponent* component,
	BMailComponent* body)
{
	// Skip the body component — it's the email text itself, not an attachment.
	// Walk the MIME tree recursively: multipart containers are expanded,
	// leaf components are checked for attachment-ness.
	if (component == NULL || component == body)
		return;
	
	// If it's a multipart container, recurse into it
	if (component->ComponentType() == B_MAIL_MULTIPART_CONTAINER) {
		BMIMEMultipartMailContainer* container = 
			dynamic_cast<BMIMEMultipartMailContainer*>(component);
		if (container != NULL) {
			for (int32 i = 0; i < container->CountComponents(); i++) {
				_CollectAttachments(container->GetComponent(i), body);
			}
		}
		return;
	}
	
	// Check MIME type
	BMimeType type;
	BString mimeType;
	if (component->MIMEType(&type) == B_OK) {
		mimeType = type.Type();
	}
	
	// Get filename (if any)
	BString filename;
	BMailAttachment* attachment = dynamic_cast<BMailAttachment*>(component);
	if (attachment != NULL) {
		char name[B_FILE_NAME_LENGTH * 2];
		if (attachment->FileName(name) == B_OK) {
			filename = name;
		}
	}
	
	// Check if this is an HTML alternative (text/html with no real filename)
	// These are alternative body representations, not real attachments
	if (mimeType.IFindFirst("text/html") >= 0 &&
		(filename.Length() == 0 || filename.ICompare("unnamed") == 0)) {
		// Store the HTML content for "View HTML version" feature
		if (fHtmlAlternativeSize == 0 && fEmailPath.Length() > 0) {
			// Try to extract raw HTML from email file to preserve original encoding
			// This bypasses Mail Kit's UTF-8 conversion
			if (ExtractRawHtmlFromEmail(fEmailPath.String(), &fHtmlAlternative, &fHtmlAlternativeSize)) {
				// Success - raw bytes extracted
			} else {
				// Fallback to GetDecodedData (will be UTF-8 converted)
				BMallocIO buffer;
				if (component->GetDecodedData(&buffer) == B_OK && buffer.BufferLength() > 0) {
					fHtmlAlternativeSize = buffer.BufferLength();
					fHtmlAlternative = malloc(fHtmlAlternativeSize);
					if (fHtmlAlternative != NULL) {
						memcpy(fHtmlAlternative, buffer.Buffer(), fHtmlAlternativeSize);
					} else {
						fHtmlAlternativeSize = 0;
					}
				}
			}
		}
		return;  // Don't add to attachments list
	}
	
	// It's a real attachment - extract info
	AttachmentInfo* info = new AttachmentInfo();
	info->component = component;
	info->componentIndex = fAttachments.CountItems();
	info->mimeType = mimeType;
	
	// Use filename if we got one
	if (filename.Length() > 0) {
		info->name = filename;
	} else {
		info->name = "unnamed";
	}
	
	// Get decoded size by decoding to a BMallocIO buffer
	BMallocIO buffer;
	if (component->GetDecodedData(&buffer) == B_OK) {
		info->decodedSize = buffer.BufferLength();
	} else {
		info->decodedSize = 0;
	}
	
	fAttachments.AddItem(info);
}


void
AttachmentStripView::ClearAttachments()
{
	fAttachments.MakeEmpty();
	free(fHtmlAlternative);
	fHtmlAlternative = NULL;
	fHtmlAlternativeSize = 0;
	delete fEmail;
	fEmail = NULL;
	fEmailPath = "";
	InvalidateLayout();
	Invalidate();
}


void
AttachmentStripView::AddAttachment(const entry_ref* ref)
{
	if (ref == NULL || !fComposeMode)
		return;
	
	// Check if already added (by comparing refs)
	for (int32 i = 0; i < fAttachments.CountItems(); i++) {
		AttachmentInfo* existing = fAttachments.ItemAt(i);
		if (existing->isFileRef && existing->ref == *ref)
			return;  // Already in list
	}
	
	BEntry entry(ref);
	if (!entry.Exists() || !entry.IsFile())
		return;
	
	AttachmentInfo* info = new AttachmentInfo();
	info->ref = *ref;
	info->isFileRef = true;
	info->name = ref->name;
	
	// Get file size
	BFile file(ref, B_READ_ONLY);
	if (file.InitCheck() == B_OK) {
		file.GetSize(&info->decodedSize);
	}
	
	// Get MIME type
	BNode node(ref);
	BNodeInfo nodeInfo(&node);
	char mimeType[B_MIME_TYPE_LENGTH];
	if (nodeInfo.GetType(mimeType) == B_OK) {
		info->mimeType = mimeType;
	} else {
		info->mimeType = "application/octet-stream";
	}
	
	fAttachments.AddItem(info);
	InvalidateLayout();
	Invalidate();
	
	// Notify window that attachments changed (for marking as changed)
	if (Window())
		Window()->PostMessage('atch');  // Custom message for attachment change
}


void
AttachmentStripView::RemoveAttachment(int32 index)
{
	if (index < 0 || index >= fAttachments.CountItems() || !fComposeMode)
		return;
	
	fAttachments.RemoveItemAt(index);
	InvalidateLayout();
	Invalidate();
	
	// Notify window that attachments changed
	if (Window())
		Window()->PostMessage('atch');
}


const entry_ref*
AttachmentStripView::AttachmentAt(int32 index) const
{
	if (index < 0 || index >= fAttachments.CountItems())
		return NULL;
	
	AttachmentInfo* info = fAttachments.ItemAt(index);
	if (info->isFileRef)
		return &info->ref;
	
	return NULL;
}


BMailComponent*
AttachmentStripView::ComponentAt(int32 index) const
{
	if (index < 0 || index >= fAttachments.CountItems())
		return NULL;
	
	AttachmentInfo* info = fAttachments.ItemAt(index);
	if (!info->isFileRef)
		return info->component;
	
	return NULL;
}


void
AttachmentStripView::AddEnclosuresFromMail(BEmailMessage* mail)
{
	if (mail == NULL)
		return;
	
	BMailComponent* body = mail->Body();
	
	for (int32 i = 0; i < mail->CountComponents(); i++) {
		BMailComponent* component = mail->GetComponent(i);
		if (component == body)
			continue;
		
		// Handle multipart containers recursively
		if (component->ComponentType() == B_MAIL_MULTIPART_CONTAINER) {
			BMIMEMultipartMailContainer* container = 
				dynamic_cast<BMIMEMultipartMailContainer*>(component);
			if (container != NULL) {
				for (int32 j = 0; j < container->CountComponents(); j++) {
					BMailComponent* subComponent = container->GetComponent(j);
					if (subComponent != body)
						_AddComponentAsAttachment(subComponent);
				}
			}
			continue;
		}
		
		_AddComponentAsAttachment(component);
	}
	
	// Show the strip if we added attachments
	if (HasAttachments()) {
		InvalidateLayout();
		Invalidate();
	}
}


void
AttachmentStripView::_AddComponentAsAttachment(BMailComponent* component)
{
	if (component == NULL)
		return;
	
	AttachmentInfo* info = new AttachmentInfo();
	info->component = component;
	info->isFileRef = false;
	info->componentIndex = fAttachments.CountItems();
	
	// Get filename
	BMailAttachment* attachment = dynamic_cast<BMailAttachment*>(component);
	if (attachment != NULL) {
		char name[B_FILE_NAME_LENGTH * 2];
		if (attachment->FileName(name) == B_OK) {
			info->name = name;
		}
	}
	
	// If no filename, use "unnamed"
	if (info->name.Length() == 0) {
		info->name = "unnamed";
	}
	
	// Get MIME type
	BMimeType type;
	if (component->MIMEType(&type) == B_OK) {
		info->mimeType = type.Type();
	}
	
	// Get decoded size
	BMallocIO buffer;
	if (component->GetDecodedData(&buffer) == B_OK) {
		info->decodedSize = buffer.BufferLength();
	} else {
		info->decodedSize = 0;
	}
	
	fAttachments.AddItem(info);
}


void
AttachmentStripView::SetEmailPath(const char* path)
{
	fEmailPath = path;
}


BRect
AttachmentStripView::_AttachmentRect(int32 index) const
{
	if (index < 0 || index >= fAttachments.CountItems())
		return BRect();
	
	float viewWidth = Bounds().Width();
	if (viewWidth <= 0)
		viewWidth = 400;  // Default for initial layout
	
	float x = kChipPadding;
	float y = kChipPadding;
	
	// Calculate position by iterating through all chips up to this one
	for (int32 i = 0; i <= index; i++) {
		float chipWidth = _CalculateChipWidth(i);
		
		// Check if chip fits on current row
		if (x + chipWidth + kChipPadding > viewWidth && x > kChipPadding) {
			// Wrap to next row
			x = kChipPadding;
			y += fChipHeight + fChipSpacing;
		}
		
		if (i == index) {
			// This is our target chip
			return BRect(x, y, x + chipWidth, y + fChipHeight);
		}
		
		x += chipWidth + fChipSpacing;
	}
	
	return BRect();
}


void
AttachmentStripView::Draw(BRect updateRect)
{
	if (fAttachments.CountItems() == 0)
		return;
	
	BFont font;
	GetFont(&font);
	font_height fh;
	font.GetHeight(&fh);
	
	rgb_color bgColor = ui_color(B_PANEL_BACKGROUND_COLOR);
	// Tint chip color based on whether background is dark or light
	rgb_color chipColor;
	if (bgColor.IsLight())
		chipColor = tint_color(bgColor, B_DARKEN_1_TINT);
	else
		chipColor = tint_color(bgColor, B_LIGHTEN_1_TINT);
	rgb_color textColor = ui_color(B_PANEL_TEXT_COLOR);
	
	for (int32 i = 0; i < fAttachments.CountItems(); i++) {
		AttachmentInfo* info = fAttachments.ItemAt(i);
		BRect chipRect = _AttachmentRect(i);
		
		// Skip if chip is outside update rect
		if (!chipRect.Intersects(updateRect))
			continue;
		
		// Draw chip background
		SetHighColor(chipColor);
		FillRoundRect(chipRect, 4, 4);
		
		// Draw MIME type icon (20x20 using vector icon for quality)
		float iconX = chipRect.left + 4;
		float iconY = chipRect.top + (chipRect.Height() - 20) / 2;
		
		BMimeType mimeType(info->mimeType.String());
		BBitmap* icon = new BBitmap(BRect(0, 0, 19, 19), B_RGBA32);
		
		// Try to get vector icon and render at exact size
		uint8* vectorData = NULL;
		size_t vectorSize = 0;
		bool gotIcon = false;
		
		if (mimeType.GetIcon(&vectorData, &vectorSize) == B_OK && vectorData != NULL) {
			if (BIconUtils::GetVectorIcon(vectorData, vectorSize, icon) == B_OK) {
				gotIcon = true;
			}
			free(vectorData);
		}
		
		// Fallback: try supertype
		if (!gotIcon) {
			BMimeType superType;
			if (mimeType.GetSupertype(&superType) == B_OK) {
				if (superType.GetIcon(&vectorData, &vectorSize) == B_OK
					&& vectorData != NULL) {
					if (BIconUtils::GetVectorIcon(vectorData, vectorSize, icon) == B_OK) {
						gotIcon = true;
					}
					free(vectorData);
				}
			}
		}
		
		// Final fallback: generic file icon (application/octet-stream)
		if (!gotIcon) {
			BMimeType genericType("application/octet-stream");
			if (genericType.GetIcon(&vectorData, &vectorSize) == B_OK
				&& vectorData != NULL) {
				if (BIconUtils::GetVectorIcon(vectorData, vectorSize, icon) == B_OK) {
					gotIcon = true;
				}
				free(vectorData);
			}
		}
		
		if (gotIcon) {
			SetDrawingMode(B_OP_ALPHA);
			DrawBitmap(icon, BPoint(iconX, iconY));
			SetDrawingMode(B_OP_COPY);
		}
		delete icon;
		
		// Draw filename only (truncated if necessary, no size)
		SetHighColor(textColor);
		float textY = chipRect.top + (chipRect.Height() + fh.ascent - fh.descent) / 2;
		float maxTextWidth = chipRect.Width() - 32;  // Account for icon and padding
		BString label = _TruncateFilename(info->name.String(), maxTextWidth);
		DrawString(label.String(), BPoint(chipRect.left + 28, textY));
	}
}


void
AttachmentStripView::MouseDown(BPoint where)
{
	// Find which attachment was clicked
	for (int32 i = 0; i < fAttachments.CountItems(); i++) {
		BRect chipRect = _AttachmentRect(i);
		if (chipRect.Contains(where)) {
			// Check which button was pressed
			int32 buttons;
			Window()->CurrentMessage()->FindInt32("buttons", &buttons);
			
			if (buttons & B_SECONDARY_MOUSE_BUTTON) {
				// Right-click - show context menu
				fSelectedAttachment = i;
				
				BPopUpMenu* menu = new BPopUpMenu("attachmentMenu", false, false);
				menu->AddItem(new BMenuItem(B_TRANSLATE("Open"),
					new BMessage(MSG_ATTACHMENT_OPEN)));
				
				if (fComposeMode) {
					// Compose mode: show Remove option
					menu->AddItem(new BMenuItem(B_TRANSLATE("Remove"),
						new BMessage(MSG_ATTACHMENT_REMOVE)));
				} else {
					// Read mode: show Save as option
					menu->AddItem(new BMenuItem(B_TRANSLATE("Save as" B_UTF8_ELLIPSIS),
						new BMessage(MSG_ATTACHMENT_SAVE)));
				}
				menu->SetTargetForItems(this);
				
				ConvertToScreen(&where);
				menu->Go(where, true, true, true);
				return;
			}
			
			// Left button - track for potential drag or double-click
			int32 clicks;
			if (Window()->CurrentMessage()->FindInt32("clicks", &clicks) == B_OK
				&& clicks >= 2) {
				_OpenAttachment(i);
				return;
			}
			
			// Start tracking for drag
			fMouseDown = true;
			fDragStarted = false;
			fMouseDownPoint = where;
			fMouseDownIndex = i;
			SetMouseEventMask(B_POINTER_EVENTS, B_NO_POINTER_HISTORY);
			return;
		}
	}
}


void
AttachmentStripView::MouseMoved(BPoint where, uint32 transit,
	const BMessage* dragMessage)
{
	if (fMouseDown && !fDragStarted && fMouseDownIndex >= 0) {
		// Check if mouse moved enough to start drag (5 pixels threshold)
		float dx = where.x - fMouseDownPoint.x;
		float dy = where.y - fMouseDownPoint.y;
		if (dx * dx + dy * dy > 25) {
			fDragStarted = true;
			_InitiateDrag(fMouseDownIndex, where);
		}
	}
	
	// Handle drag-and-drop feedback in compose mode
	if (fComposeMode && dragMessage != NULL && dragMessage->HasRef("refs")) {
		if (transit == B_ENTERED_VIEW || transit == B_INSIDE_VIEW) {
			// Show copy cursor when dragging files over
			BCursor cursor(B_CURSOR_ID_COPY);
			SetViewCursor(&cursor);
		} else if (transit == B_EXITED_VIEW) {
			// Restore default cursor
			SetViewCursor(B_CURSOR_SYSTEM_DEFAULT);
		}
	}
	
	// Handle tooltip on hover
	if (transit == B_EXITED_VIEW) {
		// Mouse left the view
		if (fHoveredAttachment >= 0) {
			fHoveredAttachment = -1;
			SetToolTip((const char*)NULL);
		}
		return;
	}
	
	// Find which attachment the mouse is over
	int32 hoveredIndex = -1;
	for (int32 i = 0; i < fAttachments.CountItems(); i++) {
		BRect chipRect = _AttachmentRect(i);
		if (chipRect.Contains(where)) {
			hoveredIndex = i;
			break;
		}
	}
	
	// Update tooltip if hovered attachment changed
	if (hoveredIndex != fHoveredAttachment) {
		fHoveredAttachment = hoveredIndex;
		
		if (fHoveredAttachment >= 0) {
			AttachmentInfo* info = fAttachments.ItemAt(fHoveredAttachment);
			BString tooltip;
			tooltip << info->name << "\n" << _BytesToString(info->decodedSize);
			SetToolTip(tooltip.String());
		} else {
			SetToolTip((const char*)NULL);
		}
	}
}


void
AttachmentStripView::MouseUp(BPoint where)
{
	fMouseDown = false;
	fDragStarted = false;
	fMouseDownIndex = -1;
}


void
AttachmentStripView::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case MSG_ATTACHMENT_OPEN:
			if (fSelectedAttachment >= 0
				&& fSelectedAttachment < fAttachments.CountItems()) {
				_OpenAttachment(fSelectedAttachment);
			}
			break;
		
		case MSG_ATTACHMENT_SAVE:
			if (fSelectedAttachment >= 0
				&& fSelectedAttachment < fAttachments.CountItems()) {
				AttachmentInfo* info = fAttachments.ItemAt(fSelectedAttachment);
				
				// Create file panel if needed
				if (!fSavePanel) {
					BMessenger messenger(this);
					fSavePanel = new BFilePanel(B_SAVE_PANEL, &messenger, NULL,
						B_FILE_NODE, false, new BMessage(MSG_ATTACHMENT_SAVE_PANEL));
				}
				
				// Set default filename
				fSavePanel->SetSaveText(info->name.String());
				fSavePanel->Show();
			}
			break;
		
		case MSG_ATTACHMENT_SAVE_PANEL:
		{
			// User selected save location
			entry_ref dirRef;
			BString name;
			if (message->FindRef("directory", &dirRef) == B_OK &&
				message->FindString("name", &name) == B_OK) {
				BPath path(&dirRef);
				path.Append(name.String());
				
				if (_ExtractAttachment(fSelectedAttachment, path.Path())) {
					// Set MIME type on saved file
					AttachmentInfo* info = fAttachments.ItemAt(fSelectedAttachment);
					BNode node(path.Path());
					BNodeInfo nodeInfo(&node);
					nodeInfo.SetType(info->mimeType.String());
				}
			}
			break;
		}
		
		case MSG_ATTACHMENT_REMOVE:
			if (fComposeMode && fSelectedAttachment >= 0
				&& fSelectedAttachment < fAttachments.CountItems()) {
				RemoveAttachment(fSelectedAttachment);
				fSelectedAttachment = -1;
			}
			break;
		
		case B_SIMPLE_DATA:
		case B_REFS_RECEIVED:
		{
			// Handle drag-and-drop of files (compose mode only)
			if (!fComposeMode)
				break;
			
			entry_ref ref;
			int32 index = 0;
			while (message->FindRef("refs", index++, &ref) == B_OK) {
				// Follow symlinks
				BEntry entry(&ref, true);
				entry.GetRef(&ref);
				
				// Only accept files
				if (entry.IsFile()) {
					AddAttachment(&ref);
				}
			}
			break;
		}
		
		case B_COPY_TARGET:
		{
			// Tracker is asking us to write the file to a specific location
			entry_ref dirRef;
			BString name;
			if (message->FindRef("directory", &dirRef) == B_OK &&
				message->FindString("name", &name) == B_OK &&
				fDraggingAttachment >= 0
				&& fDraggingAttachment < fAttachments.CountItems()) {
				
				BPath path(&dirRef);
				path.Append(name.String());
				
				if (_ExtractAttachment(fDraggingAttachment, path.Path())) {
					// Set MIME type on saved file - but not for emails
					AttachmentInfo* info = fAttachments.ItemAt(fDraggingAttachment);
					if (info->mimeType.IFindFirst("message/rfc822") < 0 &&
						info->mimeType.IFindFirst("text/x-email") < 0) {
						BNode node(path.Path());
						BNodeInfo nodeInfo(&node);
						nodeInfo.SetType(info->mimeType.String());
					}
				}
				
				fDraggingAttachment = -1;
			}
			break;
		}
		
		default:
			BView::MessageReceived(message);
			break;
	}
}


bool
AttachmentStripView::_ExtractAttachment(int32 index, const char* destPath)
{
	if (index < 0 || index >= fAttachments.CountItems())
		return false;
	
	AttachmentInfo* info = fAttachments.ItemAt(index);
	if (info->component == NULL)
		return false;
	
	// Create output file and write decoded data
	{
		BFile outFile(destPath, B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
		if (outFile.InitCheck() != B_OK)
			return false;
		
		// Use Mail Kit to decode and write the data
		status_t result = info->component->GetDecodedData(&outFile);
		if (result != B_OK)
			return false;
		
		outFile.Sync();
	}
	// File is now closed
	
	// If this is an email attachment, parse it and add Haiku mail attributes
	if (info->mimeType.IFindFirst("message/rfc822") >= 0 ||
		info->mimeType.IFindFirst("text/x-email") >= 0) {
		
		// Set the MIME type
		BNode node(destPath);
		if (node.InitCheck() != B_OK)
			return true;  // File extracted OK, just can't add attributes
			
		BNodeInfo nodeInfo(&node);
		nodeInfo.SetType("text/x-email");
		
		// Parse the extracted email to get header info
		entry_ref ref;
		BEntry entry(destPath);
		if (entry.GetRef(&ref) == B_OK) {
			BEmailMessage extractedMail(&ref);
			
			// Write standard mail attributes using BNode
			// NOTE: We intentionally skip MAIL:subject so the temp file
			// won't appear in email queries (which match on MAIL:subject=*)
			BString value;
			
			// MAIL:from
			value = extractedMail.From();
			if (value.Length() > 0)
				node.WriteAttrString("MAIL:from", &value);
			
			// MAIL:to
			value = extractedMail.To();
			if (value.Length() > 0)
				node.WriteAttrString("MAIL:to", &value);
			
			// MAIL:cc
			value = extractedMail.CC();
			if (value.Length() > 0)
				node.WriteAttrString("MAIL:cc", &value);
			
			// MAIL:when
			time_t when = extractedMail.Date();
			if (when > 0)
				node.WriteAttr("MAIL:when", B_TIME_TYPE, 0, &when, sizeof(when));
			
			// MAIL:status - mark as read
			value = "Read";
			node.WriteAttrString("MAIL:status", &value);
			
			// MAIL:reply - use from address
			value = extractedMail.From();
			if (value.Length() > 0)
				node.WriteAttrString("MAIL:reply", &value);
			
			// MAIL:name - use subject for display
			value = extractedMail.Subject();
			if (value.Length() > 0)
				node.WriteAttrString("MAIL:name", &value);
			
			// MAIL:account_id and MAIL:account - copy from parent email if available
			if (fEmailPath.Length() > 0) {
				BNode parentNode(fEmailPath.String());
				if (parentNode.InitCheck() == B_OK) {
					int32 accountId;
					if (parentNode.ReadAttr("MAIL:account_id", B_INT32_TYPE, 0,
							&accountId, sizeof(accountId)) == sizeof(accountId)) {
						node.WriteAttr("MAIL:account_id", B_INT32_TYPE, 0,
							&accountId, sizeof(accountId));
					}
					
					BString accountName;
					if (parentNode.ReadAttrString("MAIL:account", &accountName) == B_OK) {
						node.WriteAttrString("MAIL:account", &accountName);
					}
				}
			}
			
			node.Sync();
		}
	}
	
	return true;
}


void
AttachmentStripView::_OpenAttachment(int32 index)
{
	if (index < 0 || index >= fAttachments.CountItems())
		return;
	
	AttachmentInfo* info = fAttachments.ItemAt(index);
	
	// HTML body: save to temp and open in browser
	if (info->isHtmlBody) {
		BPath tempPath;
		if (find_directory(B_SYSTEM_TEMP_DIRECTORY, &tempPath) != B_OK)
			return;
		tempPath.Append("EmailViews_attachments");
		create_directory(tempPath.Path(), 0755);
		tempPath.Append("email_body.html");
		
		// Write HTML content to temp file
		BFile file(tempPath.Path(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
		if (file.InitCheck() == B_OK) {
			file.Write(info->htmlContent.String(), info->htmlContent.Length());
			file.Sync();
			
			// Set MIME type
			BNodeInfo nodeInfo(&file);
			nodeInfo.SetType("text/html");
			
			// Open in default browser (Web+)
			entry_ref ref;
			BEntry entry(tempPath.Path());
			if (entry.GetRef(&ref) == B_OK) {
				be_roster->Launch(&ref);
			}
		}
		return;
	}
	
	// Compose mode: open file directly
	if (fComposeMode && info->isFileRef) {
		// Check if this is an email file
		bool isEmail = (info->mimeType.IFindFirst("message/rfc822") >= 0 ||
		                info->mimeType.IFindFirst("text/x-email") >= 0);
		
		if (isEmail) {
			// Open email attachments in our own reader
			BMessage refsMsg(B_REFS_RECEIVED);
			refsMsg.AddRef("refs", &info->ref);
			be_app->PostMessage(&refsMsg);
		} else {
			// Open other files with system default app
			be_roster->Launch(&info->ref);
		}
		return;
	}
	
	// Read mode: extract to temp and open
	// Create temp directory if needed
	BPath tempPath;
	if (find_directory(B_SYSTEM_TEMP_DIRECTORY, &tempPath) != B_OK)
		return;
	tempPath.Append("EmailViews_attachments");
	create_directory(tempPath.Path(), 0755);
	
	// Create temp file path
	BString tempFile(tempPath.Path());
	tempFile << "/" << info->name;
	
	if (_ExtractAttachment(index, tempFile.String())) {
		// Check if this is an email attachment
		bool isEmail = (info->mimeType.IFindFirst("message/rfc822") >= 0 ||
		                info->mimeType.IFindFirst("text/x-email") >= 0);
		
		// Set MIME type - but not for emails, _ExtractAttachment already set it
		if (!isEmail) {
			BNode node(tempFile.String());
			BNodeInfo nodeInfo(&node);
			nodeInfo.SetType(info->mimeType.String());
		}
		
		// Open the attachment
		entry_ref ref;
		BEntry entry(tempFile.String());
		if (entry.GetRef(&ref) == B_OK) {
			if (isEmail) {
				// Open email attachments in our own reader via B_REFS_RECEIVED
				BMessage refsMsg(B_REFS_RECEIVED);
				refsMsg.AddRef("refs", &ref);
				be_app->PostMessage(&refsMsg);
			} else {
				// Open other files with system default app
				be_roster->Launch(&ref);
			}
		}
	}
}


void
AttachmentStripView::_InitiateDrag(int32 index, BPoint where)
{
	if (index < 0 || index >= fAttachments.CountItems())
		return;
	
	AttachmentInfo* info = fAttachments.ItemAt(index);
	
	// Store which attachment is being dragged for B_COPY_TARGET response
	fDraggingAttachment = index;
	
	// Create drag message using B_COPY_TARGET negotiation protocol
	BMessage dragMessage(B_SIMPLE_DATA);
	dragMessage.AddInt32("be:actions", B_COPY_TARGET);
	dragMessage.AddString("be:types", B_FILE_MIME_TYPE);
	dragMessage.AddString("be:filetypes", info->mimeType.String());
	dragMessage.AddString("be:clip_name", info->name.String());
	
	// Create drag bitmap from chip area
	BRect chipRect = _AttachmentRect(index);
	BBitmap* dragBitmap = new BBitmap(chipRect.OffsetToCopy(0, 0), B_RGBA32, true);
	
	if (dragBitmap->Lock()) {
		BView* view = new BView(dragBitmap->Bounds(), "", B_FOLLOW_NONE, 0);
		dragBitmap->AddChild(view);
		
		// Clear to transparent
		view->SetHighColor(B_TRANSPARENT_COLOR);
		view->FillRect(view->Bounds());
		
		// Set up alpha blending
		view->SetDrawingMode(B_OP_ALPHA);
		view->SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_COMPOSITE);
		
		// Draw semi-transparent rounded chip
		// Tint based on whether background is dark or light
		rgb_color panelBg = ui_color(B_PANEL_BACKGROUND_COLOR);
		rgb_color chipColor;
		if (panelBg.IsLight())
			chipColor = tint_color(panelBg, B_DARKEN_1_TINT);
		else
			chipColor = tint_color(panelBg, B_LIGHTEN_1_TINT);
		chipColor.alpha = 192;
		view->SetHighColor(chipColor);
		view->FillRoundRect(view->Bounds(), 4, 4);
		
		// Draw icon (20x20 using vector icon for quality)
		float iconY = (chipRect.Height() - 20) / 2;
		BMimeType mimeType(info->mimeType.String());
		BBitmap* icon = new BBitmap(BRect(0, 0, 19, 19), B_RGBA32);
		
		// Try to get vector icon and render at exact size
		uint8* vectorData = NULL;
		size_t vectorSize = 0;
		bool gotIcon = false;
		
		if (mimeType.GetIcon(&vectorData, &vectorSize) == B_OK && vectorData != NULL) {
			if (BIconUtils::GetVectorIcon(vectorData, vectorSize, icon) == B_OK) {
				gotIcon = true;
			}
			free(vectorData);
		}
		
		// Fallback: try supertype
		if (!gotIcon) {
			BMimeType superType;
			if (mimeType.GetSupertype(&superType) == B_OK) {
				if (superType.GetIcon(&vectorData, &vectorSize) == B_OK
					&& vectorData != NULL) {
					if (BIconUtils::GetVectorIcon(vectorData, vectorSize, icon) == B_OK) {
						gotIcon = true;
					}
					free(vectorData);
				}
			}
		}
		
		// Final fallback: generic file icon (application/octet-stream)
		if (!gotIcon) {
			BMimeType genericType("application/octet-stream");
			if (genericType.GetIcon(&vectorData, &vectorSize) == B_OK
				&& vectorData != NULL) {
				if (BIconUtils::GetVectorIcon(vectorData, vectorSize, icon) == B_OK) {
					gotIcon = true;
				}
				free(vectorData);
			}
		}
		
		if (gotIcon) {
			view->DrawBitmap(icon, BPoint(4, iconY));
		}
		delete icon;
		
		// Draw filename (truncated if needed)
		font_height fh;
		view->GetFontHeight(&fh);
		float textY = (chipRect.Height() + fh.ascent - fh.descent) / 2;
		float maxTextWidth = chipRect.Width() - 32;
		BString label = _TruncateFilename(info->name.String(), maxTextWidth);
		view->SetHighColor(ui_color(B_PANEL_TEXT_COLOR));
		view->DrawString(label.String(), BPoint(28, textY));
		
		view->Sync();
		dragBitmap->Unlock();
	}
	
	// Calculate offset based on click position within chip
	BPoint offset(where.x - chipRect.left, where.y - chipRect.top);
	
	// Start the drag
	DragMessage(&dragMessage, dragBitmap, B_OP_ALPHA, offset);
}


bool
AttachmentStripView::_DecodeQuotedPrintable(const char* input, size_t inputLen,
	void** outData, size_t* outSize)
{
	// Allocate output buffer (decoded is always <= input size)
	char* output = (char*)malloc(inputLen + 1);
	if (output == NULL)
		return false;
	
	size_t outPos = 0;
	for (size_t i = 0; i < inputLen; i++) {
		if (input[i] == '=') {
			if (i + 2 < inputLen) {
				// Check for soft line break (=\r\n or =\n)
				if (input[i + 1] == '\r' && input[i + 2] == '\n') {
					i += 2;
					continue;
				} else if (input[i + 1] == '\n') {
					i += 1;
					continue;
				}
				// Decode hex pair
				char hex[3] = { input[i + 1], input[i + 2], '\0' };
				char* endptr;
				long value = strtol(hex, &endptr, 16);
				if (*endptr == '\0') {
					output[outPos++] = (char)value;
					i += 2;
					continue;
				}
			}
		}
		output[outPos++] = input[i];
	}
	
	*outData = output;
	*outSize = outPos;
	return true;
}


bool
AttachmentStripView::_DecodeBase64(const char* input, size_t inputLen,
	void** outData, size_t* outSize)
{
	static const char base64Chars[] = 
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	
	// Build reverse lookup table
	int8 decodeTable[256];
	memset(decodeTable, -1, sizeof(decodeTable));
	for (int i = 0; i < 64; i++)
		decodeTable[(unsigned char)base64Chars[i]] = i;
	decodeTable['='] = 0;  // Padding
	
	// Allocate output buffer (decoded is ~3/4 of input size)
	size_t maxOutSize = (inputLen * 3) / 4 + 4;
	char* output = (char*)malloc(maxOutSize);
	if (output == NULL)
		return false;
	
	size_t outPos = 0;
	uint32 accumulator = 0;
	int bits = 0;
	
	for (size_t i = 0; i < inputLen; i++) {
		char c = input[i];
		
		// Skip whitespace
		if (c == '\r' || c == '\n' || c == ' ' || c == '\t')
			continue;
		
		// Stop at padding
		if (c == '=')
			break;
		
		int8 value = decodeTable[(unsigned char)c];
		if (value < 0)
			continue;  // Skip invalid characters
		
		accumulator = (accumulator << 6) | value;
		bits += 6;
		
		if (bits >= 8) {
			bits -= 8;
			output[outPos++] = (char)((accumulator >> bits) & 0xFF);
		}
	}
	
	*outData = output;
	*outSize = outPos;
	return true;
}


// Extract the HTML body directly from the raw MIME email file.
// We do this instead of using Mail Kit's GetDecodedData() because Mail Kit
// always converts to UTF-8, which destroys the original charset encoding
// (e.g., ISO-2022-JP for Japanese emails). The raw bytes are passed to the
// browser via "View HTML version", which can detect charset from the HTML
// meta tag itself.
bool
AttachmentStripView::ExtractRawHtmlFromEmail(const char* emailPath,
	void** outData, size_t* outSize)
{
	*outData = NULL;
	*outSize = 0;
	
	if (emailPath == NULL || emailPath[0] == '\0')
		return false;
	
	// Read the entire email file
	BFile file(emailPath, B_READ_ONLY);
	if (file.InitCheck() != B_OK)
		return false;
	
	off_t fileSize;
	if (file.GetSize(&fileSize) != B_OK || fileSize <= 0 || fileSize > 10 * 1024 * 1024)
		return false;  // Limit to 10MB
	
	char* emailData = (char*)malloc(fileSize + 1);
	if (emailData == NULL)
		return false;
	
	ssize_t bytesRead = file.Read(emailData, fileSize);
	if (bytesRead <= 0) {
		free(emailData);
		return false;
	}
	emailData[bytesRead] = '\0';
	
	// Find the Content-Type header to get the boundary
	// Look for: Content-Type: multipart/...; boundary="..."
	BString boundary;
	const char* contentType = strcasestr(emailData, "Content-Type:");
	if (contentType != NULL) {
		const char* boundaryStart = strcasestr(contentType, "boundary=");
		if (boundaryStart != NULL) {
			boundaryStart += 9;  // Skip "boundary="
			
			// Handle quoted or unquoted boundary
			if (*boundaryStart == '"') {
				boundaryStart++;
				const char* boundaryEnd = strchr(boundaryStart, '"');
				if (boundaryEnd != NULL)
					boundary.SetTo(boundaryStart, boundaryEnd - boundaryStart);
			} else {
				// Unquoted - ends at whitespace, semicolon, or newline
				const char* boundaryEnd = boundaryStart;
				while (*boundaryEnd && *boundaryEnd != ' ' && *boundaryEnd != '\t' &&
				       *boundaryEnd != ';' && *boundaryEnd != '\r' && *boundaryEnd != '\n')
					boundaryEnd++;
				boundary.SetTo(boundaryStart, boundaryEnd - boundaryStart);
			}
		}
	}
	
	if (boundary.Length() == 0) {
		free(emailData);
		return false;  // Not a multipart message
	}
	
	// Prepend "--" to boundary for searching
	BString fullBoundary("--");
	fullBoundary << boundary;
	
	// Find the text/html part
	const char* pos = emailData;
	while ((pos = strstr(pos, fullBoundary.String())) != NULL) {
		pos += fullBoundary.Length();
		
		// Skip to the end of boundary line
		while (*pos == '-') pos++;  // Skip closing boundary dashes
		while (*pos == '\r' || *pos == '\n') pos++;
		
		// Find the headers for this part
		const char* partStart = pos;
		const char* headerEnd = strstr(partStart, "\r\n\r\n");
		if (headerEnd == NULL)
			headerEnd = strstr(partStart, "\n\n");
		if (headerEnd == NULL)
			continue;
		
		// Check if this part is text/html
		size_t headerLen = headerEnd - partStart;
		char* headers = (char*)malloc(headerLen + 1);
		if (headers == NULL)
			continue;
		memcpy(headers, partStart, headerLen);
		headers[headerLen] = '\0';
		
		// Check Content-Type
		bool isHtml = (strcasestr(headers, "Content-Type:") != NULL &&
		               strcasestr(headers, "text/html") != NULL);
		
		// Get transfer encoding
		BString encoding;
		const char* encHeader = strcasestr(headers, "Content-Transfer-Encoding:");
		if (encHeader != NULL) {
			encHeader += 26;  // Skip header name
			while (*encHeader == ' ' || *encHeader == '\t') encHeader++;
			const char* encEnd = encHeader;
			while (*encEnd && *encEnd != '\r' && *encEnd != '\n') encEnd++;
			encoding.SetTo(encHeader, encEnd - encHeader);
			encoding.Trim();
			encoding.ToLower();
		}
		
		free(headers);
		
		if (!isHtml)
			continue;
		
		// Found HTML part - extract the body
		const char* bodyStart = headerEnd;
		while (*bodyStart == '\r' || *bodyStart == '\n') bodyStart++;
		
		// Find the end (next boundary)
		BString endBoundary("\r\n");
		endBoundary << fullBoundary;
		const char* bodyEnd = strstr(bodyStart, endBoundary.String());
		if (bodyEnd == NULL) {
			endBoundary.SetTo("\n");
			endBoundary << fullBoundary;
			bodyEnd = strstr(bodyStart, endBoundary.String());
		}
		if (bodyEnd == NULL)
			bodyEnd = emailData + bytesRead;  // Use end of file
		
		size_t bodyLen = bodyEnd - bodyStart;
		
		// Decode based on transfer encoding
		bool success = false;
		if (encoding == "base64") {
			success = _DecodeBase64(bodyStart, bodyLen, outData, outSize);
		} else if (encoding == "quoted-printable") {
			success = _DecodeQuotedPrintable(bodyStart, bodyLen, outData, outSize);
		} else {
			// 7bit, 8bit, or binary - copy as-is
			*outData = malloc(bodyLen);
			if (*outData != NULL) {
				memcpy(*outData, bodyStart, bodyLen);
				*outSize = bodyLen;
				success = true;
			}
		}
		
		free(emailData);
		return success;
	}
	
	free(emailData);
	return false;
}


bool
AttachmentStripView::ExtractRawBodyFromEmail(const char* emailPath,
	void** outData, size_t* outSize)
{
	*outData = NULL;
	*outSize = 0;
	
	if (emailPath == NULL || emailPath[0] == '\0')
		return false;
	
	// Read the entire email file
	BFile file(emailPath, B_READ_ONLY);
	if (file.InitCheck() != B_OK)
		return false;
	
	off_t fileSize;
	if (file.GetSize(&fileSize) != B_OK || fileSize <= 0 || fileSize > 10 * 1024 * 1024)
		return false;  // Limit to 10MB
	
	char* emailData = (char*)malloc(fileSize + 1);
	if (emailData == NULL)
		return false;
	
	ssize_t bytesRead = file.Read(emailData, fileSize);
	if (bytesRead <= 0) {
		free(emailData);
		return false;
	}
	emailData[bytesRead] = '\0';
	
	// Find the end of headers (blank line)
	const char* bodyStart = strstr(emailData, "\r\n\r\n");
	if (bodyStart != NULL) {
		bodyStart += 4;
	} else {
		bodyStart = strstr(emailData, "\n\n");
		if (bodyStart != NULL)
			bodyStart += 2;
	}
	
	if (bodyStart == NULL) {
		free(emailData);
		return false;
	}
	
	// Get transfer encoding from headers
	BString encoding;
	size_t headerLen = bodyStart - emailData;
	char* headers = (char*)malloc(headerLen + 1);
	if (headers != NULL) {
		memcpy(headers, emailData, headerLen);
		headers[headerLen] = '\0';
		
		// Search for Content-Transfer-Encoding header
		// Need to find it at start of line or after newline
		const char* searchPos = headers;
		const char* encHeader = NULL;
		while ((searchPos = strcasestr(searchPos, "Content-Transfer-Encoding:")) != NULL) {
			// Check if this is at the start or after a newline
			if (searchPos == headers || *(searchPos - 1) == '\n') {
				encHeader = searchPos;
				break;
			}
			searchPos++;
		}
		
		if (encHeader != NULL) {
			encHeader += 26;  // Skip "Content-Transfer-Encoding:"
			while (*encHeader == ' ' || *encHeader == '\t') encHeader++;
			const char* encEnd = encHeader;
			while (*encEnd && *encEnd != '\r' && *encEnd != '\n') encEnd++;
			encoding.SetTo(encHeader, encEnd - encHeader);
			encoding.Trim();
			encoding.ToLower();
		}
		free(headers);
	}
	
	size_t bodyLen = bytesRead - (bodyStart - emailData);
	
	// Decode based on transfer encoding
	bool success = false;
	if (encoding == "base64") {
		success = _DecodeBase64(bodyStart, bodyLen, outData, outSize);
	} else if (encoding == "quoted-printable") {
		success = _DecodeQuotedPrintable(bodyStart, bodyLen, outData, outSize);
	} else {
		// 7bit, 8bit, or binary - copy as-is
		*outData = malloc(bodyLen);
		if (*outData != NULL) {
			memcpy(*outData, bodyStart, bodyLen);
			*outSize = bodyLen;
			success = true;
		}
	}
	
	free(emailData);
	return success;
}
