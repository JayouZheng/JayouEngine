//
// Utility.h
//

#pragma once

#include "TypeDef.h"

namespace Utility
{
	uint32 CalcConstantBufferByteSize(uint32 byteSize);

	struct CD3DX12_INPUT_LAYOUT_DESC : public D3D12_INPUT_LAYOUT_DESC
	{
		CD3DX12_INPUT_LAYOUT_DESC() = default;
		explicit CD3DX12_INPUT_LAYOUT_DESC(const D3D12_INPUT_LAYOUT_DESC& o) :
			D3D12_INPUT_LAYOUT_DESC(o) {}

		CD3DX12_INPUT_LAYOUT_DESC(const std::vector<D3D12_INPUT_ELEMENT_DESC>& InInputLayout) 
		{
			this->pInputElementDescs = InInputLayout.data();
			this->NumElements = (uint32)InInputLayout.size();
		}
		CD3DX12_INPUT_LAYOUT_DESC(D3D12_INPUT_ELEMENT_DESC* InElemDescs, uint32 InNumElems) 
		{
			this->pInputElementDescs = InElemDescs;
			this->NumElements = InNumElems;
		}
	};
}

namespace WinUtility
{
	ComPtr<ID3DBlob> LoadBinary(const std::wstring& filename);

	ComPtr<ID3DBlob> CompileShader(
		const std::wstring& filename,
		const D3D_SHADER_MACRO* defines,
		const std::string& entrypoint,
		const std::string& target);

	void ErrorExit(LPTSTR lpszFunction);

#pragma region ThrowIfFailed

	// Helper class for COM exceptions.
	class DxException
	{
	public:
		DxException() = default;
		DxException(HRESULT hr, 
			const std::wstring& functionName, 
			const std::wstring& filename, 
			int lineNumber) :
			ErrorCode(hr),
			FunctionName(functionName),
			PathName(filename),
			LineNumber(lineNumber)
		{}

		std::wstring ToString() const
		{
			// Get the string description of the error code.
			_com_error err(ErrorCode);
			std::wstring msg = err.ErrorMessage();

			return FunctionName + L" failed in " + PathName + L"; line " + std::to_wstring(LineNumber) + L"; error: " + msg;
		}

		HRESULT ErrorCode = S_OK;
		std::wstring FunctionName;
		std::wstring PathName;
		int LineNumber = -1;
	};
	
	class com_exception : public std::exception
	{
	public:
		com_exception(HRESULT hr) : result(hr) {}

		virtual const char* what() const override
		{
			char s_str[64] = {};
			sprintf_s(s_str, "Failure with HRESULT of %08X", static_cast<unsigned int>(result));
			return s_str;
		}

	private:
		HRESULT result;
	};

	inline std::wstring _ansi_to_wstring(const std::string& str)
	{
		int bufferlen = MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, NULL, 0);
		wchar_t* buffer = new wchar_t[bufferlen];
		MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, buffer, bufferlen);
		std::wstring wstr(buffer);
		delete[] buffer;
		return wstr;
	}

// Helper utility converts D3D API failures into exceptions.
#ifndef ThrowIfFailedV1
#define ThrowIfFailedV1(x)                                                          \
{                                                                                   \
    HRESULT hr__ = (x);                                                             \
    std::wstring wfn = _ansi_to_wstring(__FILE__);                                       \
    if(FAILED(hr__)) { throw DxException(hr__, L#x, wfn, __LINE__); }               \
}
#endif

	inline void ThrowIfFailedV2(HRESULT hr)
	{
		if (FAILED(hr))
		{
			throw com_exception(hr);
		}
	}

#pragma endregion

#ifndef ReleaseCom
#define ReleaseCom(x) { if(x){ x->Release(); x = 0; } }
#endif

}