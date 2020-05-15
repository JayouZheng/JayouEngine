//
// IObject.h
//

#pragma once

#include "../TypeDef.h"

namespace Core
{
	class IObject
	{
	public:

		int32 Index;

		std::string  Name;
		std::string  PathName;
		std::wstring WPathName;

		bool bSelected = false;
		bool bIsDirty = true;

		bool bMarkAsDeleted = false;
		bool bCanBeDeleted = true;
		bool bCanBeSelected = true;
		bool bNeedRebuild = false;
		bool bIsBuiltIn = false;
		bool bIsVisible = true;

		int NumFrameDirty = 3;

		void MarkAsDirty()
		{
			bIsDirty = true;
			NumFrameDirty = 3;
		}

		void MarkAsDeleted()
		{
			bIsDirty = true;
			bIsVisible = false;
			bMarkAsDeleted = true;
		}

		bool CanbeDeletedNow()
		{
			return bMarkAsDeleted && bCanBeDeleted;
		}

		virtual ~IObject() {}
	};
}