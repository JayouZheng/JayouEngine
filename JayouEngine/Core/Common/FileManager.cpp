//
// FileManager.cpp
//

#include "FileManager.h"
#include <io.h>

using namespace WinUtility::FileManager;

bool FileUtil::OpenDialogBox(HWND owner, std::wstring& wfile_path, DWORD options)
{
	wfile_path = L"404 Not Found.";
	HRESULT hr;
	bool result = true;

	////////////////////////////////
	/* Comments ////////////////////
	
	hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED |
		COINIT_DISABLE_OLE1DDE);
	ThrowIfFailedV2(hr);
	if (SUCCEEDED(hr))
	{
		CoUninitialize();
	}

	*/ /////////////////////////////
	////////////////////////////////	

	IFileOpenDialog *pFileOpen;

	// Create the FileOpenDialog object.
	hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL,
		IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));

	if (SUCCEEDED(hr))
	{
		// Set Options.
		hr = pFileOpen->SetOptions(options);
		if (SUCCEEDED(hr))
		{
			// Show the Open dialog box.
			hr = pFileOpen->Show(owner);

			// Get the file name from the dialog box.
			if (SUCCEEDED(hr))
			{
				IShellItem *pItem;
				hr = pFileOpen->GetResult(&pItem);
				if (SUCCEEDED(hr))
				{
					PWSTR pszFilePath;
					hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);

					// Display the file name to the user.
					if (SUCCEEDED(hr))
					{
						wfile_path = std::wstring(pszFilePath);
						// MessageBox(NULL, pszFilePath, L"File Path", MB_OK);
						CoTaskMemFree(pszFilePath);
					}
					else result = false;
					pItem->Release();
				}
				else result = false;
			}
			else result = false;
			pFileOpen->Release();
		}
		else result = false;		
	}
	else result = false;

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
