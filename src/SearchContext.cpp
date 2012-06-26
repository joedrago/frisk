#include "SearchContext.h"

#define POKES_PER_SECOND (2)

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
, searchID_(0)
, offset_(0)
, window_(window)
{
    mutex_ = CreateMutex(NULL, FALSE, NULL);
    config_.load();
    lastPoke_ = GetTickCount();
}

SearchContext::~SearchContext()
{
    stop();
    clear();

    config_.save();

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

void SearchContext::append(int id, SearchEntry &entry)
{
    lock();

    std::string str = entry.filename_ + "\n";
    offset_ += str.length();

    entry.offset_ = offset_;
    list_.push_back(entry);

    unlock();

    poke(id, str, false);

    //OutputDebugString("someone appended: ");
    //OutputDebugString(entry.filename_.c_str());
    //OutputDebugString("\n");
}

void SearchContext::poke(int id, const std::string &str, bool finished)
{
    if(!str.empty())
        pokeFlowControl_ += str;

    if(window_ != INVALID_HANDLE_VALUE)
    {
        UINT now = GetTickCount();
        if(finished || (now > (lastPoke_ + (1000 / POKES_PER_SECOND))))
        {
            lastPoke_ = now;
            PostMessage(window_, WM_SEARCHCONTEXT_POKE, id, (LPARAM)strdup(pokeFlowControl_.c_str()));
            pokeFlowControl_.clear();
        }
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

void SearchContext::searchFile(int id, const std::string &filename, SearchEntry &entry)
{
    if(!filenameMatchesFilespecs(filename, params_.filespecs))
        return;

    entry.filename_ = filename;
    entry.match_ = "horee crap this is some sweet matching 456 text";
    static int a = 0;
    entry.line_ = a++;
    append(id, entry);
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
    searchID_++;

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
    int id = searchID_;
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
                searchFile(id, filename, entry);
            }
        }
    }

#if 1
    for(int i=0; i<10000; i++)
    {
        char buffer[32];
        sprintf(buffer, "noob %d", i);
        entry.filename_ = buffer;
        entry.line_ = i;
        entry.match_ = "bro this matches bro";
        append(id, entry);

        Sleep(10);

        stopCheck();
    }
#endif

cleanup:
    if(!stop_)
        poke(id, "", true);
    if(findHandle != INVALID_HANDLE_VALUE)
    {
        FindClose(findHandle);
    }
    pokeFlowControl_ = "";
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

int SearchContext::searchID()
{
    ScopedMutex lock(mutex_);
    return searchID_;
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
