//
// FileManager.cpp
//

#include "Utility.h"
#include "FileManager.h"
#include "StringManager.h"

#include <io.h>

#define success_if(x) if (SUCCEEDED(x))

using namespace WinUtility;
using namespace WinUtility::FileManager;
using namespace Utility::StringManager;

bool FileUtil::OpenDialogBox(HWND InOwner, std::vector<std::wstring>& OutPaths, DWORD InOptions, const std::vector<COMDLG_FILTERSPEC>& InFilters)
{
	////////////////////////////////
	/* Comments ////////////////////

	Com Init version_1.
	RoInitializeWrapper initialize(RO_INIT_SINGLETHREADED);

	Com Init version_2.
	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	CoUninitialize();

	*/ /////////////////////////////
	////////////////////////////////

	ComPtr<IFileOpenDialog> fileOpenDialog = nullptr;
	ComPtr<IShellItemArray> shellItemArray = nullptr;
	PWSTR                   pszFilePath;
	bool                    result = false;

	success_if(CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL,
		IID_PPV_ARGS(&fileOpenDialog)))
	{
		success_if(fileOpenDialog->SetOptions(InOptions))
		{
			if (!(InOptions & FOS_PICKFOLDERS) && !InFilters.empty())
			{
				ThrowIfFailedV1(fileOpenDialog->SetFileTypes((UINT)InFilters.size(), InFilters.data()));
			}

			success_if(fileOpenDialog->Show(InOwner))
			{
				success_if(fileOpenDialog->GetResults(&shellItemArray))
				{
					DWORD numSelected;
					DWORD index;
					ThrowIfFailedV1(shellItemArray->GetCount(&numSelected));
					for (index = 0; index < numSelected; ++index)
					{
						ComPtr<IShellItem> shellItem;
						ThrowIfFailedV1(shellItemArray->GetItemAt(index, &shellItem));

						success_if(shellItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath))
						{
							OutPaths.push_back(std::wstring(pszFilePath));
							CoTaskMemFree(pszFilePath);
						}
					}
					if (index == numSelected)
					{
						result = true;
					}
				}
			}
		}
	}
	return result;
}

void FileUtil::GetAllFilesUnder(std::string path, std::vector<std::string>& files, std::string format /*= ""*/)
{
	intptr_t file = 0;
	_finddata_t file_info;
	std::string internal_path;
	if ((file = _findfirst(internal_path.assign(path).append("\\*" + format).c_str(), &file_info)) != -1)
	{
		do
		{
			if ((file_info.attrib & _A_SUBDIR))
			{
				if (strcmp(file_info.name, ".") != 0 && strcmp(file_info.name, "..") != 0)
				{
					// include dir name and dir prefix -> 
					// files.push_back(internal_path.assign(path).append("\\").append(file_info.name));
					GetAllFilesUnder(internal_path.assign(path).append("\\").append(file_info.name), files, format);
				}
			}
			else
			{
				files.push_back(file_info.name);
			}
		} while (_findnext(file, &file_info) == 0);
		_findclose(file);
	}
}

void FileUtil::WGetAllFilesUnder(std::wstring path, std::vector<std::wstring>& files, std::wstring format /*= L""*/)
{
	intptr_t file = 0;
	_wfinddata_t file_info;
	std::wstring internal_path;
	if ((file = _wfindfirst(internal_path.assign(path).append(L"\\*" + format).c_str(), &file_info)) != -1)
	{
		do
		{
			if ((file_info.attrib & _A_SUBDIR))
			{
				if (wcscmp(file_info.name, L".") != 0 && wcscmp(file_info.name, L"..") != 0)
				{
					// include dir name and dir prefix -> 
					// files.push_back(internal_path.assign(path).append(L"\\").append(file_info.name));
					WGetAllFilesUnder(internal_path.assign(path).append(L"\\").append(file_info.name), files, format);
				}
			}
			else
			{
				files.push_back(file_info.name);
			}
		} while (_wfindnext(file, &file_info) == 0);
		_findclose(file);
	}
}

void FileUtil::GetFileTypeFromPathW(const std::wstring& InPath, ESupportFileType& OutType)
{
	std::wstring name, exten;
	StringUtil::SplitFileNameAndExtFromPathW(InPath, name, exten);
	GetFileTypeFromExtW(exten, OutType);
}

void FileUtil::GetFileTypeFromExtW(const std::wstring& InExten, ESupportFileType& OutType)
{
	OutType = SF_Unknown;
	for (auto row : SupportFileTypeTable)
	{
		ESupportFileType fileType   = row.first;
		std::wstring     fileExtens = row.second;
		std::wstring     findExten  = L"*." + InExten;
		std::wstring::size_type found = fileExtens.find(findExten);
		if (found != std::wstring::npos)
		{
			OutType = fileType;
			return;
		}

		// upper.
		std::transform(findExten.begin(), findExten.end(), findExten.begin(), ::toupper);
		found = fileExtens.find(findExten);
		if (found != std::wstring::npos)
		{
			OutType = fileType;
			return;
		}

		// lower.
		std::transform(findExten.begin(), findExten.end(), findExten.begin(), ::tolower);
		found = fileExtens.find(findExten);
		if (found != std::wstring::npos)
		{
			OutType = fileType;
			return;
		}
	}	
}

void FileUtil::GetPathMapFileTypeFromPathW(const std::vector<std::wstring>& InPaths, std::unordered_map<std::wstring, ESupportFileType>& OutPathMapType)
{
	for (auto path : InPaths)
	{
		ESupportFileType fileType;
		GetFileTypeFromPathW(path, fileType);
		OutPathMapType[path] = fileType;
	}
}
