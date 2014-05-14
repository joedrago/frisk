// ---------------------------------------------------------------------------
//                   Copyright Joe Drago 2012.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#ifndef SEARCHCONTEXT_H
#define SEARCHCONTEXT_H

#include <windows.h>
#include <config.h>
#include <pcre.h>

#include "SearchConfig.h"

#define WM_SEARCHCONTEXT_STATE (WM_USER+1)
#define WM_SEARCHCONTEXT_POKE (WM_USER+2)

void replaceAll(std::string &s, const char *f, const char *r);
std::string getWindowText(HWND ctrl);
void setWindowText(HWND ctrl, const std::string &s);
bool ctrlIsChecked(HWND ctrl);
void checkCtrl(HWND ctrl, bool checked);

class ScopedMutex
{
public:
    ScopedMutex(HANDLE mutex);
    ~ScopedMutex();

protected:
    HANDLE mutex_;
};

struct TextBlock
{
	std::string text;
	int color;
    bool link;
};

class TextBlockList : public std::deque<TextBlock>
{
public:
    void addBlock(const std::string &text, int color, bool link = false)
    {
        push_back(TextBlock());
        TextBlock &lastOne = back();
        lastOne.text = text;
        lastOne.color = color;
        lastOne.link = link;
    }

	void addHighlightedBlock(const std::string &text, int offset, int count, int textColor, int highlightedColor, bool link = false)
	{
		std::string pre = text.substr(0, offset);
		std::string highlighted = text.substr(offset, count);
		std::string post = text.substr(offset + count);

		addBlock(pre, textColor);
		addBlock(highlighted, highlightedColor, link);
		addBlock(post, textColor);
	}
};

class SearchEntry
{
public:
    SearchEntry();
    ~SearchEntry();

    std::string filename_;
    int line_;
	int offset_;
	bool contextOnly_;

	TextBlockList textBlocks;
};

typedef std::vector<SearchEntry> SearchList;
typedef std::vector<pcre *> RegexList;

struct SearchParams
{
    StringList paths;
    StringList filespecs;
    std::string match;
    std::string replace;
	std::string backupExtension;
    s64 maxFileSize;
    int flags;
};

struct PokeData // pika, pika!
{
    std::string progress;

	TextBlockList textBlocks;
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
    void poke(int id, TextBlockList &textBlocks, bool finished);

    void makePretty(SearchEntry &entry);

	void sendError(int id, const std::string &error);

    void lock();
    SearchList &list();
    void unlock();

    int count();

    SearchConfig &config() { return config_; }
    int searchID();

    void searchProc();
protected:
    bool searchFile(int id, const std::string &filename, RegexList &filespecRegexes, pcre *matchRegex);

    int directoriesSearched_;
    int directoriesSkipped_;
    int filesSearched_;
    int filesSkipped_;
    int filesWithHits_;
    int linesWithHits_;
    int hits_;

    HWND window_;

    HANDLE mutex_;
    HANDLE thread_;
    int stop_;
    int searchID_;
    int offset_;
    unsigned int lastPoke_;
	std::string lastFilename_;
	int lastLine_;
	PokeData *pokeData_;
    SearchList list_;
    SearchParams params_;
    SearchConfig config_;
};

#endif
