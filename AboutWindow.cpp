/*
 * AboutWindow.cpp - About dialog for EmailViews
 * Distributed under the terms of the MIT License.
 */

#include "AboutWindow.h"
#include "AppInfo.h"

#include <AppFileInfo.h>
#include <Application.h>
#include <Bitmap.h>
#include <Button.h>
#include <Catalog.h>
#include <Directory.h>
#include <File.h>
#include <FindDirectory.h>
#include <Font.h>
#include <GroupView.h>
#include <LayoutBuilder.h>
#include <Message.h>
#include <Path.h>
#include <Roster.h>
#include <ScrollView.h>
#include <String.h>
#include <StringView.h>
#include <TextView.h>
#include <View.h>

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "AboutWindow"

static const char* kAppDescription = 
	B_TRANSLATE("EmailViews is a fast, lightweight email viewer for Haiku that uses live "
	"queries to organize and explore your emails effortlessly.");

static const char* kAppAuthors = 
	B_TRANSLATE("Created with the assistance of AI tools.\n"
	"Maintained by Jorge Mare.");

// Credits section — headers are translated separately so they can be
// both inserted into the text and used as FindFirst() keys for bold styling.
static const char* kCreditsHeader =
	B_TRANSLATE("Credits:");
static const char* kCreditsBody =
	B_TRANSLATE("Haiku Mail application by the Haiku Project.\n"
	"Icons from Haiku and Zumi icon sets.");

static const char* kInspirationHeader =
	B_TRANSLATE("Sources of inspiration:");
static const char* kInspirationBody =
	B_TRANSLATE("Beam by Oliver Tappe (attribute search UI, drag-and-drop handling, "
	"attachment caching).\n"
	"QuickLaunch by Humdinger (Deskbar replicant integration).\n"
	"Tracker by the Haiku Project.");

static const char* kThanksHeader =
	B_TRANSLATE("Special thanks:");
static const char* kThanksBody =
	B_TRANSLATE("Special thanks go to Humdinger for his meticulous testing, detailed bug "
	"reports, valuable feature suggestions, and unwavering commitment to "
	"improving this software.");


// Helper function to get version string from app resources
static BString
GetVersionString()
{
	BString versionStr;
	
	// Find the EmailViews binary.  When the About dialog is opened from the
	// Deskbar replicant, be_app points to Deskbar, not EmailViews.  Use the
	// app roster to locate our binary by signature instead.
	entry_ref appRef;
	bool found = false;
	if (be_roster->FindApp("application/x-vnd.EmailViews", &appRef) == B_OK)
		found = true;

	if (found) {
		BFile appFile(&appRef, B_READ_ONLY);
		if (appFile.InitCheck() == B_OK) {
			BAppFileInfo appFileInfo(&appFile);
			if (appFileInfo.InitCheck() == B_OK) {
				version_info versionInfo;
				if (appFileInfo.GetVersionInfo(&versionInfo, B_APP_VERSION_KIND) == B_OK) {
					// Use short_info from rdef (e.g., "EmailViews - 1.0")
					// Extract just the version part after the dash
					BString shortInfo = versionInfo.short_info;
					int32 dashPos = shortInfo.FindFirst(" - ");
					if (dashPos >= 0) {
						shortInfo.Remove(0, dashPos + 3);
						versionStr = shortInfo;
					} else {
						versionStr = versionInfo.short_info;
					}
				}
			}
		}
	}
	
	// Fallback if we couldn't read from resources
	if (versionStr.IsEmpty())
		versionStr = "1.0";
	
	// Prepend "Version " and append build timestamp
	BString result;
	result << "Version " << versionStr << " (" << BUILD_TIMESTAMP << ")";
	
	return result;
}


// Helper view for displaying the application icon
class IconView : public BView {
public:
	explicit IconView(BBitmap* icon)
		: BView("icon", B_WILL_DRAW),
		  fIcon(icon)
	{
		SetExplicitMinSize(BSize(64, 64));
		SetExplicitMaxSize(BSize(64, 64));
	}

	virtual void Draw(BRect updateRect)
	{
		SetHighUIColor(B_PANEL_BACKGROUND_COLOR);
		FillRect(Bounds());
		if (fIcon) {
			SetDrawingMode(B_OP_ALPHA);
			DrawBitmap(fIcon, BPoint(0, 0));
		}
	}

private:
	BBitmap* fIcon;  // Not owned, AboutWindow owns it
};


AboutWindow::AboutWindow(BBitmap* icon)
	: BWindow(BRect(0, 0, 480, 320), "About",
		  B_MODAL_WINDOW_LOOK, B_MODAL_APP_WINDOW_FEEL,
		  B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS |
		  B_CLOSE_ON_ESCAPE),
	  fIcon(icon)
{
	// Icon view (64x64)
	IconView* iconView = new IconView(fIcon);

	// App name in large bold font (2x normal size)
	BStringView* nameView = new BStringView("name", kAppName);
	BFont boldFont(be_bold_font);
	boldFont.SetSize(boldFont.Size() * 2.0);
	nameView->SetFont(&boldFont);
	nameView->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNSET));

	// Version string (read from app resources)
	BString versionStr = GetVersionString();
	BStringView* versionView = new BStringView("version", versionStr.String());
	versionView->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNSET));

	// Description text in a scrollable view
	fTextView = new BTextView("description");
	fTextView->SetViewUIColor(B_LIST_BACKGROUND_COLOR);
	fTextView->SetLowUIColor(B_LIST_BACKGROUND_COLOR);
	fTextView->SetHighUIColor(B_LIST_ITEM_TEXT_COLOR);
	fTextView->MakeEditable(false);
	fTextView->MakeSelectable(false);
	fTextView->SetStylable(true);
	fTextView->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNLIMITED));

	BString descText;
	descText << kAppDescription << "\n\n"
		 << kAppAuthors << "\n\n"
		 << kCreditsHeader << "\n" << kCreditsBody << "\n\n"
		 << kInspirationHeader << "\n" << kInspirationBody << "\n\n"
		 << kThanksHeader << "\n" << kThanksBody << "\n";
	fTextView->SetText(descText.String());

	// Apply text color to all text
	rgb_color textColor = ui_color(B_LIST_ITEM_TEXT_COLOR);
	fTextView->SetFontAndColor(0, descText.Length(), NULL, B_FONT_ALL, &textColor);

	// Apply bold to section headers using the same translated strings
	BFont sectionFont(be_bold_font);

	int32 creditsPos = descText.FindFirst(kCreditsHeader);
	if (creditsPos >= 0) {
		fTextView->SetFontAndColor(creditsPos,
			creditsPos + strlen(kCreditsHeader),
			&sectionFont, B_FONT_ALL, &textColor);
	}
	int32 sourcesPos = descText.FindFirst(kInspirationHeader);
	if (sourcesPos >= 0) {
		fTextView->SetFontAndColor(sourcesPos,
			sourcesPos + strlen(kInspirationHeader),
			&sectionFont, B_FONT_ALL, &textColor);
	}
	int32 thanksPos = descText.FindFirst(kThanksHeader);
	if (thanksPos >= 0) {
		fTextView->SetFontAndColor(thanksPos,
			thanksPos + strlen(kThanksHeader),
			&sectionFont, B_FONT_ALL, &textColor);
	}

	BScrollView* scrollView = new BScrollView("scroll", fTextView,
		B_WILL_DRAW | B_FRAME_EVENTS, false, true, B_PLAIN_BORDER);
	scrollView->SetExplicitMinSize(BSize(350, 150));
	scrollView->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNLIMITED));

	// OK button
	BButton* okButton = new BButton("ok", B_TRANSLATE("OK"), new BMessage(MSG_OK));
	okButton->MakeDefault(true);

	// Layout: icon on left, everything else in right column
	// Create left column (icon)
	BGroupView* leftColumn = new BGroupView(B_VERTICAL);
	BLayoutBuilder::Group<>(leftColumn)
		.Add(iconView)
		.AddGlue();

	// Create right column (content)
	BGroupView* rightColumn = new BGroupView(B_VERTICAL, B_USE_SMALL_SPACING);
	rightColumn->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNLIMITED));
	BLayoutBuilder::Group<>(rightColumn)
		.Add(nameView)
		.Add(versionView)
		.AddStrut(B_USE_SMALL_SPACING)
		.Add(scrollView, 1.0f)
		.AddGroup(B_HORIZONTAL)
			.AddGlue()
			.Add(okButton)
		.End();

	// Main horizontal layout with weights
	BLayoutBuilder::Group<>(this, B_HORIZONTAL, B_USE_DEFAULT_SPACING)
		.SetInsets(B_USE_WINDOW_INSETS)
		.Add(leftColumn, 0.0f)
		.Add(rightColumn, 1.0f);

	// Load saved frame or center on screen
	_LoadFrame();
}


AboutWindow::~AboutWindow()
{
	delete fIcon;
}


void
AboutWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case MSG_OK:
			PostMessage(B_QUIT_REQUESTED);
			break;
		case B_COLORS_UPDATED:
		{
			// Update text color when system colors change
			if (fTextView != NULL) {
				rgb_color textColor = ui_color(B_LIST_ITEM_TEXT_COLOR);
				int32 textLength = fTextView->TextLength();
				if (textLength > 0) {
					fTextView->SetFontAndColor(0, textLength, NULL, B_FONT_ALL, &textColor);
				}
				fTextView->Invalidate();
			}
			break;
		}
		default:
			BWindow::MessageReceived(message);
			break;
	}
}


bool
AboutWindow::QuitRequested()
{
	_SaveFrame();
	return true;
}


void
AboutWindow::_SaveFrame()
{
	BPath path;
	if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) != B_OK)
		return;
	path.Append("EmailViews");
	create_directory(path.Path(), 0755);
	path.Append("emailviews_settings");

	// Load existing settings to preserve other data
	BMessage settings;
	BFile file(path.Path(), B_READ_ONLY);
	if (file.InitCheck() == B_OK) {
		settings.Unflatten(&file);
	}
	file.Unset();

	// Update about window frame
	settings.RemoveName("about_window_frame");
	settings.AddRect("about_window_frame", Frame());

	// Save back
	file.SetTo(path.Path(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	if (file.InitCheck() == B_OK) {
		settings.Flatten(&file);
	}
}


void
AboutWindow::_LoadFrame()
{
	BPath path;
	if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) != B_OK) {
		CenterOnScreen();
		return;
	}
	path.Append("EmailViews/emailviews_settings");

	BFile file(path.Path(), B_READ_ONLY);
	if (file.InitCheck() != B_OK) {
		CenterOnScreen();
		return;
	}

	BMessage settings;
	if (settings.Unflatten(&file) != B_OK) {
		CenterOnScreen();
		return;
	}

	BRect frame;
	if (settings.FindRect("about_window_frame", &frame) == B_OK) {
		MoveTo(frame.LeftTop());
		ResizeTo(frame.Width(), frame.Height());
	} else {
		CenterOnScreen();
	}
}
