#ifndef FRISKWINDOW_H
#define FRISKWINDOW_H

#define _CRT_SECURE_NO_WARNINGS
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

    void configToControls();
    void controlsToConfig();
    void windowToConfig();

    INT_PTR onInitDialog(HWND hDlg, WPARAM wParam, LPARAM lParam);
    INT_PTR onPoke(WPARAM wParam, LPARAM lParam);
    INT_PTR onNotify(WPARAM wParam, LPARAM lParam);
    INT_PTR onMove(WPARAM wParam, LPARAM lParam);
    INT_PTR onSize(WPARAM wParam, LPARAM lParam);
    INT_PTR onShow(WPARAM wParam, LPARAM lParam);

    void onCancel();
    void onSearch();
    void onDoubleClickOutput();
protected:
    HINSTANCE instance_;
    HWND dialog_;
    HWND outputCtrl_;
    HWND pathCtrl_;
    HWND filespecCtrl_;
    HWND matchCtrl_;
    SearchContext *context_;
    SearchConfig *config_;
};

#endif