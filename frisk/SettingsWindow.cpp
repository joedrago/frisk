// ---------------------------------------------------------------------------
//                   Copyright Joe Drago 2012.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "SettingsWindow.h"

#include <Commdlg.h>
#include "resource.h"

static SettingsWindow *sWindow = NULL;

SettingsWindow::SettingsWindow(HINSTANCE instance, HWND parent, SearchConfig *config)
: instance_(instance)
, parent_(parent)
, config_(config)
{
    sWindow = this;

    textColor_ = config_->textColor_;
	contextColor_ = config_->contextColor_;
    backgroundColor_ = config_->backgroundColor_;
	highlightColor_ = config_->highlightColor_;
}

SettingsWindow::~SettingsWindow()
{
    sWindow = NULL;
}

INT_PTR SettingsWindow::onInitDialog(HWND hDlg, WPARAM wParam, LPARAM lParam)
{
    dialog_ = hDlg;
    setWindowText(GetDlgItem(dialog_, IDC_CMD), config_->cmdTemplate_.c_str());

    checkCtrl(GetDlgItem(dialog_, IDC_TRIM_FILENAMES), 0 != (config_->flags_ & SF_TRIM_FILENAMES));

    updateFontDescription();
    return TRUE;
}

void SettingsWindow::onOK()
{
    config_->textColor_ = textColor_;
	config_->contextColor_ = contextColor_;
    config_->backgroundColor_ = backgroundColor_;
	config_->highlightColor_ = highlightColor_;
    config_->cmdTemplate_ = getWindowText(GetDlgItem(dialog_, IDC_CMD));
    if(ctrlIsChecked(GetDlgItem(dialog_, IDC_TRIM_FILENAMES)))
		config_->flags_ |= SF_TRIM_FILENAMES;
	else
		config_->flags_ &= ~SF_TRIM_FILENAMES;

    EndDialog(dialog_, IDOK);
}

void SettingsWindow::onCancel()
{
    EndDialog(dialog_, IDCANCEL);
}

void SettingsWindow::onTextColor()
{
    COLORREF g_rgbCustom[16] = {0};
    CHOOSECOLOR cc = {sizeof(CHOOSECOLOR)};
    cc.Flags = CC_RGBINIT | CC_FULLOPEN | CC_ANYCOLOR;
    cc.hwndOwner = dialog_;
    cc.rgbResult = textColor_;
    cc.lpCustColors = g_rgbCustom;
    if(ChooseColor(&cc))
    {
        textColor_ = cc.rgbResult;
    }
}

void SettingsWindow::onContextColor()
{
    COLORREF g_rgbCustom[16] = {0};
    CHOOSECOLOR cc = {sizeof(CHOOSECOLOR)};
    cc.Flags = CC_RGBINIT | CC_FULLOPEN | CC_ANYCOLOR;
    cc.hwndOwner = dialog_;
    cc.rgbResult = contextColor_;
    cc.lpCustColors = g_rgbCustom;
    if(ChooseColor(&cc))
    {
        contextColor_ = cc.rgbResult;
    }
}

void SettingsWindow::onBackgroundColor()
{
    COLORREF g_rgbCustom[16] = {0};
    CHOOSECOLOR cc = {sizeof(CHOOSECOLOR)};
    cc.Flags = CC_RGBINIT | CC_FULLOPEN | CC_ANYCOLOR;
    cc.hwndOwner = dialog_;
    cc.rgbResult = backgroundColor_;
    cc.lpCustColors = g_rgbCustom;
    if(ChooseColor(&cc))
    {
        backgroundColor_ = cc.rgbResult;
    }
}

void SettingsWindow::onHighlightColor()
{
    COLORREF g_rgbCustom[16] = {0};
    CHOOSECOLOR cc = {sizeof(CHOOSECOLOR)};
    cc.Flags = CC_RGBINIT | CC_FULLOPEN | CC_ANYCOLOR;
    cc.hwndOwner = dialog_;
    cc.rgbResult = highlightColor_;
    cc.lpCustColors = g_rgbCustom;
    if(ChooseColor(&cc))
    {
        highlightColor_ = cc.rgbResult;
    }
}

void SettingsWindow::onFont()
{
    CHOOSEFONT cf;            // common dialog box structure
    static LOGFONT lf;        // logical font structure

    HDC hDC = GetDC(NULL);

    ZeroMemory(&lf, sizeof(lf));
    strcpy(lf.lfFaceName, config_->fontFamily_.c_str());
    lf.lfHeight = -MulDiv(config_->textSize_, GetDeviceCaps(hDC, LOGPIXELSY), 72);

    ZeroMemory(&cf, sizeof(cf));
    cf.lStructSize = sizeof(cf);
    cf.hwndOwner = dialog_;
    cf.lpLogFont = &lf;
    cf.Flags = CF_SCREENFONTS | CF_INITTOLOGFONTSTRUCT;

    if (ChooseFont(&cf)==TRUE)
    {
        config_->fontFamily_ = lf.lfFaceName;
        config_->textSize_ = -MulDiv(lf.lfHeight, 72, GetDeviceCaps(hDC, LOGPIXELSY));
        updateFontDescription();
    }

    ReleaseDC(NULL, hDC);
}

void SettingsWindow::onCmdNotepad()
{
    setWindowText(GetDlgItem(dialog_, IDC_CMD), "notepad.exe \"!FILENAME!\"");
}

void SettingsWindow::onCmdAssoc()
{
    setWindowText(GetDlgItem(dialog_, IDC_CMD), "cmd.exe /c start \"Frisk\" \"!FILENAME!\"");
}

static INT_PTR CALLBACK SettingsProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
        processMessageH(WM_INITDIALOG, onInitDialog);

        case WM_COMMAND:
            switch(LOWORD(wParam))
            {
                processCommand(IDOK, onOK);
                processCommand(IDCANCEL, onCancel);
                processCommand(IDC_COLOR_TEXT, onTextColor);
                processCommand(IDC_COLOR_CONTEXT, onContextColor);
                processCommand(IDC_COLOR_BG, onBackgroundColor);
                processCommand(IDC_COLOR_HIGHLIGHT, onHighlightColor);
                processCommand(IDC_FONT, onFont);
                processCommand(IDC_CMD_NOTEPAD, onCmdNotepad);
                processCommand(IDC_CMD_ASSOC, onCmdAssoc);
            };
    }
    return (INT_PTR)FALSE;
}

bool SettingsWindow::show()
{
    return (IDOK == DialogBox(instance_, MAKEINTRESOURCE(IDD_SETTINGS), parent_, SettingsProc));
}

void SettingsWindow::updateFontDescription()
{
    char fontDescription[1024];
    _snprintf(fontDescription, sizeof(fontDescription), "%s, %d pt", config_->fontFamily_.c_str(), config_->textSize_);
    fontDescription[ sizeof(fontDescription) - 1 ] = 0;
    setWindowText(GetDlgItem(dialog_, IDC_FONT_DESC), fontDescription);
}