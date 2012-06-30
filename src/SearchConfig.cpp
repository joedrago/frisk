// ---------------------------------------------------------------------------
//                   Copyright Joe Drago 2012.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "SearchConfig.h"

#include <windows.h>
#include <shlobj.h>
#include <cJSON.h>

// ------------------------------------------------------------------------------------------------
// Helper functions

static std::string calcConfigFilename()
{
    char buffer[MAX_PATH];
    std::string filename;
    if(GetModuleFileName(GetModuleHandle(NULL), buffer, MAX_PATH))
    {
        char *lastBackslash = strrchr(buffer, '\\');
        if(lastBackslash)
        {
            *lastBackslash = 0;
            filename = buffer;
            filename += "\\frisk.conf";
        }
    }
    return filename;
}

bool readEntireFile(const std::string &filename, std::string &contents, s64 maxSizeKb)
{
    FILE *f = fopen(filename.c_str(), "rb");
    if(!f)
        return false;

    fseek(f, 0, SEEK_END);
    off_t size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if(size == 0)
    {
        fclose(f);
        return false;
    }

    if(maxSizeKb && ((size / 1024) > maxSizeKb))
    {
        fclose(f);
        return false;
    }

    contents.resize(size);
    size_t bytesRead = fread(&contents[0], sizeof(char), size, f);
    if(bytesRead != size)
    {
        fclose(f);
        return false;
    }

    fclose(f);
    return true;
}

bool writeEntireFile(const std::string &filename, const std::string &contents)
{
    FILE *f = fopen(filename.c_str(), "wb");
    if(!f)
        return false;

    fwrite(contents.c_str(), sizeof(char), contents.length(), f);
    fclose(f);
    return true;
}

static bool jsonGetString(cJSON *json, const char *k, std::string &v)
{
    if(json->type != cJSON_Object)
        return false;

    cJSON *child = cJSON_GetObjectItem(json, k);
    if(!child || (child->type != cJSON_String))
        return false;
    
    v = child->valuestring;
    return true;
}

static bool jsonGetInt(cJSON *json, const char *k, int &v)
{
    if(json->type != cJSON_Object)
        return false;

    cJSON *child = cJSON_GetObjectItem(json, k);
    if(!child || (child->type != cJSON_Number))
        return false;
    
    v = child->valueint;
    return true;
}

static bool jsonSetInt(cJSON *json, const char *k, int v)
{
    if(json->type != cJSON_Object)
        return false;

    cJSON_AddNumberToObject(json, k, v);
    return true;
}

static bool jsonSetString(cJSON *json, const char *k, const std::string &v)
{
    if(json->type != cJSON_Object)
        return false;

    cJSON_AddStringToObject(json, k, v.c_str());
    return true;
}

static bool jsonGetStringList(cJSON *json, const char *k, StringList &v)
{
    if(json->type != cJSON_Object)
        return false;

    cJSON *child = cJSON_GetObjectItem(json, k);
    if(!child || (child->type != cJSON_Array))
        return false;

    child = child->child;

    v.clear();
    for( ; child != NULL; child = child->next)
    {
        if(child->type != cJSON_String)
            continue;

        v.push_back(std::string(child->valuestring));
    }
    return true;
}

static void jsonSetStringList(cJSON *json, const char *k, const StringList &v)
{
    if(v.empty())
    {
        cJSON_AddItemToObject(json, k, cJSON_CreateArray());
        return;
    }

    int count = v.size();
    const char **rawStrings = (const char **)malloc(sizeof(const char **) * count);
    for(int i = 0; i < count; i++)
    {
        rawStrings[i] = v[i].c_str();
    }
    cJSON_AddItemToObject(json, k, cJSON_CreateStringArray(rawStrings, count));
    free(rawStrings);
}

static bool jsonGetSavedSearch(cJSON *json, SavedSearch &savedSearch)
{
	if(json->type != cJSON_Object)
		return false;

	jsonGetString(json, "name", savedSearch.name);
	jsonGetString(json, "match", savedSearch.match);
	jsonGetString(json, "path", savedSearch.path);
	jsonGetString(json, "filespec", savedSearch.filespec);
	jsonGetString(json, "fileSize", savedSearch.fileSize);
	jsonGetString(json, "replace", savedSearch.replace);
	jsonGetString(json, "backupExtension", savedSearch.backupExtension);
	jsonGetInt(json, "flags", savedSearch.flags);

	return true;
}

static bool jsonSetSavedSearch(cJSON *json, SavedSearch &savedSearch)
{
	if(json->type != cJSON_Object)
		return false;

	jsonSetString(json, "name", savedSearch.name);
	jsonSetString(json, "match", savedSearch.match);
	jsonSetString(json, "path", savedSearch.path);
	jsonSetString(json, "filespec", savedSearch.filespec);
	jsonSetString(json, "fileSize", savedSearch.fileSize);
	jsonSetString(json, "replace", savedSearch.replace);
	jsonSetString(json, "backupExtension", savedSearch.backupExtension);
	jsonSetInt(json, "flags", savedSearch.flags);

	return true;
}

// ------------------------------------------------------------------------------------------------

SearchConfig::SearchConfig()
{
    // Defaults go here; may be overridden by jsonGet*()
    windowX_ = 0;
    windowY_ = 0;
    windowW_ = 0;
    windowH_ = 0;
    windowMaximized_ = 0;
    flags_ = SF_RECURSIVE | SF_BACKUP;
    textColor_ = RGB(0, 0, 0);
	textSize_ = 8;
    backgroundColor_ = RGB(224, 224, 224);
	highlightColor_ = RGB(255, 0, 0);
    cmdTemplate_ = "notepad.exe \"!FILENAME!\"";
	backupExtensions_.push_back("friskbackup");
    fileSizes_.push_back("5000");

    TCHAR tempPath[MAX_PATH];
    if(SUCCEEDED(SHGetFolderPath(NULL, CSIDL_DESKTOPDIRECTORY|CSIDL_FLAG_CREATE, NULL, 0, tempPath)))
        paths_.push_back(tempPath);
    filespecs_.push_back("*.txt;*.ini");
}

SearchConfig::~SearchConfig()
{
}

// ------------------------------------------------------------------------------------------------

void SearchConfig::load()
{
    std::string filename = calcConfigFilename();
    if(filename.empty())
        return;

    std::string contents;
    if(!readEntireFile(filename, contents, 0))
        return;

    cJSON *json = cJSON_Parse(contents.c_str());
    if(!json)
        return;

    jsonGetInt(json, "windowX", windowX_);
    jsonGetInt(json, "windowY", windowY_);
    jsonGetInt(json, "windowW", windowW_);
    jsonGetInt(json, "windowH", windowH_);
    jsonGetInt(json, "windowMaximized", windowMaximized_);
    jsonGetInt(json, "flags", flags_);
    jsonGetInt(json, "textColor", textColor_);
    jsonGetInt(json, "textSize", textSize_);
    jsonGetInt(json, "backgroundColor", backgroundColor_);
    jsonGetInt(json, "highlightColor", highlightColor_);
    jsonGetString(json, "cmdTemplate", cmdTemplate_);
    jsonGetStringList(json, "matches", matches_);
    jsonGetStringList(json, "paths", paths_);
    jsonGetStringList(json, "filespecs", filespecs_);
    jsonGetStringList(json, "replaces", replaces_);
	jsonGetStringList(json, "backupExtensions", backupExtensions_);
	jsonGetStringList(json, "fileSizes", fileSizes_);

	cJSON *savedSearches = cJSON_GetObjectItem(json, "savedSearches");
	if(savedSearches && savedSearches->type == cJSON_Array)
	{
		for(savedSearches = savedSearches->child; savedSearches; savedSearches = savedSearches->next)
		{
			SavedSearch savedSearch;
			if(jsonGetSavedSearch(savedSearches, savedSearch))
			{
				savedSearches_.push_back(savedSearch);
			}
		}
	}

    cJSON_Delete(json);
}

void SearchConfig::save()
{
    std::string filename = calcConfigFilename();
    if(filename.empty())
        return;

    cJSON *json = cJSON_CreateObject();

    jsonSetInt(json, "windowX", windowX_);
    jsonSetInt(json, "windowY", windowY_);
    jsonSetInt(json, "windowW", windowW_);
    jsonSetInt(json, "windowH", windowH_);
    jsonSetInt(json, "windowMaximized", windowMaximized_);
    jsonSetInt(json, "flags", flags_);
    jsonSetInt(json, "textColor", textColor_);
    jsonSetInt(json, "textSize", textSize_);
    jsonSetInt(json, "backgroundColor", backgroundColor_);
    jsonSetInt(json, "highlightColor", highlightColor_);
    jsonSetString(json, "cmdTemplate", cmdTemplate_);
    jsonSetStringList(json, "matches", matches_);
    jsonSetStringList(json, "paths", paths_);
    jsonSetStringList(json, "filespecs", filespecs_);
    jsonSetStringList(json, "replaces", replaces_);
	jsonSetStringList(json, "backupExtensions", backupExtensions_);
	jsonSetStringList(json, "fileSizes", fileSizes_);

	cJSON *savedSearches = cJSON_CreateArray();
	cJSON_AddItemToObject(json, "savedSearches", savedSearches);
	for(SavedSearchList::iterator it = savedSearches_.begin(); it != savedSearches_.end(); ++it)
	{
		cJSON *savedSearch = cJSON_CreateObject();
		if(jsonSetSavedSearch(savedSearch, *it))
		{
			cJSON_AddItemToArray(savedSearches, savedSearch);
		}
		else
		{
			cJSON_Delete(savedSearch);
		}
	}

    char *jsonText = cJSON_Print(json);
	writeEntireFile(filename, jsonText);

    cJSON_Delete(json);
}
