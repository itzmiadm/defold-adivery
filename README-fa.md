# افزونه Adivery برای Defold

[English](README.md)

یک Native Extension اندروید برای استفاده از تبلیغات ادیوری در موتور Defold.
این پروژه یک پورت مستقل با الهام از
[godot-adivery](https://github.com/DexterFstone/godot-adivery) است و برای پل
Lua/JNI و صف callback از معماری افزونه رسمی
[extension-admob](https://github.com/defold/extension-admob) الگو می‌گیرد.

## قابلیت‌ها

- تبلیغ جایزه‌ای، بینابینی و App Open
- بنر با چهار اندازه ادیوری و هفت موقعیت روی صفحه
- تبلیغ Native به‌همراه اطلاعات محتوا و ثبت صریح کلیک/نمایش
- تنظیم شناسه کاربر ناشر
- نمایش اختیاری App Open پس از بازگشت برنامه از پس‌زمینه
- فقط یک listener سراسری ادیوری و صف thread-safe برای اجرای callback در Lua
- پیاده‌سازی Android و ثبت امن افزونه در سایر پلتفرم‌ها
- بیلد در GitHub Actions، بدون نگهداری AAR یا ابزار Android داخل مخزن

نسخه SDK روی `com.adivery:sdk:4.9.0` قفل شده است؛ این نسخه هنگام به‌روزرسانی
پروژه در ۲۹ ژوئن ۲۰۲۶ آخرین release موجود در Maven Central بوده است. حداقل API
اندروید 23 است. این پروژه محصول رسمی ادیوری نیست.

پایگاه دامنه‌های عمومی موردنیاز OkHttp همراه افزونه بسته‌بندی و هنگام راه‌اندازی
مقداردهی می‌شود. این کار از خطای `Unable to load publicsuffixes.gz` جلوگیری
می‌کند که به‌دلیل حذف resourceهای عمومی JAR توسط Android extender دیفولد رخ می‌دهد.

## نصب

آدرس زیر را به dependencies پروژه Defold اضافه کنید:

```ini
[project]
dependencies#0 = https://github.com/itzmiadm/defold-adivery/archive/refs/heads/main.zip
```

سپس از مسیر **Project > Fetch Libraries** کتابخانه‌ها را دریافت کنید. شناسه
اپلیکیشن را از پنل ادیوری بگیرید و در `game.project` قرار دهید:

```ini
[adivery]
app_id = YOUR_ADIVERY_APP_ID

[android]
minimum_sdk_version = 23
```

همچنین می‌توانید مقدار بالا را خالی بگذارید و شناسه را مستقیماً به
`adivery.initialize("YOUR_ADIVERY_APP_ID")` بدهید. هیچ شناسه تست یا placement
در افزونه hard-code نشده است.

## شروع سریع

```lua
local REWARDED = "YOUR_REWARDED_PLACEMENT_ID"

local function on_adivery_event(self, event)
    pprint(event)

    if event.type == adivery.TYPE_REWARDED then
        if event.event == adivery.EVENT_LOADED then
            adivery.show(event.placement_id)
        elseif event.event == adivery.EVENT_CLOSED and event.rewarded then
            -- جایزه را فقط در این نقطه به بازیکن بدهید.
            print("reward earned")
        end
    end
end

function init(self)
    if not adivery then return end -- فقط در خروجی Android موجود است
    adivery.set_callback(on_adivery_event)
    assert(adivery.initialize())
    adivery.set_user_id("publisher-user-id") -- اختیاری
    adivery.prepare_rewarded(REWARDED)
end

function final(self)
    if adivery then adivery.set_callback(nil) end
end
```

اگر تبلیغ آماده نباشد، `adivery.show()` و `adivery.show_app_open()` مقدار
`false` برمی‌گردانند و رویداد `not_loaded` می‌فرستند. جایزه را فقط وقتی بدهید
که رویداد `closed` مربوط به rewarded دارای `rewarded = true` باشد.

## رویدادها

callback به‌شکل `function(self, event)` فراخوانی می‌شود. همه رویدادها `type` و
`event` دارند و رویدادهای تبلیغ معمولاً دارای `placement_id` نیز هستند.

| `event.type` | رویدادهای معمول |
| --- | --- |
| `adivery.TYPE_SDK` | `initialized`، `log`، `failed`، `not_loaded` |
| `adivery.TYPE_INTERSTITIAL` | `loaded`، `shown`، `clicked`، `closed` |
| `adivery.TYPE_REWARDED` | `loaded`، `shown`، `clicked`، `closed` به‌همراه `rewarded` |
| `adivery.TYPE_APP_OPEN` | `loaded`، `shown`، `clicked`، `closed`، `not_loaded` |
| `adivery.TYPE_BANNER` | `loaded`، `shown`، `clicked`، `failed`، `destroyed` |
| `adivery.TYPE_NATIVE` | `loaded`، `shown`، `clicked`، `failed` |

در خطاها فیلد `message` وجود دارد. رویدادهای log نیز پیام SDK ادیوری را منتقل
می‌کنند.

## تبلیغ بینابینی و App Open

```lua
adivery.prepare_interstitial("PLACEMENT_ID")
if adivery.is_loaded("PLACEMENT_ID") then
    adivery.show("PLACEMENT_ID")
end

adivery.prepare_app_open("APP_OPEN_PLACEMENT_ID")
adivery.show_app_open("APP_OPEN_PLACEMENT_ID")

-- نمایش تبلیغ آماده پس از حداقل ۵ ثانیه حضور در پس‌زمینه
adivery.set_auto_app_open("APP_OPEN_PLACEMENT_ID", true, 5)
```

حالت خودکار خودش تبلیغ را load نمی‌کند. برای بازگشت بعدی، پس از رویداد close
تبلیغ بعدی را prepare کنید.

## بنر

```lua
local id = "BANNER_PLACEMENT_ID"
adivery.prepare_banner(id, adivery.BANNER_SIZE_BANNER, true)

-- بهتر است پس از رویداد loaded اجرا شود.
adivery.show_banner(id, adivery.POSITION_BOTTOM_CENTER, 0, 0)
adivery.hide_banner(id)
adivery.destroy_banner(id)
```

اندازه‌ها شامل `BANNER_SIZE_BANNER` با 320x50 dp، `BANNER_SIZE_SMART`،
`BANNER_SIZE_LARGE` با 320x100 dp و `BANNER_SIZE_MEDIUM_RECTANGLE` با 300x250
dp هستند. offsetها برحسب پیکسل Android و نسبت به موقعیت انتخاب‌شده‌اند.

## تبلیغ Native

```lua
local native_id

local function callback(self, event)
    if event.type == adivery.TYPE_NATIVE and event.event == adivery.EVENT_LOADED then
        native_id = event.native_id
        -- headline، description، advertiser، call_to_action،
        -- icon_url، image_url، icon و image نیز موجودند.
        -- icon و image رشته Base64 با فرمت JPEG و بدون پیشوند data URI هستند.
        adivery.record_native_impression(native_id)
    end
end

-- هنگام کلیک روی رابط کاربری تبلیغ:
adivery.record_native_click(native_id)

-- پس از حذف رابط تبلیغ:
adivery.destroy_native(native_id)
```

نمایش Native باید از محتوای معمول بازی قابل تشخیص باشد و با قوانین فعلی
ادیوری سازگار بماند.

## بیلد و اعتبارسنجی

هر push و pull request با workflow مشترک bob دیفولد در GitHub Actions بیلد
می‌شود. SDK ادیوری در همان CI از Maven Central دریافت می‌شود؛ بنابراین موتور
تولیدشده، AARها، ابزار Android و خروجی build وارد مخزن نمی‌شوند.

تست واقعی تبلیغ به دستگاه Android، شناسه‌های معتبر و اپلیکیشن ثبت‌شده در پنل
ادیوری نیاز دارد.

## مجوز

MIT، Copyright 2026 itzmiadm. اعلان‌های لازم پروژه‌های مبنا در
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) نگهداری شده‌اند. خود SDK ادیوری
شرایط و قوانین جداگانه دارد.
