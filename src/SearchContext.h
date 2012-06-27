#ifndef SEARCHCONTEXT_H
#define SEARCHCONTEXT_H

#include <windows.h>
#include <config.h>
#include <pcre.h>

#include "SearchConfig.h"

#define WM_SEARCHCONTEXT_STATE (WM_USER+1)
#define WM_SEARCHCONTEXT_POKE (WM_USER+2)

class ScopedMutex
{
public:
    ScopedMutex(HANDLE mutex);
    ~ScopedMutex();

protected:
    HANDLE mutex_;
};

class SearchEntry
{
public:
    SearchEntry();
    ~SearchEntry();

    std::string filename_;
    std::string match_;
    int line_;
    int offset_;
};

typedef std::vector<SearchEntry> SearchList;
typedef std::vector<pcre *> RegexList;

struct SearchParams
{
    StringList paths;
    StringList filespecs;
    std::string match;
    int flags;
};

class SearchContext
{
public:
    SearchContext(HWND window);
    ~SearchContext();

    void clear();

    void append(int id, SearchEntry &entry);         // takes ownership
    void search(const SearchParams &params); // copies
    void stop();
    void poke(int id, const std::string &str, bool finished);

    std::string generateDisplay(SearchEntry &entry);

    void lock();
    SearchList &list();
    void unlock();

    int count();

    SearchConfig &config() { return config_; }
    int searchID();

    void searchProc();
protected:
    void searchFile(int id, const std::string &filename, RegexList &filespecRegexes, pcre *matchRegex, SearchEntry &entry);

    HWND window_;

    HANDLE mutex_;
    HANDLE thread_;
    int stop_;
    int searchID_;
    int offset_;
    unsigned int lastPoke_;
    std::string pokeFlowControl_;
    SearchList list_;
    SearchParams params_;
    SearchConfig config_;
};

#endif
