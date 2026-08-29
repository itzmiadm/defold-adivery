#pragma once

#include <dmsdk/sdk.h>

namespace dmAdivery
{
    void InitializePlatform(dmExtension::Params* params);
    void FinalizePlatform();

    bool InitializeSdk(const char* app_id);
    void SetUserId(const char* user_id);
    void PrepareInterstitial(const char* placement_id);
    void PrepareRewarded(const char* placement_id);
    void PrepareAppOpen(const char* placement_id);
    bool ShowAd(const char* placement_id);
    bool ShowAppOpen(const char* placement_id);
    bool IsLoaded(const char* placement_id);

    void PrepareBanner(const char* placement_id, int size, bool retry_on_error);
    void ShowBanner(const char* placement_id, int position, int offset_x, int offset_y);
    void HideBanner(const char* placement_id);
    void DestroyBanner(const char* placement_id);

    void RequestNative(const char* placement_id);
    bool RecordNativeImpression(const char* native_id);
    bool RecordNativeClick(const char* native_id);
    void DestroyNative(const char* native_id);

    void ConfigureAutoAppOpen(const char* placement_id, bool enabled, int minimum_background_seconds);
    void OnAppActivated();
    void OnAppDeactivated();
}
