//
// Utility.h
//

#pragma once

#include "../Math/Math.h"

#include "TypeDef.h"
#include "StringManager.h"

using Microsoft::WRL::ComPtr;

using namespace DirectX;
using namespace Math;

namespace Utility
{
	uint32 CalcConstantBufferByteSize(uint32 byteSize);
}

namespace WinUtility
{	
	ComPtr<ID3DBlob> LoadBinary(const std::wstring& filename);

	ComPtr<ID3D12Resource> CreateDefaultBuffer(
		ID3D12Device* device,
		ID3D12GraphicsCommandList* cmdList,
		const void* initData,
		UINT64 byteSize,
		ComPtr<ID3D12Resource>& uploadBuffer);

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
			Filename(filename),
			LineNumber(lineNumber)
		{}

		std::wstring ToString() const
		{
			// Get the string description of the error code.
			_com_error err(ErrorCode);
			std::wstring msg = err.ErrorMessage();

			return FunctionName + L" failed in " + Filename + L"; line " + std::to_wstring(LineNumber) + L"; error: " + msg;
		}

		HRESULT ErrorCode = S_OK;
		std::wstring FunctionName;
		std::wstring Filename;
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

// Helper utility converts D3D API failures into exceptions.
#ifndef ThrowIfFailedV1
#define ThrowIfFailedV1(x)                                                          \
{                                                                                   \
    HRESULT hr__ = (x);                                                             \
    std::wstring wfn = Utility::StringManager::StringUtil::AnsiToWString(__FILE__); \
    if(FAILED(hr__)) { throw DxException(hr__, L#x, wfn, __LINE__); }               \
}
#endif

#ifndef ReleaseCom
#define ReleaseCom(x) { if(x){ x->Release(); x = 0; } }
#endif

	inline void ThrowIfFailedV2(HRESULT hr)
	{
		if (FAILED(hr))
		{
			throw com_exception(hr);
		}
	}

#pragma endregion

}