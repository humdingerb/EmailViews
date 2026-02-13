/*
 * QueryNameDialog.cpp - Simple modal input dialog for query names
 * Distributed under the terms of the MIT License.
 *
 * Uses a semaphore to implement synchronous modal behavior: Go() shows the
 * window and blocks on acquire_sem() until the user clicks Save/Cancel or
 * closes the window (which releases the sem). This avoids the complexity
 * of async callbacks while keeping the window's looper responsive.
 */

#include "QueryNameDialog.h"

#include <Catalog.h>
#include <LayoutBuilder.h>
#include <TextView.h>

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "QueryNameDialog"

QueryNameDialog::QueryNameDialog(const char* title, const char* label,
	const char* defaultText)
	:
	BWindow(BRect(0, 0, 350, 100), title,
		B_MODAL_WINDOW_LOOK, B_MODAL_APP_WINDOW_FEEL,
		B_NOT_RESIZABLE | B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS
			| B_CLOSE_ON_ESCAPE),
	fTextControl(NULL),
	fSaveButton(NULL),
	fResult(false),
	fReadyToQuit(false),
	fSem(create_sem(0, "dialog_sem"))
{
	fTextControl = new BTextControl("name", label, defaultText, NULL);
	fTextControl->SetModificationMessage(new BMessage('_mod'));

	fSaveButton = new BButton("save", B_TRANSLATE("Save"), new BMessage('save'));
	fSaveButton->SetEnabled(strlen(defaultText) > 0);
	fSaveButton->MakeDefault(true);

	BButton* cancelButton = new BButton("cancel", B_TRANSLATE("Cancel"), new BMessage('canc'));

	BLayoutBuilder::Group<>(this, B_VERTICAL, B_USE_DEFAULT_SPACING)
		.SetInsets(B_USE_WINDOW_INSETS)
		.Add(fTextControl)
		.AddGroup(B_HORIZONTAL)
			.AddGlue()
			.Add(cancelButton)
			.Add(fSaveButton)
		.End();

	CenterOnScreen();
	fTextControl->MakeFocus(true);
	// Select all text
	fTextControl->TextView()->SelectAll();
}


QueryNameDialog::~QueryNameDialog()
{
	delete_sem(fSem);
}


void
QueryNameDialog::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case '_mod':
			fSaveButton->SetEnabled(strlen(fTextControl->Text()) > 0);
			break;
		case 'save':
			fResult = true;
			fName = fTextControl->Text();
			release_sem(fSem);
			break;
		case 'canc':
			fResult = false;
			release_sem(fSem);
			break;
		default:
			BWindow::MessageReceived(message);
	}
}


bool
QueryNameDialog::QuitRequested()
{
	if (fReadyToQuit)
		return true;

	// User closed window via close button or Escape.
	// Don't actually quit yet — Go() is blocked on the sem and needs to
	// read fResult safely under Lock() before calling Quit() itself.
	fResult = false;
	release_sem(fSem);
	return false;
}


bool
QueryNameDialog::Go(BString& name)
{
	Show();
	acquire_sem(fSem);

	bool result = false;
	// Lock window to safely access member variables
	if (Lock()) {
		result = fResult;
		if (result)
			name = fName;
		fReadyToQuit = true;
		Quit();  // This unlocks and destroys the window
	}

	return result;
}
