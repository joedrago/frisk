// ---------------------------------------------------------------------------
//                   Copyright Joe Drago 2012.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "FriskWindow.h"
#include "SettingsWindow.h"
#include "resource.h"

#include <Shlobj.h>

static FriskWindow *sWindow = NULL;

// ------------------------------------------------------------------------------------------------
// Helpers

static bool hasWindowText(HWND ctrl)
{
    int len = GetWindowTextLength(ctrl);
    return (len > 0);
}

static bool ctrlIsChecked(HWND ctrl)
{
    return (SendMessage(ctrl, BM_GETCHECK, 0, 0) == BST_CHECKED);
}

static void checkCtrl(HWND ctrl, bool checked)
{
    PostMessage(ctrl, BM_SETCHECK, checked ? BST_CHECKED : BST_UNCHECKED, 0);
}

std::string getWindowText(HWND ctrl)
{
    std::string str;
    int len = GetWindowTextLength(ctrl);
    if(len > 0)
    {
        str.resize(len+1);
        GetWindowText(ctrl, &str[0], len+1);
        str.resize(len);
    }
    return str;
}

void setWindowText(HWND ctrl, const std::string &s)
{
    SetWindowText(ctrl, s.c_str());
}

static void comboClear(HWND ctrl)
{
    SendMessage(ctrl, CB_RESETCONTENT, 0, 0);
}

static void comboSet(HWND ctrl, StringList &list)
{
    comboClear(ctrl);
    for(StringList::iterator it = list.begin(); it != list.end(); it++)
    {
        SendMessage(ctrl, CB_ADDSTRING, 0, (LPARAM)it->c_str());
    }
    SendMessage(ctrl, CB_SETCURSEL, 0, 0);
    SendMessage(ctrl, CB_SETEDITSEL, 0, MAKEWORD(-1, -1));
}

static void comboLRU(HWND ctrl, StringList &list, unsigned int maxRecent)
{
    std::string chosen = getWindowText(ctrl);

    for(StringList::iterator it = list.begin(); it != list.end(); it++)
    {
        int a = it->compare(chosen);
        if(!it->compare(chosen))
        {
            list.erase(it);
            break;
        }
    }

    while(list.size() > maxRecent)
        list.pop_back();

    list.insert(list.begin(), chosen);
    comboSet(ctrl, list);
}

static void split(const std::string &orig, const char *delims, StringList &output)
{
    output.clear();
    std::string workBuffer = orig;
    char *rawString = &workBuffer[0];
    for(char *token = strtok(rawString, delims); token != NULL; token = strtok(NULL, delims))
    {
        if(token[0])
            output.push_back(std::string(token));
    }
}

// ------------------------------------------------------------------------------------------------

FriskWindow::FriskWindow(HINSTANCE instance)
: instance_(instance)
, dialog_((HWND)INVALID_HANDLE_VALUE)
, outputCtrl_((HWND)INVALID_HANDLE_VALUE)
, pathCtrl_((HWND)INVALID_HANDLE_VALUE)
, filespecCtrl_((HWND)INVALID_HANDLE_VALUE)
, matchCtrl_((HWND)INVALID_HANDLE_VALUE)
, replaceCtrl_((HWND)INVALID_HANDLE_VALUE)
, context_(NULL)
, running_(false)
, closing_(false)
{
    sWindow = this;

    HDC dc = GetDC(NULL);
    font_ = CreateFont(-MulDiv(8, GetDeviceCaps(dc, LOGPIXELSY), 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, "Courier New");
    ReleaseDC(NULL, dc);
}

FriskWindow::~FriskWindow()
{
    DeleteObject(font_);
    delete context_;
    sWindow = NULL;
}

// ------------------------------------------------------------------------------------------------

void FriskWindow::outputClear()
{
    CHARRANGE charRange;
    charRange.cpMin = 0;
    charRange.cpMax = -1;
    SendMessage(outputCtrl_, EM_EXSETSEL, 0, (LPARAM)&charRange);
    SendMessage(outputCtrl_, EM_REPLACESEL, FALSE, (LPARAM)"");
}

void FriskWindow::outputUpdatePos()
{
    RECT clientRect;
    RECT outputRect;

    GetClientRect(dialog_, &clientRect);
    GetWindowRect(outputCtrl_, &outputRect);
    POINT outputPos;
    outputPos.x = outputRect.left;
    outputPos.y = outputRect.top;
    ScreenToClient(dialog_, &outputPos);

    //outputRect.right = 5;
    MoveWindow(outputCtrl_, outputPos.x, /*outputPos.y */ 0, clientRect.right - outputPos.x + 2, clientRect.bottom - outputPos.y + 2, TRUE);
}

void FriskWindow::outputUpdateColors()
{
    SendMessage(outputCtrl_, EM_SETBKGNDCOLOR, 0, (LPARAM)config_->backgroundColor_);
    InvalidateRect(outputCtrl_, NULL, TRUE);
}

void FriskWindow::configToControls()
{
    comboSet(pathCtrl_, config_->paths_);
    comboSet(matchCtrl_, config_->matches_);
    comboSet(filespecCtrl_, config_->filespecs_);
    comboSet(replaceCtrl_, config_->replaces_);

    checkCtrl(GetDlgItem(dialog_, IDC_RECURSIVE),      0 != (config_->flags_ & SF_RECURSIVE));
    checkCtrl(GetDlgItem(dialog_, IDC_FILESPEC_REGEX), 0 != (config_->flags_ & SF_FILESPEC_REGEXES));
    checkCtrl(GetDlgItem(dialog_, IDC_FILESPEC_CASE),  0 != (config_->flags_ & SF_FILESPEC_CASE_SENSITIVE));
    checkCtrl(GetDlgItem(dialog_, IDC_MATCH_REGEXES),  0 != (config_->flags_ & SF_MATCH_REGEXES));
    checkCtrl(GetDlgItem(dialog_, IDC_MATCH_CASE),     0 != (config_->flags_ & SF_MATCH_CASE_SENSITIVE));
}

void FriskWindow::controlsToConfig()
{
    comboLRU(pathCtrl_, config_->paths_, 10);
    comboLRU(matchCtrl_, config_->matches_, 10);
    comboLRU(filespecCtrl_, config_->filespecs_, 10);
    comboLRU(replaceCtrl_, config_->replaces_, 10);

    config_->flags_ = 0;
    if(ctrlIsChecked(GetDlgItem(dialog_, IDC_RECURSIVE)))
        config_->flags_ |= SF_RECURSIVE;
    if(ctrlIsChecked(GetDlgItem(dialog_, IDC_FILESPEC_REGEX)))
        config_->flags_ |= SF_FILESPEC_REGEXES;
    if(ctrlIsChecked(GetDlgItem(dialog_, IDC_FILESPEC_CASE)))
        config_->flags_ |= SF_FILESPEC_CASE_SENSITIVE;
    if(ctrlIsChecked(GetDlgItem(dialog_, IDC_MATCH_REGEXES)))
        config_->flags_ |= SF_MATCH_REGEXES;
    if(ctrlIsChecked(GetDlgItem(dialog_, IDC_MATCH_CASE)))
        config_->flags_ |= SF_MATCH_CASE_SENSITIVE;
}

void FriskWindow::windowToConfig()
{
    RECT windowRect;
    GetWindowRect(dialog_, &windowRect);
    config_->windowX_ = windowRect.left;
    config_->windowY_ = windowRect.top;
    config_->windowW_ = windowRect.right - windowRect.left;
    config_->windowH_ = windowRect.bottom - windowRect.top;
}

void FriskWindow::updateState()
{
    const char *running = running_ ? "Frisking, please wait..." : "";
    setWindowText(stateCtrl_, running);
    InvalidateRect(stateCtrl_, NULL, TRUE);
}

// ------------------------------------------------------------------------------------------------

INT_PTR FriskWindow::onInitDialog(HWND hDlg, WPARAM wParam, LPARAM lParam)
{
    dialog_       = hDlg;
    context_      = new SearchContext(dialog_);
    config_       = &(context_->config());
    outputCtrl_   = GetDlgItem(hDlg, IDC_OUTPUT);
    pathCtrl_     = GetDlgItem(hDlg, IDC_PATH);
    filespecCtrl_ = GetDlgItem(hDlg, IDC_FILESPEC);
    matchCtrl_    = GetDlgItem(hDlg, IDC_MATCH);
    stateCtrl_    = GetDlgItem(hDlg, IDC_STATE);
    replaceCtrl_  = GetDlgItem(hDlg, IDC_REPLACE);

    SendMessage(pathCtrl_,     WM_SETFONT, (WPARAM)font_, MAKEWORD(TRUE, 0));
    SendMessage(filespecCtrl_, WM_SETFONT, (WPARAM)font_, MAKEWORD(TRUE, 0));
    SendMessage(matchCtrl_,    WM_SETFONT, (WPARAM)font_, MAKEWORD(TRUE, 0));
    SendMessage(stateCtrl_,    WM_SETFONT, (WPARAM)font_, MAKEWORD(TRUE, 0));
    SendMessage(replaceCtrl_,  WM_SETFONT, (WPARAM)font_, MAKEWORD(TRUE, 0));

    SendMessage(GetDlgItem(dialog_, IDC_RECURSIVE),      WM_SETFONT, (WPARAM)font_, MAKEWORD(TRUE, 0));
    SendMessage(GetDlgItem(dialog_, IDC_FILESPEC_REGEX), WM_SETFONT, (WPARAM)font_, MAKEWORD(TRUE, 0));
    SendMessage(GetDlgItem(dialog_, IDC_FILESPEC_CASE),  WM_SETFONT, (WPARAM)font_, MAKEWORD(TRUE, 0));
    SendMessage(GetDlgItem(dialog_, IDC_MATCH_REGEXES),  WM_SETFONT, (WPARAM)font_, MAKEWORD(TRUE, 0));
    SendMessage(GetDlgItem(dialog_, IDC_MATCH_CASE),     WM_SETFONT, (WPARAM)font_, MAKEWORD(TRUE, 0));

    bool shouldMaximize = (config_->windowMaximized_ != 0);

    HDC dc = GetDC(NULL);
    outputUpdateColors();
    SendMessage(outputCtrl_, EM_SETEVENTMASK, 0, ENM_MOUSEEVENTS);
    CHARRANGE charRange;
    charRange.cpMin = -1;
    charRange.cpMax = -1;
    SendMessage(outputCtrl_, EM_EXSETSEL, 0, (LPARAM)&charRange);
    ReleaseDC(NULL, dc);

    configToControls();
    updateState();

    HICON hIcon;
    hIcon = (HICON)LoadImage(instance_,
        MAKEINTRESOURCE(IDI_FRISK),
        IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON),
        GetSystemMetrics(SM_CYSMICON),
        0);
    if(hIcon)
    {
        SendMessage(dialog_, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
    }

    RECT windowRect;
    GetWindowRect(dialog_, &windowRect);
    int width = windowRect.right - windowRect.left;
    int height = windowRect.bottom - windowRect.top;
    if(config_->windowW_)
        width = config_->windowW_;
    if(config_->windowH_)
        height = config_->windowH_;
    MoveWindow(dialog_, config_->windowX_, config_->windowY_, width, height, FALSE);
    if(shouldMaximize)
        ShowWindow(dialog_, SW_MAXIMIZE);
    return TRUE;
}

static void splitRtfHighlights(const char *rawLine, HighlightList &highlights, StringList &l)
{
	const char* c = rawLine;
	for(HighlightList::iterator it = highlights.begin(); it != highlights.end(); it++)
	{
		Highlight &highlight = *it;
		std::string pre(c, rawLine + it->offset);
		l.push_back(pre);
		std::string p(rawLine + it->offset, it->count);
		l.push_back(p);
		c = rawLine + it->offset + it->count;
	}
	if(*c)
		l.push_back(std::string(c));
}

std::string FriskWindow::rtfHighlight(const char *rawLine, HighlightList &highlights, int count)
{
	StringList pieces;
	splitRtfHighlights(rawLine, highlights, pieces);

	char header[1024];
	sprintf(header, "{\\rtf1{\\fonttbl{\\f0\\fnil\\fcharset0 Courier New;}}{\\colortbl ;\\red%d\\green%d\\blue%d;\\red%d\\green%d\\blue%d;}\\fs%d\\cf1 ",
		(int)GetRValue(config_->textColor_),
		(int)GetGValue(config_->textColor_),
		(int)GetBValue(config_->textColor_),
		(int)GetRValue(config_->highlightColor_),
		(int)GetGValue(config_->highlightColor_),
		(int)GetBValue(config_->highlightColor_),
		config_->textSize_ * 2
		);

	std::string rtf = header;
	const char * colorTexts[2] = 
	{
		"\\cf1 ",
		"\\cf2 ",
	};
	int currentColor = 0;
	for(StringList::iterator it = pieces.begin(); it != pieces.end(); it++)
	{
		replaceAll(*it, "\\", "\\\\");
		replaceAll(*it, "{", "\\{");
		replaceAll(*it, "}", "\\}");
		replaceAll(*it, "\n", "\\line ");
		rtf += colorTexts[currentColor];
		currentColor ^= 1;
		rtf += *it;
	}
	rtf += "}";
	return rtf;
}

INT_PTR FriskWindow::onPoke(WPARAM wParam, LPARAM lParam)
{
    if(wParam != context_->searchID())
        return FALSE;

	PokeData *pokeData = (PokeData *)lParam;

    // Disable redrawing briefly
    SendMessage(outputCtrl_, WM_SETREDRAW, FALSE, 0);

    // Stash off the previous caret and scroll positions
    CHARRANGE prevRange;
    SendMessage(outputCtrl_, EM_EXGETSEL, 0, (LPARAM)&prevRange);
    POINT prevScrollPos;
    SendMessage(outputCtrl_, EM_GETSCROLLPOS, 0, (LPARAM)&prevScrollPos);

    // Find out where the last character position is (for appending)
    GETTEXTLENGTHEX textLengthEx;
    textLengthEx.codepage = CP_ACP;
    textLengthEx.flags = GTL_NUMCHARS;
    int textLength = SendMessage(outputCtrl_, EM_GETTEXTLENGTHEX, (WPARAM)&textLengthEx, 0);

    // Move the caret to the end of the text
    CHARRANGE charRange;
    charRange.cpMin = textLength;
    charRange.cpMax = textLength;
    SendMessage(outputCtrl_, EM_EXSETSEL, 0, (LPARAM)&charRange);

    // Append the incoming text
	std::string rtfText = rtfHighlight(pokeData->text.c_str(), pokeData->highlights, 0);
    SendMessage(outputCtrl_, EM_REPLACESEL, FALSE, (LPARAM)rtfText.c_str());
    delete pokeData;

    // Move the caret/selection back to where it was, and scroll to the previous view
    SendMessage(outputCtrl_, EM_EXSETSEL, 0, (LPARAM)&prevRange);
    SendMessage(outputCtrl_, EM_SETSCROLLPOS, 0, (LPARAM)&prevScrollPos);

    // Reenable redrawing and invalidate the window's contents
    SendMessage(outputCtrl_, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(outputCtrl_, NULL, TRUE);

    updateState();
    return TRUE;
}

INT_PTR FriskWindow::onState(WPARAM wParam, LPARAM lParam)
{
    running_ = (wParam != 0);
    updateState();
    return TRUE;
}

INT_PTR FriskWindow::onNotify(WPARAM wParam, LPARAM lParam)
{
    NMHDR *nmhdr = (NMHDR *)lParam;
    if(nmhdr->idFrom == IDC_OUTPUT)
    {
        switch(nmhdr->code)
        {
        case EN_MSGFILTER:
            {
                MSGFILTER *filter = (MSGFILTER *)lParam;
                if(filter->msg == WM_LBUTTONDBLCLK)
                {
                    onDoubleClickOutput();
                }
            }
            break;
        };
    }

    return TRUE;
}

INT_PTR FriskWindow::onMove(WPARAM wParam, LPARAM lParam)
{
    if(!closing_)
        windowToConfig();
    return TRUE;
}

INT_PTR FriskWindow::onSize(WPARAM wParam, LPARAM lParam)
{
    outputUpdatePos();
    if(wParam == SIZE_MAXIMIZED)
    {
        config_->windowMaximized_ = 1;
        return TRUE;
    }

    if(!closing_)
        config_->windowMaximized_ = 0;

    if(wParam == SIZE_MINIMIZED)
    {
        // Not interesting
    }
    else
    {
        windowToConfig();
    }
    return TRUE;
}

INT_PTR FriskWindow::onShow(WPARAM wParam, LPARAM lParam)
{
    outputUpdatePos();
    return TRUE;
}

void FriskWindow::onCancel()
{
    closing_ = true;
    SendMessage(dialog_, WM_SYSCOMMAND, SC_RESTORE, 0);
    EndDialog(dialog_, IDCANCEL);
}

void FriskWindow::search(int extraFlags)
{
    if(!hasWindowText(matchCtrl_)
    || !hasWindowText(pathCtrl_)
    || !hasWindowText(filespecCtrl_))
    {
        MessageBox(dialog_, "Please fill out What, Where, and Which.", "Not so fast!", MB_OK);
        return;
    }

    controlsToConfig();
    config_->save();

    context_->stop();
    outputClear();

    SearchParams params;
    params.flags = config_->flags_ | extraFlags;
    params.match = config_->matches_[0];
    params.replace = config_->replaces_[0];
    split(config_->paths_[0], ";", params.paths);
    split(config_->filespecs_[0], ";", params.filespecs);
    context_->search(params);

    updateState();
}

void FriskWindow::onSearch()
{
    search(0);
}

void FriskWindow::onReplace()
{
    if(IDYES == MessageBox(dialog_, "Are you SURE you want to perform a Replace in Files?", "Confirmation", MB_YESNO))
        search(SF_REPLACE);
}

void FriskWindow::onSettings()
{
    SettingsWindow settings(instance_, dialog_, config_);
    if(settings.show())
    {
        outputUpdateColors();
        config_->save();
    }
}

void FriskWindow::onBrowse()
{
    BROWSEINFO bi = { 0 };
    bi.lpszTitle = "Pick a Directory";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_EDITBOX | BIF_NEWDIALOGSTYLE | BIF_NONEWFOLDERBUTTON;
    LPITEMIDLIST pidl = SHBrowseForFolder ( &bi );
    if ( pidl != 0 )
    {
        // get the name of the folder
        char path[MAX_PATH];
        if ( SHGetPathFromIDList ( pidl, path ) )
        {
            setWindowText(pathCtrl_, std::string(path));
        }

        // free memory used
        IMalloc * imalloc = 0;
        if ( SUCCEEDED( SHGetMalloc ( &imalloc )) )
        {
            imalloc->Free ( pidl );
            imalloc->Release ( );
        }
    }
}

void FriskWindow::onDoubleClickOutput()
{
    CHARRANGE charRange;
    SendMessage(outputCtrl_, EM_EXGETSEL, 0, (LPARAM)&charRange);

    int offset = charRange.cpMin;

    context_->lock();
    SearchList &list = context_->list();
    const SearchEntry *entry = NULL;
    for(SearchList::const_iterator it = list.begin(); it != list.end(); it++)
    {
        if(it->offset_ > offset)
        {
            entry = &(*it);
            break;
        }
    }
    if(entry)
    {
        //std::string cmd = "c:\\vim\\vim73\\gvim.exe --remote-silent +!LINE! +zz \"!FILENAME!\"";
        std::string cmd = config_->cmdTemplate_;
        char lineBuffer[32];
        sprintf(lineBuffer, "%d", entry->line_);
        replaceAll(cmd, "!LINE!", lineBuffer);
        replaceAll(cmd, "!FILENAME!", entry->filename_.c_str());

        PROCESS_INFORMATION pi;
        STARTUPINFO si;
        ZeroMemory(&si, sizeof(STARTUPINFO));
        si.cb = sizeof(STARTUPINFO);
        if(CreateProcess(NULL, (char *)cmd.c_str(), NULL, NULL, TRUE, DETACHED_PROCESS, NULL, NULL, &si, &pi))
        {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
    }
    context_->unlock();
}

// ------------------------------------------------------------------------------------------------

static INT_PTR CALLBACK FriskProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
        processMessageH(WM_INITDIALOG, onInitDialog);
        processMessage(WM_SEARCHCONTEXT_POKE, onPoke);
        processMessage(WM_SEARCHCONTEXT_STATE, onState);
        processMessage(WM_NOTIFY, onNotify);
        processMessage(WM_MOVE, onMove);
        processMessage(WM_SIZE, onSize);
        processMessage(WM_SHOWWINDOW, onShow);

        case WM_COMMAND:
            switch(LOWORD(wParam))
            {
                processCommand(IDCANCEL, onCancel);
                processCommand(IDC_SEARCH, onSearch);
                processCommand(IDC_DOREPLACE, onReplace);
                processCommand(IDC_SETTINGS, onSettings);
                processCommand(IDC_BROWSE, onBrowse);
            };
    }
    return (INT_PTR)FALSE;
}

void FriskWindow::show()
{
    DialogBox(instance_, MAKEINTRESOURCE(IDD_FRISK), NULL, FriskProc);
}
