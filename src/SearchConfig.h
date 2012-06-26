#ifndef SEARCHCONFIG_H
#define SEARCHCONFIG_H

#include <string>
#include <vector>

typedef std::vector<std::string> StringList;

class SearchConfig
{
public:
    SearchConfig();
    ~SearchConfig();
    
    void load();
    void save();

    StringList matches_;
    StringList paths_;
    StringList filespecs_;

    int windowX_;
    int windowY_;
    int windowW_;
    int windowH_;
};

#endif