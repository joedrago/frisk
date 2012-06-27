#include "SearchContext.h"

#include <algorithm>
#include <stdio.h>

#define POKES_PER_SECOND (5)

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

    list_.clear();
}

std::string SearchContext::generateDisplay(SearchEntry &entry)
{
    std::string s;
    int requiredLen = _snprintf(NULL, 0, "%30s(%d): %s\n", entry.filename_.c_str(), entry.line_, entry.match_.c_str());
    if(requiredLen > 0)
    {
        s.resize(requiredLen+1);
        _snprintf(&s[0], requiredLen, "%30s(%d): %s\n", entry.filename_.c_str(), entry.line_, entry.match_.c_str());
        s.resize(requiredLen);
    }
    return s;
}

void SearchContext::append(int id, SearchEntry &entry)
{
    lock();

    std::string str = generateDisplay(entry);
    offset_ += str.length();

    entry.offset_ = offset_;
    list_.push_back(entry);

    unlock();

    poke(id, str, false);
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

static void replaceAll(std::string &s, const char *f, const char *r)
{
    int flen = strlen(f);
    int rlen = strlen(r);
    int pos = 0 - rlen;
    while((pos = s.find(f, pos + rlen)) != -1)
    {
        s.replace(pos, flen, r);
    }
}

void convertWildcard(std::string &regex)
{
    // Pretty terrible and crazy stuff.
    replaceAll(regex, "[", "\\[");
    replaceAll(regex, "]", "\\]");
    replaceAll(regex, "\\", "\\\\");
    replaceAll(regex, ".", "\\.");
    replaceAll(regex, "*", ".*");
    replaceAll(regex, "?", ".?");
    regex.insert(0, "^");
    regex.append("$");
}

#define stopCheck() { if(stop_) goto cleanup; }

static char * strstri(char * haystack, const char * needle)
{
    char *front = haystack;
    for(; *front; front++)
    {
        const char *a = front;
        const char *b = needle;

        while(*a && *b)
        {
            if(tolower(*a) != tolower(*b))
                break;
            a++;
            b++;
        }
        if(!*b)
            return front;
    }
    return NULL;
}

void SearchContext::searchFile(int id, const std::string &filename, RegexList &filespecRegexes, pcre *matchRegex, SearchEntry &entry)
{
    bool matchesOneFilespec = false;
    for(RegexList::iterator it = filespecRegexes.begin(); it != filespecRegexes.end(); it++)
    {
        pcre *regex = *it;
        if(pcre_exec(regex, NULL, filename.c_str(), filename.length(), 0, 0, NULL, 0) >= 0)
        {
            matchesOneFilespec = true;
            break;
        }
    }
    if(!matchesOneFilespec)
        return;

    std::string contents;
    if(!readEntireFile(filename, contents))
        return;

    int lineNumber = 1;
    const char *seps = "\n";
    for(char *line = strtok(&contents[0], seps); line != NULL; lineNumber++, line = strtok(NULL, seps))
    {
        bool matches = false;
        if(matchRegex)
        {
            if(pcre_exec(matchRegex, NULL, line, strlen(line), 0, 0, NULL, 0) >= 0)
            {
                matches = true;
            }
        }
        else
        {
            if(params_.flags & SF_MATCH_CASE_SENSITIVE)
                matches = ( strstr(line, params_.match.c_str()) != NULL );
            else
                matches = ( strstri(line, params_.match.c_str()) != NULL );
        }

        if(matches)
        {
            entry.filename_ = filename;
            entry.match_ = line;
            entry.line_ = lineNumber;
            append(id, entry);
        }
    }
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
    RegexList filespecRegexes;
    pcre *matchRegex = NULL;

    bool filespecUsesRegexes = ((params_.flags & SF_FILESPEC_REGEXES) != 0);
    bool matchUsesRegexes    = ((params_.flags & SF_MATCH_REGEXES) != 0);

    if(matchUsesRegexes)
    {
        const char *error;
        int erroffset;
        int flags = 0;
        if(!(params_.flags & SF_MATCH_CASE_SENSITIVE))
            flags |= PCRE_CASELESS;
        matchRegex = pcre_compile(params_.match.c_str(), flags, &error, &erroffset, NULL);
        if(!matchRegex)
        {
            MessageBox(window_, error, "Match Regex Error", MB_OK);
            goto cleanup;
        }
    }

    for(StringList::iterator it = params_.filespecs.begin(); it != params_.filespecs.end(); it++)
    {
        std::string regexString = it->c_str();
        if(!filespecUsesRegexes)
            convertWildcard(regexString);

        int flags = 0;
        if(!(params_.flags & SF_FILESPEC_CASE_SENSITIVE))
            flags |= PCRE_CASELESS;

        const char *error;
        int erroffset;
        pcre *regex = pcre_compile(regexString.c_str(), flags, &error, &erroffset, NULL);
        if(regex)
            filespecRegexes.push_back(regex);
        else
        {
            MessageBox(window_, error, "Filespec Regex Error", MB_OK);
            goto cleanup;
        }
    }

    PostMessage(window_, WM_SEARCHCONTEXT_STATE, 1, 0);

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
                searchFile(id, filename, filespecRegexes, matchRegex, entry);
            }
        }
    }

#if 0
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
    for(RegexList::iterator it = filespecRegexes.begin(); it != filespecRegexes.end(); it++)
    {
        pcre_free(*it);
    }
    if(matchRegex)
        pcre_free(matchRegex);
    filespecRegexes.clear();
    if(!stop_)
        poke(id, "", true);
    if(findHandle != INVALID_HANDLE_VALUE)
    {
        FindClose(findHandle);
    }
    pokeFlowControl_ = "";
    PostMessage(window_, WM_SEARCHCONTEXT_STATE, 0, 0);
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
