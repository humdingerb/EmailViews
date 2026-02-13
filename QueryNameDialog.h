/*
 * QueryNameDialog.h - Simple modal input dialog for query names
 * Distributed under the terms of the MIT License.
 */

#ifndef QUERY_NAME_DIALOG_H
#define QUERY_NAME_DIALOG_H

#include <Button.h>
#include <String.h>
#include <TextControl.h>
#include <Window.h>

#include <OS.h>

// Simple modal input dialog for query name
class QueryNameDialog : public BWindow {
public:
	QueryNameDialog(const char* title, const char* label,
		const char* defaultText);
	virtual ~QueryNameDialog();

	virtual void MessageReceived(BMessage* message);
	virtual bool QuitRequested();

	// Shows dialog and waits for result. Returns true if saved, false if cancelled.
	// If true, name contains the entered text.
	bool Go(BString& name);

private:
	BTextControl* fTextControl;
	BButton* fSaveButton;
	BString fName;
	bool fResult;
	bool fReadyToQuit;
	sem_id fSem;
};

#endif // QUERY_NAME_DIALOG_H
