/*
Open Tracker License

Terms and Conditions

Copyright (c) 1991-2001, Be Incorporated. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice applies to all licensees
and shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF TITLE, MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
BE INCORPORATED BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF, OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of Be Incorporated shall not be
used in advertising or otherwise to promote the sale, use or other dealings in
this Software without prior written authorization from Be Incorporated.

BeMail(TM), Tracker(TM), Be(R), BeOS(R), and BeIA(TM) are trademarks or registered trademarks
of Be Incorporated in the United States and other countries. Other brand product
names are registered trademarks or trademarks of their respective holders.
All rights reserved.
*/
#ifndef _EMAIL_READER_WINDOW_H
#define _EMAIL_READER_WINDOW_H


#include <Entry.h>
#include <Font.h>
#include <Locker.h>
#include <Messenger.h>
#include <ObjectList.h>
#include <SeparatorView.h>
#include <Window.h>

#include "ToolBarView.h"

#include <E-mail.h>
#include <mail_encoding.h>


class TContentView;
class TEnclosuresView;
class THeaderView;
class TMenu;
class TPrefsWindow;
class TSignatureWindow;
class Settings;
class AttachmentStripView;

class BEmailMessage;
class BFile;
class BFilePanel;
class BMailMessage;
class BMenu;
class BMenuBar;
class BMenuItem;
class Words;

// Messages for communication with EmailViewsWindow
static const uint32 kMsgReaderStatusChanged = 'RdSt';
static const uint32 kMsgReaderWindowClosed = 'RdCl';

// Forward declaration
class EmailViewsWindow;


class EmailReaderWindow : public BWindow {
public:
								EmailReaderWindow(BRect frame, const char* title,
									const entry_ref* ref, const char* to,
									const BFont* font, bool resending,
									EmailViewsWindow* emailViews = NULL);
	virtual						~EmailReaderWindow();

	virtual	void				DispatchMessage(BMessage* message, BHandler* handler);
	virtual	void				FrameResized(float width, float height);
	virtual	void				MenusBeginning();
	virtual	void				MessageReceived(BMessage*);
	virtual	bool				QuitRequested();
	virtual	void				Show();
	virtual	void				Zoom(BPoint, float, float);
	virtual	void				WindowActivated(bool state);

			void				SetTo(const char* mailTo, const char* subject,
									const char* ccTo = NULL,
									const char* bccTo = NULL,
									const BString* body = NULL,
									BMessage* enclosures = NULL);
			void				AddAutoSignature(bool beforeQuotedText = false,
									bool resetChanged = false);
			void				Forward(entry_ref*, EmailReaderWindow*,
									bool includeAttachments);
			void				Print();
			void				PrintSetup();
			void				Reply(entry_ref*, EmailReaderWindow*, uint32);
			void				CopyMessage(entry_ref* ref, EmailReaderWindow* src);
			
			// Compose response directly from entry_ref (no source window needed)
			status_t			ComposeReplyTo(entry_ref* ref, uint32 type);
			status_t			ComposeForwardOf(entry_ref* ref, bool includeAttachments);
			
			status_t			Send(bool now);
			status_t			SaveAsDraft();
			status_t			OpenMessage(const entry_ref* ref,
									uint32 characterSetForDecoding
										= B_MAIL_NULL_CONVERSION);

			status_t			GetMailNodeRef(node_ref &nodeRef) const;
			BEmailMessage*		Mail() const { return fMail; }

			bool				GetTrackerWindowFile(entry_ref*,
									bool dir) const;
			void				SaveTrackerPosition(entry_ref*);
			void				SetOriginatingWindow(BWindow* window);

			void				PreserveReadingPos(bool save);
			void				MarkMessageRead(entry_ref* message,
									read_flags flag);
			void				SetTrackerSelectionToCurrent();
			EmailReaderWindow*	FrontmostWindow();
	static	EmailReaderWindow*	FindWindow(const entry_ref& ref);
			void				UpdateViews();
			void				UpdatePreferences();

			// EmailViews integration
			void				SetEmailViewsWindow(EmailViewsWindow* window);
			EmailViewsWindow*	GetEmailViewsWindow() const { return fEmailViewsWindow; }

protected:
			void				SetTitleForMessage();
			void				AddEnclosure(BMessage* msg);
			void				BuildToolBar();
			status_t			TrainMessageAs(const char* commandWord);

private:
			void				_CreateNewPerson(BString address, BString name);
			void				_AddReadButton();
			void				_UpdateReadButton();
			void				_UpdateNavigationButtons();
			void				_UpdateLabel(uint32 command, const char* label,
									bool show);

			void				_SetDownloading(bool downloading);
			uint32				_CurrentCharacterSet() const;

	static	BBitmap*			_RetrieveVectorIcon(int32 id);

private:
			BEmailMessage*		fMail;
			entry_ref*			fRef;
				// Reference to currently displayed file
			int32				fFieldState;
			BFilePanel*			fPanel;
			BMenuBar*			fMenuBar;
			BMenuItem*			fAdd;
			BMenuItem*			fCut;
			BMenuItem*			fCopy;
			BMenuItem*			fHeader;
			BMenuItem*			fPaste;
			BMenuItem*			fPrint;
			BMenuItem*			fPrintSetup;
			BMenuItem*			fQuote;
			BMenuItem*			fRaw;
			BMenuItem*			fRemove;
			BMenuItem*			fRemoveQuote;
			BMenuItem*			fSendNow;
			BMenuItem*			fSendLater;
			BMenuItem*			fUndo;
			BMenuItem*			fRedo;
			BMenuItem*			fNextMsg;
			BMenuItem*			fPrevMsg;
			BMenuItem*			fDeleteNext;
			BMenuItem*			fSpelling;
			BMenu*				fSaveAddrMenu;

			BMenu*				fEncodingMenu;

	struct BitmapItem {
		BBitmap* bm;
		int32 id;
	};
	static	BObjectList<BitmapItem>	sBitmapCache;
	static	BLocker				sBitmapCacheLock;

			ToolBarView*			fToolBar;

			BRect				fZoom;
			TContentView*		fContentView;
			THeaderView*		fHeaderView;
			TEnclosuresView*	fEnclosuresView;
			AttachmentStripView* fAttachmentStrip;
			BSeparatorView*		fAttachmentSeparator;
			BButton*			fHtmlVersionButton;
			void*				fHtmlBodyContent;
			size_t				fHtmlBodyContentSize;
			TMenu*				fSignature;

			BMessenger			fMessengerToSpamServer;
			
			// EmailViews integration - direct pointer for simplified navigation
			EmailViewsWindow*	fEmailViewsWindow;

			entry_ref			fOpenFolder;

			bool				fSigAdded : 1;
			bool				fIncoming : 1;
			bool				fReplying : 1;
			bool				fResending : 1;
			bool				fSent : 1;
			bool				fDraft : 1;
			bool				fChanged : 1;

	static	BList				sWindowList;
	static	BLocker				sWindowListLock;

			entry_ref			fRepliedMail;
			BMessenger*			fOriginatingWindow;
			BEmailMessage*		fSourceMail;  // Source mail for Reply/Forward from main window

			bool				fAutoMarkRead : 1;
			bool				fKeepStatusOnClose;

			bool				fDownloading;
};


#endif // _EMAIL_READER_WINDOW_H
