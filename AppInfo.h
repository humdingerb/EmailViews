/*
 * AppInfo.h - Application identity constants
 * Distributed under the terms of the MIT License.
 */

#ifndef APP_INFO_H
#define APP_INFO_H

// BUILD_TIMESTAMP is injected by the Makefile via -D flag at compile time
// (format: "YYYY-MM-DD HH:MM"). Falls back to __DATE__ for IDE builds.
#ifndef BUILD_TIMESTAMP
#define BUILD_TIMESTAMP __DATE__
#endif

static const char* kAppName = "EmailViews";

#endif // APP_INFO_H
