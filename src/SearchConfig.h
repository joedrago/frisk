// ---------------------------------------------------------------------------
//                   Copyright Joe Drago 2012.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#ifndef SEARCHCONFIG_H
#define SEARCHCONFIG_H

#include <string>
#include <vector>

typedef long long s64;

typedef std::vector<std::string> StringList;
bool readEntireFile(const std::string &filename, std::string &contents, s64 maxSizeKb);
bool writeEntireFile(const std::string &filename, const std::string &contents);

enum SearchFlag
{
    SF_RECURSIVE               = (1 << 0),
    SF_FILESPEC_REGEXES        = (1 << 1),
    SF_FILESPEC_CASE_SENSITIVE = (1 << 2),
    SF_MATCH_REGEXES           = (1 << 3),
    SF_MATCH_CASE_SENSITIVE    = (1 << 4),
    SF_REPLACE                 = (1 << 5),
    SF_BACKUP                  = (1 << 6),
	SF_TRIM_FILENAMES          = (1 << 7),

    SF_COUNT
};

class SearchConfig
{
public:
    SearchConfig();
    ~SearchConfig();

    void load();
    void save();

    std::string cmdTemplate_;

    StringList matches_;
    StringList paths_;
    StringList filespecs_;
    StringList replaces_;
	StringList backupExtensions_;
    StringList fileSizes_;

    int windowX_;
    int windowY_;
    int windowW_;
    int windowH_;
    int windowMaximized_;
    int flags_;
	int textSize_;
    int textColor_;
    int backgroundColor_;
	int highlightColor_;
};

#endif
