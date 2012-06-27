#ifndef SETTINGSWINDOW_H
#define SETTINGSWINDOW_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "SearchContext.h"

#define processMessageH(MSG, FUNC) case MSG: return sWindow->FUNC(hDlg, wParam, lParam)
#define processMessage(MSG, FUNC)  case MSG: return sWindow->FUNC(wParam, lParam)
#define processCommand(CMD, FUNC)  case CMD: sWindow->FUNC(); return TRUE

class SettingsWindow
{
public:
    SettingsWindow(HINSTANCE instance, HWND parent, SearchConfig *config);
    ~SettingsWindow();

    bool show();

    INT_PTR onInitDialog(HWND hDlg, WPARAM wParam, LPARAM lParam);
    void onOK();
    void onCancel();
    void onTextColor();
    void onBackgroundColor();

protected:
    HINSTANCE instance_;
    HWND dialog_;
    HWND parent_;
    SearchConfig *config_;
    int textColor_;
    int backgroundColor_;
};

#endif