/*
 * AttachmentStripView.h - Horizontal chip-based attachment display
 * Distributed under the terms of the MIT License.
 *
 * Operates in two modes:
 * - Read mode: parses a BEmailMessage's MIME tree to extract attachments
 *   and HTML alternatives, displaying them as clickable chips. Supports
 *   open, save, and drag-and-drop to Tracker.
 * - Compose mode: displays user-added file attachments with remove support.
 *
 * Each chip shows the filename, size, and a type icon. Chips flow left-to-
 * right and wrap to multiple rows. The view auto-sizes its height to fit.
 */

#ifndef ATTACHMENT_STRIP_VIEW_H
#define ATTACHMENT_STRIP_VIEW_H

#include <Entry.h>
#include <FilePanel.h>
#include <ObjectList.h>
#include <String.h>
#include <View.h>

#include <MailComponent.h>
#include <MailMessage.h>


// Attachment info structure
struct AttachmentInfo {
	BString name;
	BString mimeType;
	BMailComponent* component;  // Points into BEmailMessage's MIME tree (not owned) - read mode
	entry_ref ref;              // File reference - compose mode
	off_t decodedSize;          // Estimated decoded size (MIME parts are often base64-inflated)
	int32 componentIndex;       // For reference
	bool isFileRef;             // true if this is a file ref (compose mode)
	bool isHtmlBody;            // true if this is HTML body content (for preview)
	BString htmlContent;        // HTML content (when isHtmlBody is true)
	
	AttachmentInfo() : component(NULL), decodedSize(0), componentIndex(0), 
	                   isFileRef(false), isHtmlBody(false) {}
};


// Message constants for attachment context menu
const uint32 MSG_ATTACHMENT_OPEN = 'aopn';
const uint32 MSG_ATTACHMENT_SAVE = 'asav';
const uint32 MSG_ATTACHMENT_SAVE_PANEL = 'aspn';
const uint32 MSG_ATTACHMENT_REMOVE = 'armv';


// Attachment strip view for displaying attachments as horizontal chips
class AttachmentStripView : public BView {
public:
						AttachmentStripView(bool composeMode = false);
	virtual				~AttachmentStripView();
	
	virtual void		Draw(BRect updateRect);
	virtual void		MouseDown(BPoint where);
	virtual void		MouseMoved(BPoint where, uint32 transit,
							const BMessage* dragMessage);
	virtual void		MouseUp(BPoint where);
	virtual void		MessageReceived(BMessage* message);
	virtual void		GetPreferredSize(float* width, float* height);
	virtual BSize		MinSize();
	virtual BSize		MaxSize();
	virtual void		FrameResized(float newWidth, float newHeight);
	
	// Read mode - display attachments from email
	void				SetAttachments(BEmailMessage* email);  // Takes ownership
	void				SetEmailPath(const char* path);
	
	// Compose mode - manage file attachments
	void				AddAttachment(const entry_ref* ref);
	void				AddEnclosuresFromMail(BEmailMessage* mail);  // For forwarding
	void				RemoveAttachment(int32 index);
	int32				CountAttachments() const { return fAttachments.CountItems(); }
	const entry_ref*	AttachmentAt(int32 index) const;
	BMailComponent*		ComponentAt(int32 index) const;
	
	// Common
	void				ClearAttachments();
	bool				HasAttachments() const
							{ return fAttachments.CountItems() > 0; }
	bool				IsComposeMode() const { return fComposeMode; }
	bool				HasHtmlAlternative() const { return fHtmlAlternativeSize > 0; }
	const void*			HtmlAlternative() const { return fHtmlAlternative; }
	size_t				HtmlAlternativeSize() const { return fHtmlAlternativeSize; }
	
	// Raw email extraction (for preserving original charset encoding)
	bool				ExtractRawHtmlFromEmail(const char* emailPath,
							void** outData, size_t* outSize);
	bool				ExtractRawBodyFromEmail(const char* emailPath,
							void** outData, size_t* outSize);
	
private:
	BRect				_AttachmentRect(int32 index) const;
	float				_CalculateChipWidth(int32 index) const;
	float				_CalculateTotalHeight() const;
	BString				_TruncateFilename(const char* filename,
							float maxWidth) const;
	bool				_ExtractAttachment(int32 index, const char* destPath);
	void				_OpenAttachment(int32 index);
	void				_InitiateDrag(int32 index, BPoint where);
	void				_CollectAttachments(BMailComponent* component,
							BMailComponent* body);
	void				_AddComponentAsAttachment(BMailComponent* component);
	bool				_DecodeQuotedPrintable(const char* input, size_t inputLen,
							void** outData, size_t* outSize);
	bool				_DecodeBase64(const char* input, size_t inputLen,
							void** outData, size_t* outSize);
	
	static BString		_BytesToString(off_t bytes);
	
	static const float	kMaxChipWidth;
	static const float	kChipPadding;

#if B_HAIKU_VERSION > B_HAIKU_VERSION_1_BETA_5
	BObjectList<AttachmentInfo, true> fAttachments;  // owns AttachmentInfo items
#else
	BObjectList<AttachmentInfo> fAttachments;
#endif
	
	BEmailMessage*		fEmail;  // Owns the email message (read mode only)
	BString				fEmailPath;
	void*				fHtmlAlternative;      // Raw HTML alternative body content (if any)
	size_t				fHtmlAlternativeSize;  // Size of fHtmlAlternative buffer
	float				fChipHeight;
	float				fChipSpacing;
	int32				fSelectedAttachment;  // For context menu / file panel
	BFilePanel*			fSavePanel;
	int32				fHoveredAttachment;  // For tooltip display
	bool				fComposeMode;  // true for compose, false for read
	
	// Drag tracking
	bool				fDragStarted;
	bool				fMouseDown;
	BPoint				fMouseDownPoint;
	int32				fMouseDownIndex;
	int32				fDraggingAttachment;  // For B_COPY_TARGET response
};


#endif // ATTACHMENT_STRIP_VIEW_H
