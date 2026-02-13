/*
 * QueryItem.h - List items for query sidebar
 * Distributed under the terms of the MIT License.
 */

#ifndef QUERY_ITEM_H
#define QUERY_ITEM_H

#include <ListItem.h>
#include <String.h>
#include <StringItem.h>

class BBitmap;

// Query list item with icon support
class QueryItem : public BStringItem {
public:
	QueryItem(const char* name, const char* path, BBitmap* icon,
		bool isQuery = false, bool isCustomQuery = false,
		float iconSize = 24.0);
	virtual ~QueryItem();

	const char* GetPath() const { return fPath.String(); }
	const char* GetBaseName() const { return fBaseName.String(); }
	bool IsQuery() const { return fIsQuery; }
	bool IsCustomQuery() const { return fIsCustomQuery; }
	float IconSize() const { return fIconSize; }
	void SetIcon(BBitmap* icon);

	virtual void DrawItem(BView* owner, BRect frame, bool complete = false);
	virtual void Update(BView* owner, const BFont* font);

private:
	BString fPath;
	BString fBaseName;  // Original name without count
	BBitmap* fIcon;
	float fIconSize;
	bool fIsQuery;
	bool fIsCustomQuery;
};

// Separator item for visual grouping
class SeparatorItem : public BListItem {
public:
	SeparatorItem();
	virtual void DrawItem(BView* owner, BRect frame, bool complete = false);
	virtual void Update(BView* owner, const BFont* font);
};

#endif // QUERY_ITEM_H
