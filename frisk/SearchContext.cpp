// ---------------------------------------------------------------------------
//                   Copyright Joe Drago 2012.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "SearchContext.h"

#include <algorithm>
#include <stdio.h>
#include <deque>

#define POKES_PER_SECOND (5)

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

// ------------------------------------------------------------------------------------------------

HANDLE WTFMUTEX;

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
    : contextOnly_(false)
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
, window_(window)
, pokeData_(NULL)
{
    WTFMUTEX = CreateMutex(NULL, FALSE, NULL);

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

void SearchContext::makePretty(SearchEntry &entry)
{
    if(entry.filename_.empty())
        return;

    std::string s;
    TextBlockList blocks;

    if(lastFilename_ != entry.filename_)
    {
        s = entry.filename_.c_str();
        if(params_.flags & SF_TRIM_FILENAMES)
        {
            std::string &startingPath = params_.paths[0];
            if(strstri((char *)s.c_str(), startingPath.c_str()) == s.c_str())
            {
                s = s.substr(startingPath.length());
                if(s.length() && (s[0] == '\\'))
                {
                    s.erase(s.begin());
                }
            }
        }

        s.insert(0, "\n");
        s += ":\n";
        blocks.addBlock(s, config_.contextColor_);

        lastFilename_ = entry.filename_;
    }
    else if(entry.line_ != (lastLine_ + 1))
    {
        blocks.addBlock("  ...\n", config_.contextColor_);
    }

    lastLine_ = entry.line_;

    char lineNo[64];
    sprintf(lineNo, "%5d%s ", entry.line_, entry.contextOnly_ ? " " : ":");
    blocks.addBlock(lineNo, config_.contextColor_);

    for(TextBlockList::iterator it = entry.textBlocks.begin(); it != entry.textBlocks.end(); ++it)
    {
        blocks.push_back(*it);
    }

    blocks.addBlock("\n", config_.textColor_);

    blocks.swap(entry.textBlocks);
}

void SearchContext::sendError(int id, const std::string &error)
{
    TextBlockList textBlocks;
    textBlocks.addBlock(error, RGB(255, 0, 0));
    poke(id, textBlocks, false);
}

void SearchContext::append(int id, SearchEntry &entry)
{
    lock();

    makePretty(entry);

    TextBlockList textBlocks;
    textBlocks.swap(entry.textBlocks);
    for(TextBlockList::iterator it = textBlocks.begin(); it != textBlocks.end(); ++it)
    {
        offset_ += it->text.size();
    }
    entry.offset_ = offset_;
    list_.push_back(entry);

    unlock();

    poke(id, textBlocks, false);
}

void SearchContext::poke(int id, TextBlockList &textBlocks, bool finished)
{
    if(textBlocks.size())
        pokeData_->textBlocks.swap(textBlocks);

    if(window_ != INVALID_HANDLE_VALUE)
    {
        UINT now = GetTickCount();
        if(finished || (now > (lastPoke_ + (1000 / POKES_PER_SECOND))) || pokeData_->textBlocks.size())
        {
            lastPoke_ = now;

            char buffer[256];
            sprintf(buffer, "%d hits, %d dirs, %d files", hits_, directoriesSearched_+directoriesSkipped_, filesSearched_+filesSkipped_);
            if(!finished)
                pokeData_->progress = buffer;

            PostMessage(window_, WM_SEARCHCONTEXT_POKE, id, (LPARAM)pokeData_);
            pokeData_ = new PokeData;
        }
    }
}

// ------------------------------------------------------------------------------------------------

void replaceAll(std::string &s, const char *f, const char *r)
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

static char *nextToken(char **p, char sep)
{
    char *front = *p;
    if(!front || !*front)
        return NULL;

    char *end = front;
    while(*end && (*end != sep))
    {
        end++;
    }
    if(*end == sep)
    {
        *end = 0;
        *p = end+1;
    }
    else
    {
        *p = NULL;
    }
    return front;
}

bool SearchContext::searchFile(int id, const std::string &filename, RegexList &filespecRegexes, pcre *matchRegex)
{
    bool matchesOneFilespec = false;
    for(RegexList::iterator it = filespecRegexes.begin(); it != filespecRegexes.end(); ++it)
    {
        pcre *regex = *it;
        if(pcre_exec(regex, NULL, filename.c_str(), filename.length(), 0, 0, NULL, 0) >= 0)
        {
            matchesOneFilespec = true;
            break;
        }
    }
    if(!matchesOneFilespec)
        return false;

    std::string contents;
    if(!readEntireFile(filename, contents, params_.maxFileSize))
        return false;

    std::string workBuffer = contents;
    std::string updatedContents;

    std::deque<char *> contextLines;

    bool atLeastOneMatch = false;

    int trailingContextLines = 0;
    int lineNumber = 1;
    char *p = &workBuffer[0];
    char *line;
    while((line = nextToken(&p, '\n')) != NULL)
    {
        char *originalLine = line;
        std::string replacedLine;
        SearchEntry entry;
        int ovector[100];

        // Strip newline, but remember exactly what kind it was
        bool hasCarriageReturn = false;
        int lineLen = strlen(line);
        if(lineLen && (line[lineLen - 1] == '\r'))
        {
            line[lineLen - 1] = 0;
            hasCarriageReturn = true;
        }

        // Matching loop (we might find our string a few times on a single line)
        bool lineMatched = false;
        do
        {
            bool matches = false;
            int matchPos;
            int matchLen;

            // The actual match. Either invoke PCRE or do a boring strstr
            if(matchRegex)
            {
                int rc;
                if(rc = pcre_exec(matchRegex, 0, line, strlen(line), 0, 0, ovector, sizeof(ovector)) >= 0)
                {
                    matches = true;
                    matchPos = ovector[0];
                    matchLen = ovector[1] - ovector[0];
                }
            }
            else
            {
                char *match;
                if(params_.flags & SF_MATCH_CASE_SENSITIVE)
                    match = strstr(line, params_.match.c_str());
                else
                    match = strstri(line, params_.match.c_str());
                if(match != NULL)
                {
                    matches = true;
                    matchPos = match - line;
                    matchLen = params_.match.length();
                }
            }

            if(matches)
                lineMatched = true;

            // Handle the match. For replace or find, we:
            // * Add output explaining the match
            // * Advance the line pointer for another match attempt
            // ... or ...
            // * "break", which leaves the matching loop
            if(params_.flags & SF_REPLACE)
            {
                if(matches)
                {
                    std::string temp(line, matchPos);
                    replacedLine.append(temp);
                    replacedLine.append(params_.replace);
                    entry.textBlocks.addBlock(temp, config_.textColor_);
                    entry.textBlocks.addBlock(params_.replace, config_.highlightColor_, true);
                    line += matchPos + matchLen;
                }
                else
                {
                    break;
                }
            }
            else
            {
                if(matches)
                {
                    entry.textBlocks.addHighlightedBlock(originalLine, matchPos + (line - originalLine), matchLen, config_.textColor_, config_.highlightColor_, true);
                    line += matchPos + matchLen;
                }
                else
                {
                    break;
                }
            }

            if(matches)
                hits_++;
        }
        while(*line); // end of matching loop

        // If we're doing a replace, finish the line and append to the final updated contents
        if(params_.flags & SF_REPLACE)
        {
            if(line && *line)
            {
                replacedLine += line;
                entry.textBlocks.addBlock(line, config_.textColor_);
            }
            if(hasCarriageReturn)
            {
                replacedLine += "\r";
            }
            replacedLine += "\n";
            updatedContents += replacedLine;
        }

        bool outputMatch = false;
        if(lineMatched)
        {
            // keep stats
            linesWithHits_++;
            atLeastOneMatch = true;

            // If we matched, consider notifying the user. We'll always say something
            // unless the replaced text doesn't actually change the line.
            outputMatch = ( !(params_.flags & SF_REPLACE) ) || (replacedLine != originalLine);

            if(outputMatch)
            {
                entry.filename_ = filename;
                entry.line_ = lineNumber;

                // output all existing context lines
                if(contextLines.size())
                {
                    SearchEntry contextEntry;
                    contextEntry.filename_ = entry.filename_;
                    contextEntry.contextOnly_ = true;
                    int currLine = entry.line_ - contextLines.size();
                    for(std::deque<char *>::iterator it = contextLines.begin(); it != contextLines.end(); ++it)
                    {
                        TextBlockList textBlocks;
                        textBlocks.addBlock(*it, config_.textColor_);
                        contextEntry.textBlocks.swap(textBlocks);
                        contextEntry.line_ = currLine++;
                        append(id, contextEntry);
                    }

                    contextLines.clear();
                }

                append(id, entry);
            }

            // Remember that we'd like the next few lines, even if they don't match
            trailingContextLines = config_.contextLines_;
        }

        if(!outputMatch)
        {
            // Didn't output a match. Keep track or output the line anyway for contextual reasons.

            if(trailingContextLines > 0)
            {
                // A recent match wants to see this line in the output anyway

                SearchEntry trailingEntry;
                trailingEntry.filename_ = filename;
                trailingEntry.line_ = lineNumber;
                trailingEntry.contextOnly_ = true;
                trailingEntry.textBlocks.addBlock(originalLine, config_.textColor_);
                append(id, trailingEntry);
                trailingContextLines--;
            }
            else
            {
                // didn't output a match, and wasn't output as context. stash it in contextLines

                contextLines.push_back(originalLine);
                if((int)contextLines.size() > config_.contextLines_)
                    contextLines.pop_front();
            }
        }

        lineNumber++;
    } // end of line loop (done reading file)

    if(atLeastOneMatch)
        filesWithHits_++;

    if(params_.flags & SF_REPLACE)
    {
        if((contents != updatedContents))
        {
            bool overwriteFile = true;
            if(params_.flags & SF_BACKUP)
            {
                std::string backupFilename = filename;
                backupFilename += ".";
                backupFilename += params_.backupExtension;

                if(!writeEntireFile(backupFilename, contents))
                {
                    std::string err = "WARNING: Couldn't write backup file (skipping replacement): ";
                    err += backupFilename;
                    err += "\n";
                    sendError(id, err);

                    overwriteFile = false;
                }
            }

            if(overwriteFile)
            {
                if(writeEntireFile(filename, updatedContents))
                {
                    return true;
                }
                else
                {
                    std::string err = "WARNING: Couldn't write to file: ";
                    err += filename;
                    err += "\n";
                    sendError(id, err);
                }
            }
        }
        return false;
    }
    return true;
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
    paths = params_.paths;
    RegexList filespecRegexes;
    pcre *matchRegex = NULL;

    directoriesSearched_ = 0;
    directoriesSkipped_ = 0;
    filesSearched_ = 0;
    filesSkipped_ = 0;
    filesWithHits_ = 0;
    linesWithHits_ = 0;
    hits_ = 0;
    lastFilename_ = "";

    unsigned int startTick = GetTickCount();

    bool filespecUsesRegexes = ((params_.flags & SF_FILESPEC_REGEXES) != 0);
    bool matchUsesRegexes    = ((params_.flags & SF_MATCH_REGEXES) != 0);

    delete pokeData_;
    pokeData_ = new PokeData;

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

    for(StringList::iterator it = params_.filespecs.begin(); it != params_.filespecs.end(); ++it)
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
        directoriesSearched_++;
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
            bool isDirectory = ((wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0);

            if((wfd.cFileName[0] == '.') || (wfd.cFileName[0] == 0))
            {
                if(isDirectory)
                    directoriesSkipped_++;
                else
                    filesSkipped_++;
                continue;
            }

            std::string filename = currentSearchPath;
            if(!filename.length() || (filename[filename.length() - 1] != '\\'))
            {
                filename += "\\";
            }
            filename += wfd.cFileName;

            if(isDirectory)
            {
                if(params_.flags & SF_RECURSIVE)
                    paths.push_back(filename);
            }
            else
            {
                if(searchFile(id, filename, filespecRegexes, matchRegex))
                {
                    filesSearched_++;
                }
                else
                {
                    filesSkipped_++;
                }

                poke(id, TextBlockList(), false);
            }
        }

        if(findHandle != INVALID_HANDLE_VALUE)
        {
            FindClose(findHandle);
        }
    }

cleanup:
    for(RegexList::iterator it = filespecRegexes.begin(); it != filespecRegexes.end(); ++it)
    {
        pcre_free(*it);
    }
    if(matchRegex)
        pcre_free(matchRegex);
    filespecRegexes.clear();
    if(!stop_)
    {
        unsigned int endTick = GetTickCount();
        char buffer[512];
        float sec = (endTick - startTick) / 1000.0f;
        const char *verb = "searched";
        if(params_.flags & SF_REPLACE)
            verb = "updated";
        sprintf(buffer, "\n%d hits in %d lines across %d files.\n%d directories scanned, %d files %s, %d files skipped (%3.3f sec)",
            hits_,
            linesWithHits_,
            filesWithHits_,
            directoriesSearched_,
            filesSearched_,
            verb,
            filesSkipped_,
            sec);

        TextBlockList textBlocks;
        textBlocks.addBlock(buffer, config_.textColor_);
        poke(id, textBlocks, true);
    }
    delete pokeData_;
    pokeData_ = NULL;
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
