/*
 * EmailAccountMap.cpp - Implementation of account ID to name mapping
 * Distributed under the terms of the MIT License.
 */

#include "EmailAccountMap.h"

#include <Directory.h>
#include <Entry.h>
#include <File.h>
#include <FindDirectory.h>
#include <Message.h>
#include <Path.h>

#include <cstdio>


EmailAccountMap::EmailAccountMap()
{
    _LoadAccountMappings();
}


BString
EmailAccountMap::GetAccountName(int32 accountId)
{
    auto it = fMap.find(accountId);
    if (it != fMap.end())
        return it->second;
    
    // Not found - return empty string
    return BString();
}



void
EmailAccountMap::_LoadAccountMappings()
{
    // Find the Mail accounts directory
    BPath accountsPath;
    if (find_directory(B_USER_SETTINGS_DIRECTORY, &accountsPath) != B_OK)
        return;
    
    accountsPath.Append("Mail/accounts");
    
    BDirectory accountsDir(accountsPath.Path());
    if (accountsDir.InitCheck() != B_OK)
        return;
    
    // Each file in ~/config/settings/Mail/accounts/ is a flattened BMessage
    // with "id" (int32) and "name" (string) fields, written by mail_daemon's
    // Accounts settings.
    BEntry entry;
    while (accountsDir.GetNextEntry(&entry) == B_OK) {
        // Skip non-files
        if (!entry.IsFile())
            continue;
        
        // Try to read the account settings
        BFile file(&entry, B_READ_ONLY);
        if (file.InitCheck() != B_OK)
            continue;
        
        BMessage accountMsg;
        if (accountMsg.Unflatten(&file) != B_OK)
            continue;
        
        // Extract id and name
        int32 accountId;
        const char* accountName;
        
        if (accountMsg.FindInt32("id", &accountId) == B_OK &&
            accountMsg.FindString("name", &accountName) == B_OK) {
            fMap[accountId] = accountName;
        }
    }
    
    
    #ifdef DEBUG
    printf("EmailAccountMap: Loaded %ld account mappings\n", fMap.size());
    #endif
}
