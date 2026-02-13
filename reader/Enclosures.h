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
#ifndef _ENCLOSURES_H
#define _ENCLOSURES_H


#include <Box.h>
#include <File.h>
#include <ListView.h>
#include <NodeInfo.h>
#include <Path.h>
#include <Point.h>
#include <Rect.h>
#include <ScrollView.h>
#include <View.h>
#include <Volume.h>

#include <MailMessage.h>


class TListView;
class EmailReaderWindow;
class TScrollView;


class TEnclosuresView : public BView {
public:
								TEnclosuresView();
	virtual						~TEnclosuresView();

	virtual	void				MessageReceived(BMessage* message);
			void				Focus(bool focus);
			void				AddEnclosuresFromMail(BEmailMessage* mail);

			TListView*			fList;

private:
			bool				fFocus;
			EmailReaderWindow*		fWindow;
};


class TListView : public BListView {
public:
	explicit					TListView(TEnclosuresView* view);

	virtual	void				AttachedToWindow() override;
	virtual BSize				MinSize() override;
	virtual	void				MakeFocus(bool focus) override;
	virtual	void				MouseDown(BPoint point) override;
	virtual	void				KeyDown(const char* bytes,int32 numBytes) override;

private:
			TEnclosuresView*	fParent;
};


class TListItem : public BListItem {
public:
	explicit					TListItem(entry_ref* ref);
	explicit					TListItem(BMailComponent* component);

	virtual	void				DrawItem(BView* owner, BRect rect,
									bool complete) override;
	virtual	void				Update(BView* owner, const BFont* font) override;

			BMailComponent*		Component() { return fComponent; };
			entry_ref*			Ref() { return &fRef; }
			node_ref*			NodeRef() { return &fNodeRef; }

private:
			BMailComponent*		fComponent;
			entry_ref			fRef;
			node_ref			fNodeRef;
};


#endif	// _ENCLOSURES_H
