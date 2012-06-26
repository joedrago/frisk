#include "FriskWindow.h"
#include "resource.h"

static FriskWindow *sWindow = NULL;

FriskWindow::FriskWindow(HINSTANCE instance)
: instance_(instance)
, dialog_((HWND)INVALID_HANDLE_VALUE)
, richEdit_((HWND)INVALID_HANDLE_VALUE)
, context_(NULL)
{
    sWindow = this;
}

FriskWindow::~FriskWindow()
{
    delete context_;
    sWindow = NULL;
}

// ------------------------------------------------------------------------------------------------

void FriskWindow::updateOutputPos()
{
    RECT clientRect;
    RECT richEditRect;

    GetClientRect(dialog_, &clientRect);
    GetWindowRect(richEdit_, &richEditRect);
    POINT richEditPos;
    richEditPos.x = richEditRect.left;
    richEditPos.y = richEditRect.top;
    ScreenToClient(dialog_, &richEditPos);

    richEditRect.right = 10;
    MoveWindow(richEdit_, richEditPos.x, richEditPos.y, clientRect.right - (richEditPos.x * 2), clientRect.bottom - richEditPos.y - 10, TRUE);
}

// ------------------------------------------------------------------------------------------------

INT_PTR FriskWindow::onInitDialog(HWND hDlg, WPARAM wParam, LPARAM lParam)
{
    dialog_ = hDlg;
    context_ = new SearchContext(dialog_);
    richEdit_ = GetDlgItem(hDlg, IDC_OUTPUT);

    HDC dc = GetDC(NULL);
    CHARFORMAT format = {0};
    format.cbSize = sizeof(CHARFORMAT);
    format.dwMask = CFM_FACE | CFM_SIZE | CFM_COLOR;
    format.yHeight = 160;
    format.crTextColor = RGB(255, 255, 255);
    strcpy(format.szFaceName, "Courier New");
    SendMessage(richEdit_, EM_SETBKGNDCOLOR, 0, (LPARAM)RGB(0,0,0));
    SendMessage(richEdit_, EM_SETCHARFORMAT, SCF_DEFAULT, (LPARAM)&format);
    SendMessage(richEdit_, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&format);
    SendMessage(richEdit_, EM_SETEVENTMASK, 0, ENM_MOUSEEVENTS);
    SendMessage(richEdit_, EM_SETSEL, -1, -1);
    ReleaseDC(NULL, dc);

    return TRUE;
}

INT_PTR FriskWindow::onPoke(WPARAM wParam, LPARAM lParam)
{
    SendMessage(richEdit_, EM_SETSEL, -1, -1);
    SendMessage(richEdit_, EM_REPLACESEL, FALSE, (LPARAM)lParam);
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

INT_PTR FriskWindow::onSize(WPARAM wParam, LPARAM lParam)
{
    updateOutputPos();
    return TRUE;
}

INT_PTR FriskWindow::onShow(WPARAM wParam, LPARAM lParam)
{
    updateOutputPos();
    return TRUE;
}

void FriskWindow::onCancel()
{
    EndDialog(dialog_, IDCANCEL);
}

void FriskWindow::onSearch()
{
    SearchParams params;
    params.match = "derp";
    params.paths.push_back("c:\\work\\custom");
    params.filespecs.push_back("txt");
    context_->search(params);
}

void FriskWindow::onDoubleClickOutput()
{
    CHARRANGE charRange;
    SendMessage(richEdit_, EM_EXGETSEL, 0, (LPARAM)&charRange);

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
        MessageBox(dialog_, entry->filename_.c_str(), "you clicked on", MB_OK);
    }
    context_->unlock();
}

// ------------------------------------------------------------------------------------------------

#define processMessageH(MSG, FUNC) case MSG: return sWindow->FUNC(hDlg, wParam, lParam)
#define processMessage(MSG, FUNC)  case MSG: return sWindow->FUNC(wParam, lParam)
#define processCommand(CMD, FUNC)  case CMD: sWindow->FUNC(); return TRUE

static INT_PTR CALLBACK FriskProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
        processMessageH(WM_INITDIALOG, onInitDialog);
        processMessage(WM_SEARCHCONTEXT_POKE, onPoke);
        processMessage(WM_NOTIFY, onNotify);
        processMessage(WM_SIZE, onSize);
        processMessage(WM_SHOWWINDOW, onShow);

	    case WM_COMMAND:
            switch(LOWORD(wParam))
            {
                processCommand(IDCANCEL, onCancel);
                processCommand(IDC_SEARCH, onSearch);
            };
	}
	return (INT_PTR)FALSE;
}

void FriskWindow::show()
{
    DialogBox(instance_, MAKEINTRESOURCE(IDD_FRISK), NULL, FriskProc);
}
