#if defined(DM_PLATFORM_ANDROID)

#include "adivery_callback.h"
#include "adivery_private.h"

#include <dmsdk/dlib/android.h>
#include <jni.h>
#include <stdlib.h>
#include <string.h>

extern "C"
JNIEXPORT void JNICALL Java_com_defold_adivery_AdiveryJNI_nativeEmit(
    JNIEnv* env, jclass, jstring json)
{
    if (!json)
    {
        dmAdivery::QueueEvent("{}");
        return;
    }
    const char* value = env->GetStringUTFChars(json, 0);
    dmAdivery::QueueEvent(value);
    env->ReleaseStringUTFChars(json, value);
}

namespace dmAdivery
{
    struct AdiveryPlatform
    {
        jobject instance;
        char* configured_app_id;

        jmethodID initialize;
        jmethodID shutdown;
        jmethodID set_user_id;
        jmethodID prepare_interstitial;
        jmethodID prepare_rewarded;
        jmethodID prepare_app_open;
        jmethodID show_ad;
        jmethodID show_app_open;
        jmethodID is_loaded;
        jmethodID prepare_banner;
        jmethodID show_banner;
        jmethodID hide_banner;
        jmethodID destroy_banner;
        jmethodID request_native;
        jmethodID record_native_impression;
        jmethodID record_native_click;
        jmethodID destroy_native;
        jmethodID configure_auto_app_open;
        jmethodID on_activated;
        jmethodID on_deactivated;
    };

    static AdiveryPlatform g_Adivery;

    static bool ClearJavaException(JNIEnv* env, const char* operation)
    {
        if (!env->ExceptionCheck())
        {
            return false;
        }
        env->ExceptionDescribe();
        env->ExceptionClear();
        dmLogError("Java exception while calling Adivery.%s", operation);
        return true;
    }

    static jstring NewString(JNIEnv* env, const char* value)
    {
        return env->NewStringUTF(value ? value : "");
    }

    static void CallVoid(jmethodID method, const char* operation)
    {
        if (!g_Adivery.instance || !method) return;
        dmAndroid::ThreadAttacher attacher;
        JNIEnv* env = attacher.GetEnv();
        env->CallVoidMethod(g_Adivery.instance, method);
        ClearJavaException(env, operation);
    }

    static void CallVoidString(jmethodID method, const char* value, const char* operation)
    {
        if (!g_Adivery.instance || !method) return;
        dmAndroid::ThreadAttacher attacher;
        JNIEnv* env = attacher.GetEnv();
        jstring string_value = NewString(env, value);
        env->CallVoidMethod(g_Adivery.instance, method, string_value);
        ClearJavaException(env, operation);
        env->DeleteLocalRef(string_value);
    }

    static bool CallBoolString(jmethodID method, const char* value, const char* operation)
    {
        if (!g_Adivery.instance || !method) return false;
        dmAndroid::ThreadAttacher attacher;
        JNIEnv* env = attacher.GetEnv();
        jstring string_value = NewString(env, value);
        jboolean result = env->CallBooleanMethod(g_Adivery.instance, method, string_value);
        bool failed = ClearJavaException(env, operation);
        env->DeleteLocalRef(string_value);
        return !failed && result == JNI_TRUE;
    }

    void InitializePlatform(dmExtension::Params* params)
    {
        memset(&g_Adivery, 0, sizeof(g_Adivery));
        const char* configured = dmConfigFile::GetString(params->m_ConfigFile, "adivery.app_id", "");
        g_Adivery.configured_app_id = strdup(configured ? configured : "");

        dmAndroid::ThreadAttacher attacher;
        JNIEnv* env = attacher.GetEnv();
        jclass cls = dmAndroid::LoadClass(env, "com.defold.adivery.AdiveryJNI");
        if (!cls)
        {
            dmLogError("Unable to load com.defold.adivery.AdiveryJNI");
            ClearJavaException(env, "load_class");
            return;
        }

        jmethodID constructor = env->GetMethodID(cls, "<init>", "(Landroid/app/Activity;)V");
        jobject local = env->NewObject(cls, constructor, attacher.GetActivity()->clazz);
        g_Adivery.instance = env->NewGlobalRef(local);
        env->DeleteLocalRef(local);

        g_Adivery.initialize = env->GetMethodID(cls, "initialize", "(Ljava/lang/String;)Z");
        g_Adivery.shutdown = env->GetMethodID(cls, "shutdown", "()V");
        g_Adivery.set_user_id = env->GetMethodID(cls, "setUserId", "(Ljava/lang/String;)V");
        g_Adivery.prepare_interstitial = env->GetMethodID(cls, "prepareInterstitial", "(Ljava/lang/String;)V");
        g_Adivery.prepare_rewarded = env->GetMethodID(cls, "prepareRewarded", "(Ljava/lang/String;)V");
        g_Adivery.prepare_app_open = env->GetMethodID(cls, "prepareAppOpen", "(Ljava/lang/String;)V");
        g_Adivery.show_ad = env->GetMethodID(cls, "showAd", "(Ljava/lang/String;)Z");
        g_Adivery.show_app_open = env->GetMethodID(cls, "showAppOpen", "(Ljava/lang/String;)Z");
        g_Adivery.is_loaded = env->GetMethodID(cls, "isLoaded", "(Ljava/lang/String;)Z");
        g_Adivery.prepare_banner = env->GetMethodID(cls, "prepareBanner", "(Ljava/lang/String;IZ)V");
        g_Adivery.show_banner = env->GetMethodID(cls, "showBanner", "(Ljava/lang/String;III)V");
        g_Adivery.hide_banner = env->GetMethodID(cls, "hideBanner", "(Ljava/lang/String;)V");
        g_Adivery.destroy_banner = env->GetMethodID(cls, "destroyBanner", "(Ljava/lang/String;)V");
        g_Adivery.request_native = env->GetMethodID(cls, "requestNative", "(Ljava/lang/String;)V");
        g_Adivery.record_native_impression = env->GetMethodID(cls, "recordNativeImpression", "(Ljava/lang/String;)Z");
        g_Adivery.record_native_click = env->GetMethodID(cls, "recordNativeClick", "(Ljava/lang/String;)Z");
        g_Adivery.destroy_native = env->GetMethodID(cls, "destroyNative", "(Ljava/lang/String;)V");
        g_Adivery.configure_auto_app_open = env->GetMethodID(cls, "configureAutoAppOpen", "(Ljava/lang/String;ZI)V");
        g_Adivery.on_activated = env->GetMethodID(cls, "onActivated", "()V");
        g_Adivery.on_deactivated = env->GetMethodID(cls, "onDeactivated", "()V");
        ClearJavaException(env, "resolve_methods");
        env->DeleteLocalRef(cls);
    }

    void FinalizePlatform()
    {
        if (!g_Adivery.instance) return;
        CallVoid(g_Adivery.shutdown, "shutdown");
        dmAndroid::ThreadAttacher attacher;
        attacher.GetEnv()->DeleteGlobalRef(g_Adivery.instance);
        g_Adivery.instance = 0;
        free(g_Adivery.configured_app_id);
        g_Adivery.configured_app_id = 0;
    }

    bool InitializeSdk(const char* app_id)
    {
        const char* selected = app_id && app_id[0] ? app_id : g_Adivery.configured_app_id;
        return CallBoolString(g_Adivery.initialize, selected, "initialize");
    }

    void SetUserId(const char* value) { CallVoidString(g_Adivery.set_user_id, value, "set_user_id"); }
    void PrepareInterstitial(const char* value) { CallVoidString(g_Adivery.prepare_interstitial, value, "prepare_interstitial"); }
    void PrepareRewarded(const char* value) { CallVoidString(g_Adivery.prepare_rewarded, value, "prepare_rewarded"); }
    void PrepareAppOpen(const char* value) { CallVoidString(g_Adivery.prepare_app_open, value, "prepare_app_open"); }
    bool ShowAd(const char* value) { return CallBoolString(g_Adivery.show_ad, value, "show"); }
    bool ShowAppOpen(const char* value) { return CallBoolString(g_Adivery.show_app_open, value, "show_app_open"); }
    bool IsLoaded(const char* value) { return CallBoolString(g_Adivery.is_loaded, value, "is_loaded"); }

    void PrepareBanner(const char* placement_id, int size, bool retry_on_error)
    {
        if (!g_Adivery.instance || !g_Adivery.prepare_banner) return;
        dmAndroid::ThreadAttacher attacher;
        JNIEnv* env = attacher.GetEnv();
        jstring placement = NewString(env, placement_id);
        env->CallVoidMethod(g_Adivery.instance, g_Adivery.prepare_banner, placement, (jint)size,
                            (jboolean)(retry_on_error ? JNI_TRUE : JNI_FALSE));
        ClearJavaException(env, "prepare_banner");
        env->DeleteLocalRef(placement);
    }

    void ShowBanner(const char* placement_id, int position, int offset_x, int offset_y)
    {
        if (!g_Adivery.instance || !g_Adivery.show_banner) return;
        dmAndroid::ThreadAttacher attacher;
        JNIEnv* env = attacher.GetEnv();
        jstring placement = NewString(env, placement_id);
        env->CallVoidMethod(g_Adivery.instance, g_Adivery.show_banner, placement,
                            (jint)position, (jint)offset_x, (jint)offset_y);
        ClearJavaException(env, "show_banner");
        env->DeleteLocalRef(placement);
    }

    void HideBanner(const char* value) { CallVoidString(g_Adivery.hide_banner, value, "hide_banner"); }
    void DestroyBanner(const char* value) { CallVoidString(g_Adivery.destroy_banner, value, "destroy_banner"); }
    void RequestNative(const char* value) { CallVoidString(g_Adivery.request_native, value, "request_native"); }
    bool RecordNativeImpression(const char* value) { return CallBoolString(g_Adivery.record_native_impression, value, "record_native_impression"); }
    bool RecordNativeClick(const char* value) { return CallBoolString(g_Adivery.record_native_click, value, "record_native_click"); }
    void DestroyNative(const char* value) { CallVoidString(g_Adivery.destroy_native, value, "destroy_native"); }

    void ConfigureAutoAppOpen(const char* placement_id, bool enabled, int minimum_background_seconds)
    {
        if (!g_Adivery.instance || !g_Adivery.configure_auto_app_open) return;
        dmAndroid::ThreadAttacher attacher;
        JNIEnv* env = attacher.GetEnv();
        jstring placement = NewString(env, placement_id);
        env->CallVoidMethod(g_Adivery.instance, g_Adivery.configure_auto_app_open, placement,
                            (jboolean)(enabled ? JNI_TRUE : JNI_FALSE),
                            (jint)minimum_background_seconds);
        ClearJavaException(env, "set_auto_app_open");
        env->DeleteLocalRef(placement);
    }

    void OnAppActivated() { CallVoid(g_Adivery.on_activated, "on_activated"); }
    void OnAppDeactivated() { CallVoid(g_Adivery.on_deactivated, "on_deactivated"); }
}

#endif
