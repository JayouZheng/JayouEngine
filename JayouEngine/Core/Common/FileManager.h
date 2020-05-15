//
// FileManager.h
//

#pragma once

#include "TypeDef.h"

#include <fstream>
// The Common Item Dialog implements an interface named IFileOpenDialog, 
// which is declared in the header file Shobjidl.h.
#include <shobjidl.h>

// Currently as WinUtility, some of them need to be moved into Utility (Cross Platform).
namespace WinUtility
{
	namespace FileManager
	{	
		enum ESupportFileType
		{
			SF_JayouEngine,
			SF_AssimpModel,
			SF_StdImage,
			SF_Unknown
		};

		const std::unordered_map<ESupportFileType, std::wstring> SupportFileTypeTable = 
		{
			{ SF_JayouEngine, L"*.jayou;*.jscene" },
			{ SF_AssimpModel, L"*.fbx;*.obj;*.3D;*.3DS;*.3MF;*.AC;*.AC3D;*.ACC;*.AMJ;*.ASE;*.ASK;*.B3D;*.BLEND \
				*.BVH;*.CMS;*.COB;*.DAE;*.DXF;*.ENFF;*.glTF;*.glTF;*.HMB;*.IFC-STEP;*.IRR;*.LWO;*.LWS;*.LXO    \
				*.M3D;*.MD2;*.MD3;*.MD5;*.MDC;*.MDL;*.MESH;*.MOT;*.MS3D;*.NDO;*.NFF;*.OFF;*.OGEX;*.PLY;*.PMX   \
				*.PRJ;*.Q3O;*.Q3S;*.RAW;*.SCN;*.SIB;*.SMD;*.STP;*.STL;*.TER;*.UC;*.VTA;*.X;*.X3D;*.XGL;*.ZGL" },
			{ SF_StdImage,    L"*.png;*.jpg;*.jpeg;*.tga;*.bmp;*.psd;*.gif;*.hdr;*.pic;*.pnm;*.ppm;*.pgm" },
		};

		const std::vector<COMDLG_FILTERSPEC> DefaultFilter =
		{
			{ L"JayouEngine", L"*.jayou;*.jscene" },
			{ L"Model1", L"*.fbx;*.obj;*.3D;*.3DS;*.3MF;*.AC;*.AC3D;*.ACC;*.AMJ;*.ASE;*.ASK;*.B3D;*.BLEND" },
			{ L"Model2", L"*.BVH;*.CMS;*.COB;*.DAE;*.DXF;*.ENFF;*.glTF;*.glTF;*.HMB;*.IFC-STEP;*.IRR;*.LWO;*.LWS;*.LXO" },
			{ L"Model3", L"*.M3D;*.MD2;*.MD3;*.MD5;*.MDC;*.MDL;*.MESH;*.MOT;*.MS3D;*.NDO;*.NFF;*.OFF;*.OGEX;*.PLY;*.PMX" },
			{ L"Model4", L"*.PRJ;*.Q3O;*.Q3S;*.RAW;*.SCN;*.SIB;*.SMD;*.STP;*.STL;*.TER;*.UC;*.VTA;*.X;*.X3D;*.XGL;*.ZGL" },
			{ L"Image", L"*.png;*.jpg;*.jpeg;*.tga;*.bmp;*.psd;*.gif;*.hdr;*.pic;*.pnm;*.ppm;*.pgm" },
			{ L"All", L"*.*" }
		};

		class FileUtil
		{
		public:
	
			// OpenDialog.
			static bool OpenDialogBox(HWND InOwner, std::vector<std::wstring>& OutPaths, DWORD InOptions, const std::vector<COMDLG_FILTERSPEC>& InFilters = DefaultFilter);

			static void GetAllFilesUnder(std::string path, std::vector<std::string>& files, std::string format = "");		

			static void WGetAllFilesUnder(std::wstring path, std::vector<std::wstring>& files, std::wstring format = L"");			
			
			static void GetFileTypeFromPathW(const std::wstring& InPath, ESupportFileType& OutType);

			static void GetFileTypeFromExtW(const std::wstring& InExten, ESupportFileType& OutType);

			static void GetPathMapFileTypeFromPathW(const std::vector<std::wstring>& InPaths, std::unordered_map<std::wstring, ESupportFileType>& OutPathMapType);
			
		};

		template<typename TCHAR = char>
		class FileStream;

		using file_stream = FileStream<char>;
		using wfile_stream = FileStream<wchar_t>;

		///////////////////////////////////////
		/* Comments ///////////////////////////

		std::string str0 = Core::to_string(path);
		std::string str1 = Core::AppUtil::WStringToString(path);
		std::wstring str2 = Core::AppUtil::StringToWString(str1);
		std::wstring str3 = Core::to_wstring(str0);

		std::locale::global(std::locale(""));
		std::wofstream out(L"001.csv");
		out << str3 << std::endl;
		out.close();
		std::ofstream out2(L"002.csv", std::ofstream::binary);
		char utf_8_BOM[3] = { int8(0xef),int8(0xbb),int8(0xbf) };
		out2.write(utf_8_BOM, 3);
		out2.close();
		out2.open(L"002.csv", std::ofstream::app);
		out2 << str0 << std::endl;
		out2.close();

		*/ ////////////////////////////////////
		///////////////////////////////////////

		template<typename TCHAR>
		class FileStream
		{
		public:

			using _file_stream = std::basic_fstream<TCHAR, std::char_traits<TCHAR>>;
			using _string = std::basic_string<TCHAR, std::char_traits<TCHAR>, std::allocator<TCHAR>>;

			FileStream() {}
			~FileStream() { _fstream.close(); }

			FileStream(_string path, std::ios_base::openmode mode = _file_stream::in || _file_stream::out)
			{
				std::locale::global(std::locale(""));
				_fstream.open(path, mode);

				if (_fstream.fail())
				{
				}
			}

		private:

			_file_stream _fstream;
		};
	}
}
