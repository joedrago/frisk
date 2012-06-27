#ifndef SEARCHCONFIG_H
#define SEARCHCONFIG_H

#include <string>
#include <vector>

typedef std::vector<std::string> StringList;
bool readEntireFile(const std::string &filename, std::string &contents);

enum SearchFlag
{
    SF_RECURSIVE               = (1 << 0),
    SF_FILESPEC_REGEXES        = (1 << 1),
    SF_FILESPEC_CASE_SENSITIVE = (1 << 2),
    SF_MATCH_REGEXES           = (1 << 3),
    SF_MATCH_CASE_SENSITIVE    = (1 << 4),

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

    int windowX_;
    int windowY_;
    int windowW_;
    int windowH_;
    int windowMaximized_;
    int flags_;
    int textColor_;
    int backgroundColor_;
};

#endif