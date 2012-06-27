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
    backgroundColor_ = config_->backgroundColor_;
}

SettingsWindow::~SettingsWindow()
{
    sWindow = NULL;
}

INT_PTR SettingsWindow::onInitDialog(HWND hDlg, WPARAM wParam, LPARAM lParam)
{
    dialog_ = hDlg;
    return TRUE;
}


void SettingsWindow::onOK()
{
    config_->textColor_ = textColor_;
    config_->backgroundColor_ = backgroundColor_;

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
                processCommand(IDC_COLOR_BG, onBackgroundColor);
            };
	}
	return (INT_PTR)FALSE;
}

bool SettingsWindow::show()
{
    return (IDOK == DialogBox(instance_, MAKEINTRESOURCE(IDD_SETTINGS), parent_, SettingsProc));
}