// ---------------------------------------------------------------------------
//                   Copyright Joe Drago 2012.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "SearchContext.h"

#include <algorithm>
#include <stdio.h>

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
, pokeData_(NULL)
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

std::string SearchContext::generateDisplay(SearchEntry &entry, int &textOffset)
{
	char middle[64];
	sprintf(middle, "(%d): ", entry.line_);

	std::string s = entry.filename_.c_str();
	if(params_.flags & SF_TRIM_FILENAMES)
	{
		std::string &startingPath = params_.paths[0];
		if(strstri((char *)s.c_str(), startingPath.c_str()) == s.c_str())
		{
			s = s.substr(startingPath.length() + 1);
		}
	}

	s += middle;
	textOffset = s.length();
	s += entry.match_;
	s += "\n";
    return s;
}

void SearchContext::append(int id, SearchEntry &entry)
{
    lock();

	int textOffset = 0;
    std::string str = generateDisplay(entry, textOffset);
    offset_ += str.length() - 1;

    entry.offset_ = offset_;
    list_.push_back(entry);

    unlock();

    poke(id, str, entry.highlights_, textOffset, false);
}

void SearchContext::poke(int id, const std::string &str, HighlightList &highlights, int highlightOffset, bool finished)
{
    if(!str.empty())
	{
		int additionalOffset = pokeData_->text.length() + highlightOffset;
        pokeData_->text += str;
		for(HighlightList::iterator it = highlights.begin(); it != highlights.end(); ++it)
		{
			pokeData_->highlights.push_back(Highlight(it->offset + additionalOffset, it->count));
		}
	}

    if(window_ != INVALID_HANDLE_VALUE)
    {
        UINT now = GetTickCount();
        if(finished || (now > (lastPoke_ + (1000 / POKES_PER_SECOND))))
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

    bool atLeastOneMatch = false;

    int lineNumber = 1;
    char *p = &workBuffer[0];
    char *line;
    while((line = nextToken(&p, '\n')) != NULL)
    {
        char *originalLine = line;
        std::string replacedLine;
		SearchEntry entry;
        int ovector[100];
        do
        {
            bool matches = false;
            int matchPos;
            int matchLen;

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

            if(params_.flags & SF_REPLACE)
            {
                if(matches)
                {
                    replacedLine.append(line, matchPos);
					entry.highlights_.push_back(Highlight(replacedLine.length(), params_.replace.length()));
                    replacedLine.append(params_.replace);
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
                    entry.filename_ = filename;
                    entry.match_ = originalLine;
                    entry.line_ = lineNumber;
					entry.highlights_.push_back(Highlight(matchPos + (line - originalLine), matchLen));
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
        while(*line);

        if(!entry.filename_.empty())
        {
            linesWithHits_++;
            atLeastOneMatch = true;
        }

        if(params_.flags & SF_REPLACE)
        {
            if(line && *line)
                replacedLine += line;
            if(replacedLine != originalLine)
            {
                entry.filename_ = filename;
                entry.match_ = replacedLine;
                entry.line_ = lineNumber;
                append(id, entry);
            }
            replacedLine += "\n";
            updatedContents += replacedLine;
        }
		else
		{
			if(!entry.filename_.empty())
				append(id, entry);
		}
		lineNumber++;
    }
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
					poke(id, err.c_str(), HighlightList(), 0, false);

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
					poke(id, err.c_str(), HighlightList(), 0, false);
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
            filename += "\\";
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
                poke(id, "", HighlightList(), 0, false);
            }
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
        poke(id, buffer, HighlightList(), 0, true);
    }
    if(findHandle != INVALID_HANDLE_VALUE)
    {
        FindClose(findHandle);
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
