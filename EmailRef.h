/*
 * EmailRef.h - Email identity and attribute structure
 * Distributed under the terms of the MIT License.
 *
 * Holds identity (node_ref, entry_ref) and ALL display attributes.
 * All attributes are read in the constructor, which runs on a
 * background thread. This makes sorting instant since all data
 * is already in memory — no disk I/O on the UI thread.
 *
 * ReloadAttributes() re-reads from disk when a node monitor fires
 * (only called for the ~30-50 visible items).
 */

#ifndef EMAIL_REF_H
#define EMAIL_REF_H

#include <Entry.h>
#include <Node.h>
#include <String.h>
#include <fs_attr.h>

struct EmailRef {
    node_ref    nodeRef;        // For matching live query notifications
    entry_ref   entryRef;       // For file access
    time_t      when;           // MAIL:when, cached for sorting
    timespec    crtime;         // Creation time for secondary sort

    // Display attributes (read upfront on background thread)
    BString     status;         // MAIL:status
    BString     from;           // MAIL:from
    BString     to;             // MAIL:to
    BString     subject;        // MAIL:subject
    BString     account;        // MAIL:account (resolved to name if int32)

    // Boolean state
    bool        isRead;         // Derived from status
    bool        hasAttachment;  // MAIL:attachment
    bool        isStarred;      // FILE:starred

    // Constructor from entry_ref (reads all attributes)
    EmailRef(const entry_ref& ref)
        : entryRef(ref),
          when(0),
          crtime({0, 0}),
          isRead(true),
          hasAttachment(false),
          isStarred(false)
    {
        BEntry entry(&ref);
        if (entry.InitCheck() == B_OK) {
            entry.GetNodeRef(&nodeRef);

            // Get creation time for secondary sort
            time_t crtimeVal;
            if (entry.GetCreationTime(&crtimeVal) == B_OK) {
                crtime.tv_sec = crtimeVal;
                crtime.tv_nsec = 0;
            }

            BNode node(&ref);
            if (node.InitCheck() == B_OK)
                _ReadAttributes(node);
        }
    }

    // Re-read all attributes from disk (called on node monitor update)
    void ReloadAttributes()
    {
        BNode node(&entryRef);
        if (node.InitCheck() == B_OK)
            _ReadAttributes(node);
    }

private:
    // Resolve account ID to name - defined out-of-line in
    // EmailListView.cpp to avoid circular dependency
    void _ResolveAccountName(int32 accountId);

    void _ReadAttributes(BNode& node)
    {
        char buffer[512];

        // MAIL:when - try int64 first (modern mail_daemon), then int32 (legacy)
        int64 whenAttr64 = 0;
        ssize_t bytesRead = node.ReadAttr("MAIL:when", B_TIME_TYPE, 0,
                                           &whenAttr64, sizeof(whenAttr64));
        if (bytesRead == sizeof(int64))
            when = (time_t)whenAttr64;
        else if (bytesRead == sizeof(int32))
            when = (time_t)(int32)whenAttr64;

        // MAIL:status
        bytesRead = node.ReadAttr("MAIL:status", B_STRING_TYPE, 0,
                                   buffer, sizeof(buffer) - 1);
        if (bytesRead > 0) {
            buffer[bytesRead] = '\0';
            status = buffer;
        } else {
            status = "";
        }
        // "New" means never opened; "Read" and "Seen" both count as read
        // for visual purposes (bold vs normal weight in list view)
        isRead = (status.ICompare("New") != 0);

        // MAIL:from
        bytesRead = node.ReadAttr("MAIL:from", B_STRING_TYPE, 0,
                                   buffer, sizeof(buffer) - 1);
        if (bytesRead > 0) {
            buffer[bytesRead] = '\0';
            from = buffer;
        } else {
            from = "";
        }

        // MAIL:to
        bytesRead = node.ReadAttr("MAIL:to", B_STRING_TYPE, 0,
                                   buffer, sizeof(buffer) - 1);
        if (bytesRead > 0) {
            buffer[bytesRead] = '\0';
            to = buffer;
        } else {
            to = "";
        }

        // MAIL:subject
        bytesRead = node.ReadAttr("MAIL:subject", B_STRING_TYPE, 0,
                                   buffer, sizeof(buffer) - 1);
        if (bytesRead > 0) {
            buffer[bytesRead] = '\0';
            subject = buffer;
        } else {
            subject = "";
        }

        // MAIL:account - mail_daemon stores this as string on some versions
        // and int32 on others; must check actual attribute type before reading
        attr_info attrInfo;
        if (node.GetAttrInfo("MAIL:account", &attrInfo) == B_OK) {
            if (attrInfo.type == B_STRING_TYPE) {
                bytesRead = node.ReadAttr("MAIL:account", B_STRING_TYPE, 0,
                                          buffer, sizeof(buffer) - 1);
                if (bytesRead > 0) {
                    buffer[bytesRead] = '\0';
                    account = buffer;
                } else {
                    account = "";
                }
            } else if (attrInfo.type == B_INT32_TYPE) {
                int32 accountId;
                if (node.ReadAttr("MAIL:account", B_INT32_TYPE, 0,
                                  &accountId, sizeof(accountId))
                    == sizeof(accountId)) {
                    _ResolveAccountName(accountId);
                } else {
                    account = "";
                }
            } else {
                account = "";
            }
        } else {
            account = "";
        }

        // Draft emails store MAIL:account as "AccountName: User <email>"
        // (the full identity string). Trim to just the account name.
        int32 draftFlag = 0;
        if (node.ReadAttr("MAIL:draft", B_INT32_TYPE, 0,
                          &draftFlag, sizeof(draftFlag)) == sizeof(int32)
            && draftFlag == 1) {
            int32 colonPos = account.FindFirst(':');
            if (colonPos > 0)
                account.Truncate(colonPos);
        }

        // Attachment detection: prefer EMAILVIEWS:attachment (written by
        // EmailViews after parsing the full MIME structure) over MAIL:attachment
        // (set by mail_daemon during download, but not always reliable)
        int8 evAttachFlag = 0;
        bytesRead = node.ReadAttr("EMAILVIEWS:attachment", B_INT8_TYPE, 0,
                                   &evAttachFlag, sizeof(evAttachFlag));
        if (bytesRead == sizeof(int8)) {
            hasAttachment = (evAttachFlag != 0);
        } else {
            int32 attachFlag = 0;
            bytesRead = node.ReadAttr("MAIL:attachment", B_INT32_TYPE, 0,
                                       &attachFlag, sizeof(attachFlag));
            if (bytesRead == sizeof(int32))
                hasAttachment = (attachFlag != 0);
        }

        // FILE:starred
        int32 starFlag = 0;
        bytesRead = node.ReadAttr("FILE:starred", B_INT32_TYPE, 0,
                                   &starFlag, sizeof(starFlag));
        if (bytesRead == sizeof(int32))
            isStarred = (starFlag != 0);
    }
};


#endif // EMAIL_REF_H
