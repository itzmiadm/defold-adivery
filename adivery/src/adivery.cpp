#define EXTENSION_NAME AdiveryExt
#define LIB_NAME "Adivery"
#define MODULE_NAME "adivery"

#define DLIB_LOG_DOMAIN LIB_NAME
#include <dmsdk/sdk.h>

#if defined(DM_PLATFORM_ANDROID)

#include "adivery_callback.h"
#include "adivery_private.h"

namespace dmAdivery
{
    static const char* CheckString(lua_State* L, int index, const char* argument)
    {
        if (lua_type(L, index) != LUA_TSTRING)
        {
            luaL_error(L, "%s must be a string, got %s", argument, luaL_typename(L, index));
            return 0;
        }
        return lua_tostring(L, index);
    }

    static int LuaInitialize(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        const char* app_id = 0;
        if (!lua_isnoneornil(L, 1))
        {
            app_id = CheckString(L, 1, "app_id");
        }
        lua_pushboolean(L, InitializeSdk(app_id));
        return 1;
    }

    static int LuaSetCallback(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        SetLuaCallback(L, 1);
        return 0;
    }

    static int LuaSetUserId(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        SetUserId(CheckString(L, 1, "user_id"));
        return 0;
    }

    static int LuaPrepareInterstitial(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        PrepareInterstitial(CheckString(L, 1, "placement_id"));
        return 0;
    }

    static int LuaPrepareRewarded(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        PrepareRewarded(CheckString(L, 1, "placement_id"));
        return 0;
    }

    static int LuaPrepareAppOpen(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        PrepareAppOpen(CheckString(L, 1, "placement_id"));
        return 0;
    }

    static int LuaShow(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushboolean(L, ShowAd(CheckString(L, 1, "placement_id")));
        return 1;
    }

    static int LuaShowAppOpen(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushboolean(L, ShowAppOpen(CheckString(L, 1, "placement_id")));
        return 1;
    }

    static int LuaIsLoaded(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushboolean(L, IsLoaded(CheckString(L, 1, "placement_id")));
        return 1;
    }

    static int LuaPrepareBanner(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        const char* placement_id = CheckString(L, 1, "placement_id");
        int size = luaL_optinteger(L, 2, 0);
        bool retry = lua_isnoneornil(L, 3) ? true : lua_toboolean(L, 3);
        PrepareBanner(placement_id, size, retry);
        return 0;
    }

    static int LuaShowBanner(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        const char* placement_id = CheckString(L, 1, "placement_id");
        int position = luaL_optinteger(L, 2, 4);
        int offset_x = luaL_optinteger(L, 3, 0);
        int offset_y = luaL_optinteger(L, 4, 0);
        ShowBanner(placement_id, position, offset_x, offset_y);
        return 0;
    }

    static int LuaHideBanner(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        HideBanner(CheckString(L, 1, "placement_id"));
        return 0;
    }

    static int LuaDestroyBanner(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        DestroyBanner(CheckString(L, 1, "placement_id"));
        return 0;
    }

    static int LuaRequestNative(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        RequestNative(CheckString(L, 1, "placement_id"));
        return 0;
    }

    static int LuaRecordNativeImpression(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushboolean(L, RecordNativeImpression(CheckString(L, 1, "native_id")));
        return 1;
    }

    static int LuaRecordNativeClick(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushboolean(L, RecordNativeClick(CheckString(L, 1, "native_id")));
        return 1;
    }

    static int LuaDestroyNative(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        DestroyNative(CheckString(L, 1, "native_id"));
        return 0;
    }

    static int LuaSetAutoAppOpen(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        const char* placement_id = CheckString(L, 1, "placement_id");
        bool enabled = lua_isnoneornil(L, 2) ? true : lua_toboolean(L, 2);
        int seconds = luaL_optinteger(L, 3, 5);
        ConfigureAutoAppOpen(placement_id, enabled, seconds < 0 ? 0 : seconds);
        return 0;
    }

    static const luaL_reg ModuleMethods[] =
    {
        {"initialize", LuaInitialize},
        {"set_callback", LuaSetCallback},
        {"set_user_id", LuaSetUserId},
        {"prepare_interstitial", LuaPrepareInterstitial},
        {"prepare_rewarded", LuaPrepareRewarded},
        {"prepare_app_open", LuaPrepareAppOpen},
        {"show", LuaShow},
        {"show_app_open", LuaShowAppOpen},
        {"is_loaded", LuaIsLoaded},
        {"prepare_banner", LuaPrepareBanner},
        {"show_banner", LuaShowBanner},
        {"hide_banner", LuaHideBanner},
        {"destroy_banner", LuaDestroyBanner},
        {"request_native", LuaRequestNative},
        {"record_native_impression", LuaRecordNativeImpression},
        {"record_native_click", LuaRecordNativeClick},
        {"destroy_native", LuaDestroyNative},
        {"set_auto_app_open", LuaSetAutoAppOpen},
        {0, 0}
    };

    static void SetStringConstant(lua_State* L, const char* name, const char* value)
    {
        lua_pushstring(L, value);
        lua_setfield(L, -2, name);
    }

    static void LuaInit(lua_State* L)
    {
        luaL_register(L, MODULE_NAME, ModuleMethods);

        SetStringConstant(L, "TYPE_SDK", "sdk");
        SetStringConstant(L, "TYPE_INTERSTITIAL", "interstitial");
        SetStringConstant(L, "TYPE_REWARDED", "rewarded");
        SetStringConstant(L, "TYPE_APP_OPEN", "app_open");
        SetStringConstant(L, "TYPE_BANNER", "banner");
        SetStringConstant(L, "TYPE_NATIVE", "native");

        SetStringConstant(L, "EVENT_INITIALIZED", "initialized");
        SetStringConstant(L, "EVENT_LOADED", "loaded");
        SetStringConstant(L, "EVENT_SHOWN", "shown");
        SetStringConstant(L, "EVENT_CLICKED", "clicked");
        SetStringConstant(L, "EVENT_CLOSED", "closed");
        SetStringConstant(L, "EVENT_FAILED", "failed");
        SetStringConstant(L, "EVENT_NOT_LOADED", "not_loaded");
        SetStringConstant(L, "EVENT_DESTROYED", "destroyed");
        SetStringConstant(L, "EVENT_LOG", "log");

        lua_pushinteger(L, 0); lua_setfield(L, -2, "BANNER_SIZE_BANNER");
        lua_pushinteger(L, 1); lua_setfield(L, -2, "BANNER_SIZE_SMART");
        lua_pushinteger(L, 2); lua_setfield(L, -2, "BANNER_SIZE_LARGE");
        lua_pushinteger(L, 3); lua_setfield(L, -2, "BANNER_SIZE_MEDIUM_RECTANGLE");

        lua_pushinteger(L, 0); lua_setfield(L, -2, "POSITION_TOP_LEFT");
        lua_pushinteger(L, 1); lua_setfield(L, -2, "POSITION_TOP_CENTER");
        lua_pushinteger(L, 2); lua_setfield(L, -2, "POSITION_TOP_RIGHT");
        lua_pushinteger(L, 3); lua_setfield(L, -2, "POSITION_BOTTOM_LEFT");
        lua_pushinteger(L, 4); lua_setfield(L, -2, "POSITION_BOTTOM_CENTER");
        lua_pushinteger(L, 5); lua_setfield(L, -2, "POSITION_BOTTOM_RIGHT");
        lua_pushinteger(L, 6); lua_setfield(L, -2, "POSITION_CENTER");

        lua_pop(L, 1);
    }

    static dmExtension::Result Initialize(dmExtension::Params* params)
    {
        LuaInit(params->m_L);
        InitializeCallbackQueue();
        InitializePlatform(params);
        return dmExtension::RESULT_OK;
    }

    static dmExtension::Result Update(dmExtension::Params*)
    {
        DispatchEvents();
        return dmExtension::RESULT_OK;
    }

    static dmExtension::Result Finalize(dmExtension::Params*)
    {
        FinalizePlatform();
        FinalizeCallbackQueue();
        return dmExtension::RESULT_OK;
    }

    static void OnEvent(dmExtension::Params*, const dmExtension::Event* event)
    {
        if (event->m_Event == dmExtension::EVENT_ID_ACTIVATEAPP)
        {
            OnAppActivated();
        }
        else if (event->m_Event == dmExtension::EVENT_ID_DEACTIVATEAPP)
        {
            OnAppDeactivated();
        }
    }
}

DM_DECLARE_EXTENSION(EXTENSION_NAME, LIB_NAME, 0, 0, dmAdivery::Initialize,
                     dmAdivery::Update, dmAdivery::OnEvent, dmAdivery::Finalize)

#else

static dmExtension::Result InitializeAdiveryNull(dmExtension::Params*)
{
    dmLogInfo("Adivery extension is available on Android only");
    return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(EXTENSION_NAME, LIB_NAME, 0, 0, InitializeAdiveryNull, 0, 0, 0)

#endif
