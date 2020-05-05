//
// StringManager.h
// NOTE: std::numeric_limits<T>::max() Conflict with windows.h #undef max...

#pragma once

#include <sstream>
#include <codecvt>
#include "TypeDef.h"

namespace Utility
{
	namespace StringManager
	{
		class StringUtil
		{
		public:

			static std::wstring StringToWString(const std::string& str);

			static std::wstring AnsiToWString(const std::string& str);
			
			// Data converted from UTF-16 to non-Unicode encodings is subject to data loss,
			// because a code page might not be able to represent every character used in the specific Unicode data.
			static std::string WStringToString(const std::wstring& wstr);

			// wchar_t as a numeric digit, or -1 if it is not a digit.
			static int32 WCharToInt32(wchar_t wch);

			template<typename T>
			static T WStringToNumeric(const std::wstring& wstr);

			static std::vector<std::string> GetBetween(const std::string& str, const std::string& boundary);

			static std::vector<std::wstring> WGetBetween(const std::wstring& wstr, const std::wstring& boundary);

			static std::vector<std::wstring> WGetBetween(const std::wstring& wstr, const std::wstring& bound1, const std::wstring& bound2);

			static std::vector<std::string> RemoveBetween(std::string& str, const std::string& boundary);

			static std::vector<std::wstring> WRemoveBetween(std::wstring& wstr, const std::wstring& boundary);

			static std::vector<std::string> ReplaceBetween(std::string& str, const std::string& boundary, const std::string& str_replace);

			static std::vector<std::wstring> WReplaceBetween(std::wstring& wstr, const std::wstring& boundary, const std::wstring& wstr_replace);

			static void EraseAll(std::string& str, const std::string& str_erase);

			static void WEraseAll(std::wstring& wstr, const std::wstring& wstr_erase);

			// API for Special Purpose.
			static std::wstring WFindFirstBetween(const std::wstring& wstr, const std::wstring& boundary, std::wstring::size_type& find_end, std::wstring::size_type offet = 0);

			template<typename T>
			static std::vector<T> WStringToArray(const std::wstring& wstr, const wchar_t& separator);

		};

		template<typename T>
		T StringUtil::WStringToNumeric(const std::wstring& wstr)
		{
			std::wistringstream wstr_stream(wstr);
			T temp;
			wstr_stream >> temp; // uint8/bool Not Supported.
			if (wstr_stream.fail())
			{
				temp = std::numeric_limits<T>::max();
			}
			return temp;
		}

		template<typename T>
		static std::vector<T>
			StringUtil::WStringToArray(const std::wstring& wstr, const wchar_t& separator)
		{
			std::wistringstream wstr_stream(wstr);
			std::vector<T> temp_array;
			std::wstring temp_wstr;
			T temp;
			while (true)
			{
				std::getline(wstr_stream, temp_wstr, separator);
				if (wstr_stream.fail())
					break;
				temp = WStringToNumeric<T>(temp_wstr);
				if (temp == std::numeric_limits<T>::max())
					continue;		
				temp_array.push_back(temp);
			}
			return temp_array;
		}

		wchar_t const* const WCharDigitTables[] =
		{
			L"0123456789",
			L"\u0660\u0661\u0662\u0663\u0664\u0665\u0666\u0667\u0668\u0669",
			// ...
		};

		using convert_t = std::codecvt_utf8<wchar_t>;
		using wsconvert_t = std::wstring_convert<convert_t, wchar_t>;

		inline std::string to_string(std::wstring wstr)
		{
			wsconvert_t strconverter;
			return strconverter.to_bytes(wstr);
		}

		inline std::wstring to_wstring(std::string str)
		{
			wsconvert_t strconverter;
			return strconverter.from_bytes(str);
		}
	}
}
