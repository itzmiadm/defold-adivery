#pragma once

#include <dmsdk/sdk.h>

namespace dmAdivery
{
    void InitializeCallbackQueue();
    void FinalizeCallbackQueue();
    void SetLuaCallback(lua_State* L, int index);
    void QueueEvent(const char* json);
    void DispatchEvents();
}
