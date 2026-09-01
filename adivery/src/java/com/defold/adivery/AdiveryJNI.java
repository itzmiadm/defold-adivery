package com.defold.adivery;

import android.app.Activity;
import android.app.Application;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.os.Handler;
import android.os.Looper;
import android.text.TextUtils;
import android.util.Base64;
import android.util.DisplayMetrics;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.FrameLayout;

import com.adivery.sdk.Adivery;
import com.adivery.sdk.AdiveryAdListener;
import com.adivery.sdk.AdiveryBannerAdView;
import com.adivery.sdk.AdiveryListener;
import com.adivery.sdk.AdiveryNativeCallback;
import com.adivery.sdk.BannerSize;
import com.adivery.sdk.NativeAd;
import com.adivery.sdk.networks.adivery.AdiveryNativeAd;

import okhttp3.internal.publicsuffix.PublicSuffixDatabase;

import org.json.JSONException;
import org.json.JSONObject;

import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.Map;
import java.util.UUID;
import java.util.concurrent.ConcurrentHashMap;
import java.util.zip.GZIPInputStream;

public final class AdiveryJNI {
    private static final String TYPE_SDK = "sdk";
    private static final String TYPE_INTERSTITIAL = "interstitial";
    private static final String TYPE_REWARDED = "rewarded";
    private static final String TYPE_APP_OPEN = "app_open";
    private static final String TYPE_BANNER = "banner";
    private static final String TYPE_NATIVE = "native";

    private static final String EVENT_INITIALIZED = "initialized";
    private static final String EVENT_LOADED = "loaded";
    private static final String EVENT_SHOWN = "shown";
    private static final String EVENT_CLICKED = "clicked";
    private static final String EVENT_CLOSED = "closed";
    private static final String EVENT_FAILED = "failed";
    private static final String EVENT_NOT_LOADED = "not_loaded";
    private static final String EVENT_DESTROYED = "destroyed";
    private static final String EVENT_LOG = "log";

    private final Activity activity;
    private final Handler mainHandler = new Handler(Looper.getMainLooper());
    private final Map<String, AdiveryBannerAdView> banners =
            new ConcurrentHashMap<String, AdiveryBannerAdView>();
    private final Map<String, FrameLayout> bannerContainers =
            new ConcurrentHashMap<String, FrameLayout>();
    private final Map<String, Integer> bannerSizes =
            new ConcurrentHashMap<String, Integer>();
    private final Map<String, NativeAd> nativeAds =
            new ConcurrentHashMap<String, NativeAd>();
    private final Map<String, String> placementTypes =
            new ConcurrentHashMap<String, String>();

    private AdiveryListener globalListener;
    private volatile boolean active = true;
    private volatile boolean initialized;
    private String appId = "";

    private volatile boolean autoAppOpenEnabled;
    private volatile String autoAppOpenPlacement = "";
    private volatile long minimumBackgroundMillis = 5000L;
    private volatile long deactivatedAt;

    public AdiveryJNI(Activity activity) {
        this.activity = activity;
    }

    private static native void nativeEmit(String json);

    private void emit(String type, String event, String placementId) {
        emit(type, event, placementId, null);
    }

    private void emit(String type, String event, String placementId, JSONObject values) {
        if (!active) {
            return;
        }
        try {
            JSONObject result = values == null ? new JSONObject() : values;
            result.put("type", type);
            result.put("event", event);
            if (placementId != null) {
                result.put("placement_id", placementId);
            }
            nativeEmit(result.toString());
        } catch (JSONException ignored) {
            nativeEmit("{\"type\":\"sdk\",\"event\":\"failed\",\"message\":\"Failed to encode SDK event\"}");
        }
    }

    private void emitFailure(String type, String placementId, String message) {
        JSONObject values = new JSONObject();
        try {
            values.put("message", message == null ? "Unknown Adivery error" : message);
        } catch (JSONException ignored) {
        }
        emit(type, EVENT_FAILED, placementId, values);
    }

    public synchronized boolean initialize(String requestedAppId) {
        final String candidate = requestedAppId == null ? "" : requestedAppId.trim();
        if (TextUtils.isEmpty(candidate)) {
            emitFailure(TYPE_SDK, null,
                    "Adivery app id is empty. Pass it to initialize() or set adivery.app_id in game.project.");
            return false;
        }
        if (initialized) {
            if (!candidate.equals(appId)) {
                emitFailure(TYPE_SDK, null, "Adivery is already initialized with another app id.");
                return false;
            }
            emit(TYPE_SDK, EVENT_INITIALIZED, null);
            return true;
        }

        try {
            if (!installOkHttpPublicSuffixDatabase()) {
                return false;
            }
            Application application = activity.getApplication();
            Adivery.configure(application, candidate);
            installGlobalListener();
            appId = candidate;
            initialized = true;
            emit(TYPE_SDK, EVENT_INITIALIZED, null);
            return true;
        } catch (RuntimeException exception) {
            emitFailure(TYPE_SDK, null, exception.getMessage());
            return false;
        }
    }

    /**
     * Defold's Android extender dexes Maven JARs but does not currently copy
     * arbitrary classpath resources from them into the APK. OkHttp requires
     * publicsuffixes.gz before its DNS-over-HTTPS client can make a request.
     * The extension bundles the unmodified OkHttp 4.12.0 resource as an Android
     * asset, so prime OkHttp's singleton explicitly before configuring Adivery.
     */
    private boolean installOkHttpPublicSuffixDatabase() {
        final String assetPath = "okhttp3/internal/publicsuffix/publicsuffixes.gz";
        try (InputStream asset = activity.getAssets().open(assetPath);
             GZIPInputStream gzip = new GZIPInputStream(asset);
             DataInputStream input = new DataInputStream(gzip)) {
            byte[] rules = readLengthPrefixedBytes(input);
            byte[] exceptions = readLengthPrefixedBytes(input);
            PublicSuffixDatabase.Companion.get().setListBytes(rules, exceptions);
            return true;
        } catch (IOException | RuntimeException exception) {
            emitFailure(TYPE_SDK, null,
                    "Unable to initialize OkHttp public suffix database: " + exception);
            return false;
        }
    }

    private static byte[] readLengthPrefixedBytes(DataInputStream input) throws IOException {
        int length = input.readInt();
        if (length < 0 || length > 1024 * 1024) {
            throw new IOException("Invalid public suffix data length: " + length);
        }
        byte[] result = new byte[length];
        input.readFully(result);
        return result;
    }

    private synchronized void installGlobalListener() {
        if (globalListener != null) {
            return;
        }
        globalListener = new AdiveryListener() {
            @Override
            public void onInterstitialAdLoaded(String placementId) {
                emit(TYPE_INTERSTITIAL, EVENT_LOADED, placementId);
            }

            @Override
            public void onInterstitialAdShown(String placementId) {
                emit(TYPE_INTERSTITIAL, EVENT_SHOWN, placementId);
            }

            @Override
            public void onInterstitialAdClicked(String placementId) {
                emit(TYPE_INTERSTITIAL, EVENT_CLICKED, placementId);
            }

            @Override
            public void onInterstitialAdClosed(String placementId) {
                emit(TYPE_INTERSTITIAL, EVENT_CLOSED, placementId);
            }

            @Override
            public void onRewardedAdLoaded(String placementId) {
                emit(TYPE_REWARDED, EVENT_LOADED, placementId);
            }

            @Override
            public void onRewardedAdShown(String placementId) {
                emit(TYPE_REWARDED, EVENT_SHOWN, placementId);
            }

            @Override
            public void onRewardedAdClicked(String placementId) {
                emit(TYPE_REWARDED, EVENT_CLICKED, placementId);
            }

            @Override
            public void onRewardedAdClosed(String placementId, boolean rewarded) {
                JSONObject values = new JSONObject();
                try {
                    values.put("rewarded", rewarded);
                } catch (JSONException ignored) {
                }
                emit(TYPE_REWARDED, EVENT_CLOSED, placementId, values);
            }

            @Override
            public void onAppOpenAdLoaded(String placementId) {
                emit(TYPE_APP_OPEN, EVENT_LOADED, placementId);
            }

            @Override
            public void onAppOpenAdShown(String placementId) {
                emit(TYPE_APP_OPEN, EVENT_SHOWN, placementId);
            }

            @Override
            public void onAppOpenAdClicked(String placementId) {
                emit(TYPE_APP_OPEN, EVENT_CLICKED, placementId);
            }

            @Override
            public void onAppOpenAdClosed(String placementId) {
                emit(TYPE_APP_OPEN, EVENT_CLOSED, placementId);
            }

            @Override
            public void log(String placementId, String message) {
                JSONObject values = new JSONObject();
                try {
                    values.put("message", message == null ? "" : message);
                } catch (JSONException ignored) {
                }
                emit(TYPE_SDK, EVENT_LOG, placementId, values);
            }
        };
        Adivery.addGlobalListener(globalListener);
    }

    private boolean requireInitialized(String type, String placementId) {
        if (initialized) {
            return true;
        }
        emitFailure(type, placementId, "Call adivery.initialize() before requesting ads.");
        return false;
    }

    private boolean requirePlacement(String type, String placementId) {
        if (!TextUtils.isEmpty(placementId)) {
            return true;
        }
        emitFailure(type, null, "Placement id must not be empty.");
        return false;
    }

    public void setUserId(String userId) {
        if (!requireInitialized(TYPE_SDK, null)) return;
        Adivery.setUserId(userId == null ? "" : userId);
    }

    public void prepareInterstitial(final String placementId) {
        if (!requireInitialized(TYPE_INTERSTITIAL, placementId)
                || !requirePlacement(TYPE_INTERSTITIAL, placementId)) return;
        mainHandler.post(new Runnable() {
            @Override
            public void run() {
                try {
                    placementTypes.put(placementId, TYPE_INTERSTITIAL);
                    Adivery.prepareInterstitialAd(activity, placementId);
                } catch (RuntimeException exception) {
                    emitFailure(TYPE_INTERSTITIAL, placementId, exception.getMessage());
                }
            }
        });
    }

    public void prepareRewarded(final String placementId) {
        if (!requireInitialized(TYPE_REWARDED, placementId)
                || !requirePlacement(TYPE_REWARDED, placementId)) return;
        mainHandler.post(new Runnable() {
            @Override
            public void run() {
                try {
                    placementTypes.put(placementId, TYPE_REWARDED);
                    Adivery.prepareRewardedAd(activity, placementId);
                } catch (RuntimeException exception) {
                    emitFailure(TYPE_REWARDED, placementId, exception.getMessage());
                }
            }
        });
    }

    public void prepareAppOpen(final String placementId) {
        if (!requireInitialized(TYPE_APP_OPEN, placementId)
                || !requirePlacement(TYPE_APP_OPEN, placementId)) return;
        mainHandler.post(new Runnable() {
            @Override
            public void run() {
                try {
                    Adivery.prepareAppOpenAd(activity, placementId);
                } catch (RuntimeException exception) {
                    emitFailure(TYPE_APP_OPEN, placementId, exception.getMessage());
                }
            }
        });
    }

    public boolean showAd(final String placementId) {
        String knownType = placementTypes.get(placementId);
        final String type = knownType == null ? TYPE_SDK : knownType;
        if (!requireInitialized(type, placementId)) {
            return false;
        }
        if (!Adivery.isLoaded(placementId)) {
            emit(type, EVENT_NOT_LOADED, placementId);
            return false;
        }
        mainHandler.post(new Runnable() {
            @Override
            public void run() {
                try {
                    Adivery.showAd(placementId);
                } catch (RuntimeException exception) {
                    emitFailure(type, placementId, exception.getMessage());
                }
            }
        });
        return true;
    }

    public boolean showAppOpen(final String placementId) {
        if (!requireInitialized(TYPE_APP_OPEN, placementId)) {
            return false;
        }
        if (!Adivery.isLoaded(placementId)) {
            emit(TYPE_APP_OPEN, EVENT_NOT_LOADED, placementId);
            return false;
        }
        mainHandler.post(new Runnable() {
            @Override
            public void run() {
                try {
                    Adivery.showAppOpenAd(activity, placementId);
                } catch (RuntimeException exception) {
                    emitFailure(TYPE_APP_OPEN, placementId, exception.getMessage());
                }
            }
        });
        return true;
    }

    public boolean isLoaded(String placementId) {
        return initialized && !TextUtils.isEmpty(placementId) && Adivery.isLoaded(placementId);
    }

    public void prepareBanner(final String placementId, final int size, final boolean retryOnError) {
        if (!requireInitialized(TYPE_BANNER, placementId)
                || !requirePlacement(TYPE_BANNER, placementId)) return;
        mainHandler.post(new Runnable() {
            @Override
            public void run() {
                destroyBannerOnUiThread(placementId, false);
                try {
                    final AdiveryBannerAdView banner = new AdiveryBannerAdView(activity);
                    banner.setPlacementId(placementId);
                    banner.setRetryOnError(retryOnError);
                    banner.setBannerSize(toBannerSize(size));
                    banner.setVisibility(View.VISIBLE);
                    banner.setBannerAdListener(new AdiveryAdListener() {
                        @Override
                        public void onAdLoaded() {
                            if (banners.get(placementId) == banner) {
                                emit(TYPE_BANNER, EVENT_LOADED, placementId);
                            }
                        }

                        @Override
                        public void onAdShown() {
                            if (banners.get(placementId) == banner) {
                                emit(TYPE_BANNER, EVENT_SHOWN, placementId);
                            }
                        }

                        @Override
                        public void onAdClicked() {
                            if (banners.get(placementId) == banner) {
                                emit(TYPE_BANNER, EVENT_CLICKED, placementId);
                            }
                        }

                        @Override
                        public void onError(String reason) {
                            if (banners.get(placementId) == banner) {
                                emitFailure(TYPE_BANNER, placementId, reason);
                            }
                        }
                    });

                    FrameLayout container = new FrameLayout(activity);
                    container.setVisibility(View.VISIBLE);
                    container.addView(banner, new FrameLayout.LayoutParams(
                            ViewGroup.LayoutParams.MATCH_PARENT,
                            ViewGroup.LayoutParams.MATCH_PARENT));
                    bannerContainers.put(placementId, container);
                    bannerSizes.put(placementId, size);
                    banners.put(placementId, banner);
                    banner.loadAd();
                } catch (RuntimeException exception) {
                    destroyBannerOnUiThread(placementId, false);
                    emitFailure(TYPE_BANNER, placementId, exception.getMessage());
                }
            }
        });
    }

    public void showBanner(final String placementId, final int position,
                           final int offsetX, final int offsetY) {
        mainHandler.post(new Runnable() {
            @Override
            public void run() {
                AdiveryBannerAdView banner = banners.get(placementId);
                FrameLayout container = bannerContainers.get(placementId);
                if (banner == null || container == null) {
                    emitFailure(TYPE_BANNER, placementId,
                            "Banner does not exist. Call prepare_banner() first.");
                    return;
                }
                Integer requestedSize = bannerSizes.get(placementId);
                WindowManager.LayoutParams params = bannerWindowLayout(
                        requestedSize == null ? 0 : requestedSize,
                        toGravity(position), offsetX, offsetY);
                try {
                    container.setSystemUiVisibility(
                            activity.getWindow().getDecorView().getSystemUiVisibility());
                    WindowManager windowManager = activity.getWindowManager();
                    if (container.isAttachedToWindow()) {
                        windowManager.updateViewLayout(container, params);
                    } else {
                        windowManager.addView(container, params);
                    }
                    emit(TYPE_BANNER, EVENT_SHOWN, placementId);
                } catch (RuntimeException exception) {
                    emitFailure(TYPE_BANNER, placementId, exception.getMessage());
                }
            }
        });
    }

    public void hideBanner(final String placementId) {
        mainHandler.post(new Runnable() {
            @Override
            public void run() {
                AdiveryBannerAdView banner = banners.get(placementId);
                FrameLayout container = bannerContainers.get(placementId);
                if (banner != null && container != null && container.isAttachedToWindow()) {
                    activity.getWindowManager().removeView(container);
                }
            }
        });
    }

    public void destroyBanner(final String placementId) {
        mainHandler.post(new Runnable() {
            @Override
            public void run() {
                destroyBannerOnUiThread(placementId, true);
            }
        });
    }

    private void destroyBannerOnUiThread(String placementId, boolean notify) {
        AdiveryBannerAdView banner = banners.remove(placementId);
        FrameLayout container = bannerContainers.remove(placementId);
        bannerSizes.remove(placementId);
        if (banner == null && container == null) return;
        if (container != null) {
            if (container.isAttachedToWindow()) {
                activity.getWindowManager().removeView(container);
            }
            container.removeAllViews();
        } else if (banner != null) {
            ViewGroup parent = (ViewGroup) banner.getParent();
            if (parent != null) parent.removeView(banner);
        }
        if (notify) emit(TYPE_BANNER, EVENT_DESTROYED, placementId);
    }

    private BannerSize toBannerSize(int size) {
        switch (size) {
            case 1: return BannerSize.SMART_BANNER;
            case 2: return BannerSize.LARGE_BANNER;
            case 3: return BannerSize.MEDIUM_RECTANGLE;
            default: return BannerSize.BANNER;
        }
    }

    private int toGravity(int position) {
        switch (position) {
            case 0: return Gravity.TOP | Gravity.LEFT;
            case 1: return Gravity.TOP | Gravity.CENTER_HORIZONTAL;
            case 2: return Gravity.TOP | Gravity.RIGHT;
            case 3: return Gravity.BOTTOM | Gravity.LEFT;
            case 5: return Gravity.BOTTOM | Gravity.RIGHT;
            case 6: return Gravity.CENTER;
            default: return Gravity.BOTTOM | Gravity.CENTER_HORIZONTAL;
        }
    }

    private FrameLayout.LayoutParams bannerLayout(int size, int gravity) {
        int widthDp;
        int heightDp;
        switch (size) {
            case 1:
                return new FrameLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT,
                        ViewGroup.LayoutParams.WRAP_CONTENT,
                        gravity);
            case 2:
                widthDp = 320;
                heightDp = 100;
                break;
            case 3:
                widthDp = 300;
                heightDp = 250;
                break;
            default:
                widthDp = 320;
                heightDp = 50;
                break;
        }
        return new FrameLayout.LayoutParams(dp(widthDp), dp(heightDp), gravity);
    }

    private WindowManager.LayoutParams bannerWindowLayout(
            int size, int gravity, int offsetX, int offsetY) {
        FrameLayout.LayoutParams dimensions = bannerLayout(size, gravity);
        WindowManager.LayoutParams params = new WindowManager.LayoutParams();
        params.width = dimensions.width;
        params.height = dimensions.height;
        params.flags = WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL
                | WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE;
        params.gravity = gravity;
        params.x = offsetX;
        params.y = offsetY;
        return params;
    }

    private int dp(int value) {
        DisplayMetrics metrics = activity.getResources().getDisplayMetrics();
        return Math.round(value * metrics.density);
    }

    public void requestNative(final String placementId) {
        if (!requireInitialized(TYPE_NATIVE, placementId)
                || !requirePlacement(TYPE_NATIVE, placementId)) return;
        Adivery.requestNativeAd(activity, placementId, new AdiveryNativeCallback() {
            @Override
            public void onAdLoaded(NativeAd ad) {
                if (!(ad instanceof AdiveryNativeAd)) {
                    emitFailure(TYPE_NATIVE, placementId, "Unsupported native ad implementation.");
                    return;
                }
                AdiveryNativeAd nativeAd = (AdiveryNativeAd) ad;
                String id = UUID.randomUUID().toString();
                nativeAds.put(id, ad);

                JSONObject values = new JSONObject();
                try {
                    values.put("native_id", id);
                    putNullable(values, "headline", nativeAd.getHeadline());
                    putNullable(values, "description", nativeAd.getDescription());
                    putNullable(values, "advertiser", nativeAd.getAdvertiser());
                    putNullable(values, "call_to_action", nativeAd.getCallToAction());
                    putNullable(values, "icon_url", nativeAd.getIconUrl());
                    putNullable(values, "image_url", nativeAd.getImageUrl());
                    putNullable(values, "icon", drawableToBase64(nativeAd.getIcon()));
                    putNullable(values, "image", drawableToBase64(nativeAd.getImage()));
                } catch (JSONException exception) {
                    nativeAds.remove(id);
                    emitFailure(TYPE_NATIVE, placementId, exception.getMessage());
                    return;
                }
                emit(TYPE_NATIVE, EVENT_LOADED, placementId, values);
            }

            @Override
            public void onAdLoadFailed(String reason) {
                emitFailure(TYPE_NATIVE, placementId, reason);
            }

            @Override
            public void onAdShowFailed(String reason) {
                emitFailure(TYPE_NATIVE, placementId, reason);
            }

            @Override
            public void onAdShown() {
                emit(TYPE_NATIVE, EVENT_SHOWN, placementId);
            }

            @Override
            public void onAdClicked() {
                emit(TYPE_NATIVE, EVENT_CLICKED, placementId);
            }
        });
    }

    public boolean recordNativeImpression(String nativeId) {
        NativeAd ad = nativeAds.get(nativeId);
        if (!(ad instanceof AdiveryNativeAd)) return false;
        ((AdiveryNativeAd) ad).recordImpression();
        return true;
    }

    public boolean recordNativeClick(String nativeId) {
        NativeAd ad = nativeAds.get(nativeId);
        if (!(ad instanceof AdiveryNativeAd)) return false;
        ((AdiveryNativeAd) ad).recordClick();
        return true;
    }

    public void destroyNative(String nativeId) {
        nativeAds.remove(nativeId);
    }

    private static void putNullable(JSONObject object, String key, Object value) throws JSONException {
        object.put(key, value == null ? JSONObject.NULL : value);
    }

    private static String drawableToBase64(Drawable drawable) {
        if (drawable == null) return null;
        Bitmap bitmap;
        if (drawable instanceof BitmapDrawable
                && ((BitmapDrawable) drawable).getBitmap() != null) {
            bitmap = ((BitmapDrawable) drawable).getBitmap();
        } else {
            int width = Math.max(1, drawable.getIntrinsicWidth());
            int height = Math.max(1, drawable.getIntrinsicHeight());
            bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
            Canvas canvas = new Canvas(bitmap);
            drawable.setBounds(0, 0, canvas.getWidth(), canvas.getHeight());
            drawable.draw(canvas);
        }
        ByteArrayOutputStream output = new ByteArrayOutputStream();
        bitmap.compress(Bitmap.CompressFormat.JPEG, 90, output);
        return Base64.encodeToString(output.toByteArray(), Base64.NO_WRAP);
    }

    public void configureAutoAppOpen(String placementId, boolean enabled,
                                     int minimumBackgroundSeconds) {
        autoAppOpenPlacement = placementId == null ? "" : placementId;
        autoAppOpenEnabled = enabled && !TextUtils.isEmpty(autoAppOpenPlacement);
        minimumBackgroundMillis = Math.max(0, minimumBackgroundSeconds) * 1000L;
    }

    public void onDeactivated() {
        deactivatedAt = System.currentTimeMillis();
    }

    public void onActivated() {
        if (!autoAppOpenEnabled || deactivatedAt == 0L) return;
        long elapsed = System.currentTimeMillis() - deactivatedAt;
        deactivatedAt = 0L;
        if (elapsed >= minimumBackgroundMillis) {
            showAppOpen(autoAppOpenPlacement);
        }
    }

    public void shutdown() {
        active = false;
        autoAppOpenEnabled = false;
        final ArrayList<String> ids = new ArrayList<String>(banners.keySet());
        mainHandler.post(new Runnable() {
            @Override
            public void run() {
                for (String id : ids) {
                    destroyBannerOnUiThread(id, false);
                }
            }
        });
        nativeAds.clear();
        placementTypes.clear();
    }
}
