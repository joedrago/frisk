#ifndef SEARCHCONTEXT_H
#define SEARCHCONTEXT_H

#include <windows.h>

#include <string>
#include <vector>

#define WM_SEARCHCONTEXT_POKE (WM_USER+1)

typedef std::vector<std::string> StringList;

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

    void append(SearchEntry &entry);         // takes ownership
    void search(const SearchParams &params); // copies
    void stop();
    void poke(const std::string &str);

    void lock();
    SearchList &list();
    void unlock();

    int count();

    void searchProc();
protected:
    bool searchFile(const std::string &filename, SearchEntry &entry);

    HWND window_;

    HANDLE mutex_;
    HANDLE thread_;
    int stop_;
    int offset_;
    SearchList list_;
    SearchParams params_;
};

#endif
