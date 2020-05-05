//
// InputManager.cpp
//

#include "InputManager.h"

bool WinUtility::InputManager::IsKeyDown(int vkeyCode)
{
	return (GetAsyncKeyState(vkeyCode) & 0x8000) != 0;
}
