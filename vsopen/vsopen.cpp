//#include "targetver.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <atlbase.h>
#include <atlwin.h>
#include <atlwin.h>

#include <string>
#include <vector>

// JDRAGO - this stolen stolen code is stolen from StackOverflow.

//-----------------------------------------------------------------------
// This code is blatently stolen from http://benbuck.com/archives/13
//
// This is from the blog of somebody called "BenBuck" for which there
// seems to be no information.
//-----------------------------------------------------------------------

// import EnvDTE
#pragma warning(disable : 4278)
#pragma warning(disable : 4146)
#import "libid:80cc9f66-e7d8-4ddd-85b6-d9e6cd0e93e2" version("8.0") lcid("0") raw_interfaces_only named_guids
#pragma warning(default : 4146)
#pragma warning(default : 4278)

bool visual_studio_open_file(char const *filename, unsigned int line)
{
    HRESULT result;
    CLSID clsid;
    result = ::CLSIDFromProgID(L"VisualStudio.DTE", &clsid);
    if (FAILED(result))
        return false;

    CComPtr<IUnknown> punk;
    result = ::GetActiveObject(clsid, NULL, &punk);
    if (FAILED(result))
        return false;

    CComPtr<EnvDTE::_DTE> DTE;
    DTE = punk;

    CComPtr<EnvDTE::ItemOperations> item_ops;
    result = DTE->get_ItemOperations(&item_ops);
    if (FAILED(result))
        return false;

    CComBSTR bstrFileName(filename);
    CComBSTR bstrKind(EnvDTE::vsViewKindTextView);
    CComPtr<EnvDTE::Window> window;
    result = item_ops->OpenFile(bstrFileName, bstrKind, &window);
    if (FAILED(result))
        return false;

    CComPtr<EnvDTE::Document> doc;
    result = DTE->get_ActiveDocument(&doc);
    if (FAILED(result))
        return false;

    CComPtr<IDispatch> selection_dispatch;
    result = doc->get_Selection(&selection_dispatch);
    if (FAILED(result))
        return false;

    CComPtr<EnvDTE::TextSelection> selection;
    result = selection_dispatch->QueryInterface(&selection);
    if (FAILED(result))
        return false;

    result = selection->GotoLine(line, FALSE);
    if (FAILED(result))
        return false;

    return true;
}

typedef std::vector<std::string> StringList;

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

static void trimQuotes(std::string &s)
{
    s.erase(0, s.find_first_not_of(' '));
    s.erase(s.find_last_not_of(' ')+1);
}

static void trimAllQuotes(StringList &l)
{
	for(StringList::iterator it = l.begin(); it != l.end(); ++it)
    {
        trimQuotes(*it);
    }
}

int APIENTRY WinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPTSTR    lpCmdLine,
                     int       nCmdShow)
{
    std::string cmdline = lpCmdLine;
    StringList args;
    split(cmdline, " ", args);
    if(args.size() != 2)
        return 1;

    CoInitialize(NULL);
    visual_studio_open_file(args[0].c_str(), atoi(args[1].c_str()));
    return 0;
}
