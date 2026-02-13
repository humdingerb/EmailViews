/*
 * Copyright 2002-2006, project beam (http://sourceforge.net/projects/beam).
 * All rights reserved. Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Oliver Tappe <beam@hirschkaefer.de>
 */
#ifndef _AUTO_COMPLETER_DEFAULT_IMPL_H
#define _AUTO_COMPLETER_DEFAULT_IMPL_H

#include <ListView.h>
#include <String.h>

#include "AutoCompleter.h"

class BDefaultPatternSelector : public BAutoCompleter::PatternSelector {
public:
	virtual	void				SelectPatternBounds(const BString& text,
									int32 caretPos, int32* start,
									int32* length) override;
};


class BDefaultCompletionStyle : public BAutoCompleter::CompletionStyle {
public:
								BDefaultCompletionStyle(
									BAutoCompleter::EditView* editView, 
									BAutoCompleter::ChoiceModel* choiceModel,
									BAutoCompleter::ChoiceView* choiceView, 
									BAutoCompleter::PatternSelector*
										patternSelector);
	virtual						~BDefaultCompletionStyle() override;

	virtual	bool				Select(int32 index) override;
	virtual	bool				SelectNext(bool wrap = false) override;
	virtual	bool				SelectPrevious(bool wrap = false) override;
	virtual	bool				IsChoiceSelected() const override;
	virtual	int32				SelectedChoiceIndex() const override;

	virtual	void				ApplyChoice(bool hideChoices = true) override;
	virtual	void				CancelChoice() override;

	virtual	void				EditViewStateChanged(bool updateChoices) override;

private:
			BString				fFullEnteredText;
			int32				fSelectedIndex;
			int32				fPatternStartPos;
			int32				fPatternLength;
			bool				fIgnoreEditViewStateChanges;
};


class BDefaultChoiceView : public BAutoCompleter::ChoiceView {
protected:
	class ListView : public BListView {
	public:
		explicit				ListView(
									BAutoCompleter::CompletionStyle* completer);
		virtual	void			SelectionChanged() override;
		virtual	void			MessageReceived(BMessage* msg) override;
		virtual	void			MouseDown(BPoint point) override;
		virtual	void			AttachedToWindow() override;

	private:
				BAutoCompleter::CompletionStyle* fCompleter;
	};

	class ListItem : public BListItem {
	public:
		explicit				ListItem(const BAutoCompleter::Choice* choice);
		virtual	void			DrawItem(BView* owner, BRect frame,
									bool complete = false) override;
	private:
				BString			fPreText;
				BString			fMatchText;
				BString			fPostText;
	};

public:
								BDefaultChoiceView();
	virtual						~BDefaultChoiceView() override;
	
	virtual	void				SelectChoiceAt(int32 index) override;
	virtual	void				ShowChoices(
									BAutoCompleter::CompletionStyle* completer) override;
	virtual	void				HideChoices() override;
	virtual	bool				ChoicesAreShown() override;
	virtual int32				CountVisibleChoices() const override;

				void			SetMaxVisibleChoices(int32 choices);
				int32			MaxVisibleChoices() const;

private:
			BWindow*			fWindow;
			ListView*			fListView;
			int32				fMaxVisibleChoices;
};

#endif // _AUTO_COMPLETER_DEFAULT_IMPL_H
