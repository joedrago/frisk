// ---------------------------------------------------------------------------
//                   Copyright Joe Drago 2012.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#ifndef FRISKWINDOW_H
#define FRISKWINDOW_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <RichEdit.h>

#include "SearchContext.h"

class FriskWindow
{
public:
    FriskWindow(HINSTANCE instance);
    ~FriskWindow();

    void show();

    void outputClear();
    void outputUpdatePos();
    void outputUpdateColors();

    void configToControls();
    void controlsToConfig();
    void windowToConfig();
    void updateState(const std::string &progress = "");
	std::string rtfHighlight(const char *rawLine, HighlightList &highlights, int count);

    INT_PTR onInitDialog(HWND hDlg, WPARAM wParam, LPARAM lParam);
    INT_PTR onPoke(WPARAM wParam, LPARAM lParam);
    INT_PTR onState(WPARAM wParam, LPARAM lParam);
    INT_PTR onNotify(WPARAM wParam, LPARAM lParam);
    INT_PTR onMove(WPARAM wParam, LPARAM lParam);
    INT_PTR onSize(WPARAM wParam, LPARAM lParam);
    INT_PTR onShow(WPARAM wParam, LPARAM lParam);

    void search(int extraFlags);

    void onCancel();
    void onSearch();
    void onReplace();
    void onDoubleClickOutput();
    void onSettings();
    void onBrowse();
protected:
    HINSTANCE instance_;
    HFONT font_;
    HWND dialog_;
    HWND outputCtrl_;
    HWND pathCtrl_;
    HWND filespecCtrl_;
    HWND matchCtrl_;
    HWND stateCtrl_;
    HWND replaceCtrl_;
    SearchContext *context_;
    SearchConfig *config_;
    bool running_;
    bool closing_;
};

#endif
