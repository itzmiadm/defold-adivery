# Adivery for Defold

[فارسی](README-fa.md)

An Android native extension that brings the Adivery advertising SDK to the
Defold game engine. It is an independent port inspired by
[godot-adivery](https://github.com/DexterFstone/godot-adivery) and follows the
callback/bridge architecture used by Defold's official
[extension-admob](https://github.com/defold/extension-admob).

## Features

- Rewarded, interstitial, and App Open ads
- Banner ads with four Adivery sizes and seven screen positions
- Native ads, including creative fields and explicit click/impression tracking
- Publisher user IDs
- Optional automatic App Open display after returning from background
- One global Adivery listener and a thread-safe event queue for Lua callbacks
- Android-only implementation with safe no-op registration on other platforms
- GitHub Actions build; no Android SDK or Adivery artifact is vendored

The extension currently pins `com.adivery:sdk:4.9.0`, the latest release shown
by Maven Central metadata (last updated 29 June 2026). Minimum Android API is
23. This project is not an official Adivery product.

The required OkHttp public-suffix database is bundled and initialized by the
extension. This avoids `Unable to load publicsuffixes.gz` failures caused by
Defold's Android extender omitting arbitrary Maven JAR resources from the APK.

## Installation

Add the repository archive to your Defold project's dependencies:

```ini
[project]
dependencies#0 = https://github.com/itzmiadm/defold-adivery/archive/refs/heads/main.zip
```

Fetch libraries from **Project > Fetch Libraries**. Set the application ID from
your Adivery panel:

```ini
[adivery]
app_id = YOUR_ADIVERY_APP_ID

[android]
minimum_sdk_version = 23
```

You can omit the project setting and pass the app ID directly to
`adivery.initialize("YOUR_ADIVERY_APP_ID")` instead. Placement IDs are never
hard-coded by the extension.

## Quick start

```lua
local REWARDED = "YOUR_REWARDED_PLACEMENT_ID"

local function on_adivery_event(self, event)
    pprint(event)

    if event.type == adivery.TYPE_REWARDED then
        if event.event == adivery.EVENT_LOADED then
            adivery.show(event.placement_id)
        elseif event.event == adivery.EVENT_CLOSED and event.rewarded then
            -- Grant the reward only here.
            print("reward earned")
        end
    end
end

function init(self)
    if not adivery then return end -- Android bundle only
    adivery.set_callback(on_adivery_event)
    assert(adivery.initialize())
    adivery.set_user_id("publisher-user-id") -- optional
    adivery.prepare_rewarded(REWARDED)
end

function final(self)
    if adivery then adivery.set_callback(nil) end
end
```

`adivery.show()` and `adivery.show_app_open()` return `false` and emit a
`not_loaded` event when the placement is not ready. Always grant a reward only
when the rewarded `closed` event contains `rewarded = true`.

## Events

The callback receives `function(self, event)`. Every event contains `type` and
`event`; ad events also contain `placement_id`.

| `event.type` | Typical events |
| --- | --- |
| `adivery.TYPE_SDK` | `initialized`, `log`, `failed`, `not_loaded` |
| `adivery.TYPE_INTERSTITIAL` | `loaded`, `shown`, `clicked`, `closed` |
| `adivery.TYPE_REWARDED` | `loaded`, `shown`, `clicked`, `closed` (`rewarded`) |
| `adivery.TYPE_APP_OPEN` | `loaded`, `shown`, `clicked`, `closed`, `not_loaded` |
| `adivery.TYPE_BANNER` | `loaded`, `shown`, `clicked`, `failed`, `destroyed` |
| `adivery.TYPE_NATIVE` | `loaded`, `shown`, `clicked`, `failed` |

Failures include `message`. SDK log events include Adivery's `message` and may
include `placement_id`.

## Interstitial and App Open

```lua
adivery.prepare_interstitial("PLACEMENT_ID")
if adivery.is_loaded("PLACEMENT_ID") then
    adivery.show("PLACEMENT_ID")
end

adivery.prepare_app_open("APP_OPEN_PLACEMENT_ID")
adivery.show_app_open("APP_OPEN_PLACEMENT_ID")

-- Show only if already loaded after at least 5 seconds in background.
adivery.set_auto_app_open("APP_OPEN_PLACEMENT_ID", true, 5)
```

Automatic App Open never loads an ad by itself. Prepare the next ad after a
close event if you want it available on a future resume.

## Banners

```lua
local id = "BANNER_PLACEMENT_ID"
adivery.prepare_banner(id, adivery.BANNER_SIZE_BANNER, true)

-- Usually call this after the banner loaded event.
adivery.show_banner(id, adivery.POSITION_BOTTOM_CENTER, 0, 0)
adivery.hide_banner(id)
adivery.destroy_banner(id)
```

Sizes: `BANNER_SIZE_BANNER` (320x50 dp), `BANNER_SIZE_SMART`,
`BANNER_SIZE_LARGE` (320x100 dp), and `BANNER_SIZE_MEDIUM_RECTANGLE` (300x250
dp). Positions cover top/bottom left/center/right and center. Offsets are Android
pixels relative to the selected gravity position.

The SDK banner view is hosted in a bounded, non-focusable Android window using
the same overlay pattern as Defold's official AdMob extension. This prevents its
internal creative view from covering or resizing the Defold surface. Repeated
preparation of the same placement also ignores callbacks from an old, destroyed
banner view.

## Native ads

```lua
local native_id

local function callback(self, event)
    if event.type == adivery.TYPE_NATIVE and event.event == adivery.EVENT_LOADED then
        native_id = event.native_id
        -- headline, description, advertiser, call_to_action,
        -- icon_url, image_url, icon and image are also available.
        -- icon/image are Base64 JPEG strings without a data URI prefix.
        adivery.record_native_impression(native_id)
    end
end

-- Call when your own native-ad UI is clicked:
adivery.record_native_click(native_id)

-- Call after removing that UI:
adivery.destroy_native(native_id)
```

Your UI must make the ad distinguishable from normal content and comply with
Adivery's current native-ad policies.

## Building and verification

Pushes and pull requests run Defold's reusable bob workflow in GitHub Actions.
That CI resolves the Adivery SDK from Maven Central and compiles the native
extension, keeping generated engines, Android toolchains, AARs, and build output
out of this repository.

Runtime ad delivery still needs an Android device, valid Adivery app/placement
IDs, and an app registered in the Adivery panel.

## License

MIT, copyright 2026 itzmiadm. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)
for required upstream notices. The Adivery SDK has its own terms and policies.
