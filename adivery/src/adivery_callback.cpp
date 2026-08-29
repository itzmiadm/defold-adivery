#include "adivery_callback.h"

#include <stdlib.h>
#include <string.h>

namespace dmAdivery
{
    static dmScript::LuaCallbackInfo* g_Callback = 0;
    static dmArray<char*> g_Events;
    static dmMutex::HMutex g_Mutex = 0;
    static bool g_AcceptEvents = false;

    static void FreeEvents(dmArray<char*>& events)
    {
        for (uint32_t i = 0; i < events.Size(); ++i)
        {
            free(events[i]);
        }
        events.SetSize(0);
    }

    static void DestroyLuaCallback()
    {
        if (g_Callback)
        {
            dmScript::DestroyCallback(g_Callback);
            g_Callback = 0;
        }
    }

    void InitializeCallbackQueue()
    {
        if (!g_Mutex)
        {
            g_Mutex = dmMutex::New();
        }
        DM_MUTEX_SCOPED_LOCK(g_Mutex);
        g_AcceptEvents = true;
    }

    void FinalizeCallbackQueue()
    {
        dmArray<char*> pending;
        {
            DM_MUTEX_SCOPED_LOCK(g_Mutex);
            g_AcceptEvents = false;
            pending.Swap(g_Events);
        }
        FreeEvents(pending);
        DestroyLuaCallback();
    }

    void SetLuaCallback(lua_State* L, int index)
    {
        DestroyLuaCallback();
        if (!lua_isnoneornil(L, index))
        {
            luaL_checktype(L, index, LUA_TFUNCTION);
            g_Callback = dmScript::CreateCallback(L, index);
        }
    }

    void QueueEvent(const char* json)
    {
        char* copy = strdup(json ? json : "{}");
        DM_MUTEX_SCOPED_LOCK(g_Mutex);
        if (!g_AcceptEvents)
        {
            free(copy);
            return;
        }
        if (g_Events.Full())
        {
            g_Events.OffsetCapacity(8);
        }
        g_Events.Push(copy);
    }

    static void InvokeEvent(const char* json)
    {
        if (!dmScript::IsCallbackValid(g_Callback))
        {
            return;
        }

        lua_State* L = dmScript::GetCallbackLuaContext(g_Callback);
        int top = lua_gettop(L);
        if (!dmScript::SetupCallback(g_Callback))
        {
            return;
        }

        dmScript::JsonToLua(L, json, strlen(json));
        dmScript::PCall(L, 2, 0);
        dmScript::TeardownCallback(g_Callback);
        assert(top == lua_gettop(L));
    }

    void DispatchEvents()
    {
        dmArray<char*> pending;
        {
            DM_MUTEX_SCOPED_LOCK(g_Mutex);
            if (g_Events.Empty())
            {
                return;
            }
            pending.Swap(g_Events);
        }

        for (uint32_t i = 0; i < pending.Size(); ++i)
        {
            InvokeEvent(pending[i]);
        }
        FreeEvents(pending);
    }
}
