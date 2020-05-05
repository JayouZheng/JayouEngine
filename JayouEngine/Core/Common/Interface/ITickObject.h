//
// ITickObject.h
//

#pragma once

class ITickObject
{
public:

	virtual void Init() = 0;
	virtual void Tick() = 0;
};