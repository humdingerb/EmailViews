/*
 * EmailAccountMap.h - Singleton that maps account IDs to names
 * Distributed under the terms of the MIT License.
 * 
 * Some emails store MAIL:account as an int32 ID instead of a string.
 * This class loads the mapping from ~/config/settings/Mail/accounts/
 */

#ifndef EMAIL_ACCOUNT_MAP_H
#define EMAIL_ACCOUNT_MAP_H

#include <String.h>
#include <map>

class EmailAccountMap {
public:
    // Singleton access
    static EmailAccountMap& Instance() {
        static EmailAccountMap instance;
        return instance;
    }
    
    // Get account name from ID, returns empty string if not found
    BString GetAccountName(int32 accountId);
    
    // Get copy of the map (for use in background threads)
    const std::map<int32, BString>& GetMap() const { return fMap; }
    
private:
    EmailAccountMap();
    ~EmailAccountMap() {}
    
    // Non-copyable
    EmailAccountMap(const EmailAccountMap&) = delete;
    EmailAccountMap& operator=(const EmailAccountMap&) = delete;
    
    void _LoadAccountMappings();
    
    std::map<int32, BString> fMap;
};

#endif // EMAIL_ACCOUNT_MAP_H
