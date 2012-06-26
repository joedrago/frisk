#define _CRT_SECURE_NO_WARNINGS
#include "SearchContext.h"

// ------------------------------------------------------------------------------------------------

ScopedMutex::ScopedMutex(HANDLE mutex)
: mutex_(mutex)
{
    WaitForSingleObject(mutex_, INFINITE);
}

ScopedMutex::~ScopedMutex()
{
    ReleaseMutex(mutex_);
}

// ------------------------------------------------------------------------------------------------

SearchEntry::SearchEntry()
{
}

SearchEntry::~SearchEntry()
{
}

// ------------------------------------------------------------------------------------------------

SearchContext::SearchContext(HWND window)
: thread_(INVALID_HANDLE_VALUE)
, stop_(0)
, offset_(0)
, window_(window)
{
    mutex_ = CreateMutex(NULL, FALSE, NULL);
}

SearchContext::~SearchContext()
{
    stop();
    clear();

    CloseHandle(mutex_);
}

void SearchContext::clear()
{
    ScopedMutex lock(mutex_);

    //for(SearchList::iterator it = list_.begin(); it != list_.end(); it++)
    //{
    //    delete *it;
    //}
    list_.clear();
}

void SearchContext::append(SearchEntry &entry)
{
    lock();

    std::string str = entry.filename_ + "\n";
    offset_ += str.length();

    entry.offset_ = offset_;
    list_.push_back(entry);

    unlock();

    poke(str);

    //OutputDebugString("someone appended: ");
    //OutputDebugString(entry.filename_.c_str());
    //OutputDebugString("\n");
}

void SearchContext::poke(const std::string &str)
{
    if(window_ != INVALID_HANDLE_VALUE)
    {
        SendMessage(window_, WM_SEARCHCONTEXT_POKE, 0, (LPARAM)str.c_str());
    }
}

// ------------------------------------------------------------------------------------------------

static bool filenameMatchesFilespecs(const std::string &filename, const StringList &filespecs)
{
    for(StringList::const_iterator it = filespecs.begin(); it != filespecs.end(); it++)
    {
        if(strstr(filename.c_str(), (*it).c_str()))
        {
            return true;
        }
    }
    return false;
}

#define stopCheck() { if(stop_) goto cleanup; }

bool SearchContext::searchFile(const std::string &filename, SearchEntry &entry)
{
    bool ret = false;
    stopCheck();

    if(!filenameMatchesFilespecs(filename, params_.filespecs))
        return false;

    entry.filename_ = filename;
    entry.match_ = "horee crap this is some sweet matching 456 text";

    static int a = 0;
    entry.line_ = a++;
    ret = true;

cleanup:
    return ret;
}

// ------------------------------------------------------------------------------------------------

static DWORD WINAPI staticSearchProc(void *param)
{
    SearchContext * context = (SearchContext *)param;
    context->searchProc();
    return 0;
}

void SearchContext::search(const SearchParams &params)
{
    stop();
    clear();

    params_ = params;
    stop_ = 0;
    offset_ = 0;

    DWORD id;
    thread_ = CreateThread(NULL, 0, staticSearchProc, (void*)this, 0, &id);
}

void SearchContext::stop()
{
    if(thread_ != INVALID_HANDLE_VALUE)
    {
        stop_ = 1;
        WaitForSingleObject(thread_, INFINITE);
        CloseHandle(thread_);
        thread_ = INVALID_HANDLE_VALUE;
    }
}

void SearchContext::searchProc()
{
    HANDLE findHandle = INVALID_HANDLE_VALUE;
    StringList paths;
    SearchEntry entry;
    paths = params_.paths;

    while(!paths.empty())
    {
        stopCheck();

        std::string currentSearchPath = paths.back();
        std::string currentSearchWildcard = currentSearchPath + "\\*";

        paths.pop_back();

        WIN32_FIND_DATA wfd;
        findHandle = FindFirstFile(currentSearchWildcard.c_str(), &wfd);
        if(findHandle == INVALID_HANDLE_VALUE)
            continue;

        while(FindNextFile(findHandle, &wfd))
        {
            stopCheck();
            if((wfd.cFileName[0] == '.') || (wfd.cFileName[0] == 0))
                continue;

            bool isDirectory = ((wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0);
            std::string filename = currentSearchPath;
            filename += "\\";
            filename += wfd.cFileName;

            if(isDirectory)
            {
                paths.push_back(filename);
            }
            else
            {
                if(searchFile(filename, entry))
                    append(entry);
            }
        }
    }

    for(int i=0; i<5000; i++)
    {
        char buffer[32];
        sprintf(buffer, "noob %d", i);
        entry.filename_ = buffer;
        entry.line_ = i;
        entry.match_ = "bro this matches bro";
        append(entry);
    }

cleanup:
    if(findHandle != INVALID_HANDLE_VALUE)
    {
        FindClose(findHandle);
    }
    return;
}

// ------------------------------------------------------------------------------------------------

void SearchContext::lock()
{
    WaitForSingleObject(mutex_, INFINITE);
}

void SearchContext::unlock()
{
    ReleaseMutex(mutex_);
}

SearchList &SearchContext::list()
{
    return list_;
}

int SearchContext::count()
{
    ScopedMutex lock(mutex_);
    return list_.size();
}

// ------------------------------------------------------------------------------------------------

void test()
{
    SearchContext *context = new SearchContext((HWND)INVALID_HANDLE_VALUE);
    SearchParams params;
    params.paths.push_back("c:\\work\\custom");
    params.filespecs.push_back("txt");
    params.filespecs.push_back("elf");
    context->search(params);
    Sleep(100);
    delete context;
}
