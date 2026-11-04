# Korvo native factory application

This is a review candidate for the openvela native Activity/Window framework,
based on Espressif esp-dev-kits commit
`5c06bf1835329d6525f316475f7218aab6acfe79`, factory_demo on ESP32-S31-Korvo.
The application executable is `s31factory`, package
`org.openvela.s31.factorydemo`, entry `FactoryActivity`.

The dashboard retains the upstream 800 x 480 layout and controls: camera preview,
record/stop/playback, SD probe, four physical-button/RGB-LED indicators, USB host
keyboard/mouse status, and the Coffee Demo entry. All 12 coffee screens, their
components, 32 animation functions, fonts, visual styling and event actions are
retained. The 45 original generated image sources are preserved for provenance
but are not linked; the original UI already uses `coffee_assets_get_src()`.

## Native ownership

`FactoryActivity` creates children of `getWindow()->getRoot()`. The process never
creates a display, calls a global screen loader, modifies the display's theme,
or drives `lv_timer_handler()`. The coffee bridge applies LVGL's simple theme to
its own widgets, then the original generated styles. The 12 original screens
are 800 x 480 child panels. Local navigation emits SCREEN_UNLOAD_START,
SCREEN_LOAD_START, SCREEN_LOADED and SCREEN_UNLOADED events with the upstream
delays. The original helper already ignores fade mode and speed.

Animations bind their LVGL `var` to the actual target object, so cancellation is
scoped to the coffee subtree. Delayed navigation, looping animations, generated
objects, the initial-actions object and theme bookkeeping are cleaned up before
the Activity window is removed. One generated instance is allowed per process.
The Activity pauses by destroying coffee and quiescing the backend; returning to
coffee restarts its splash screen. Transient coffee selections are not persisted.

All UI and lifecycle calls run on the native LVGL owner loop. The backend has no
UI callback and cannot retain LVGL objects. A frame lease is copied into a
UI-owned RGB565 draw buffer and released immediately; the buffer outlives the
canvas. Backend shutdown/join completes before the Activity is destroyed.

## Integration and resources

Enable `CONFIG_XMSDEMO_FACTORY` only after the native providers are present:

- `s31factory_backend`: implements `createFactoryBackend()` using
  `FactoryBackend.h`. No hardware fallback is provided. Driver failures and
  unavailable capabilities must be exposed as real state/errno.
- `s31coffee_assets`: implements the C `coffee_assets_init()` and
  `coffee_assets_get_src()` symbols with the preserved enum IDs. The native
  init result is `int` (0 or negative errno). Image-source strings and decoder
  registration must remain valid for process lifetime; init must be idempotent.
  Missing resources keep the dashboard usable and report the error.
- Package `manifest.json` under `/etc/app/s31factory/manifest.json`, following
  the existing native AppFS layout, and install the native `s31factory` program.
  Keep the digit-recognition application and its package independent.

Required display is 800 x 480 with native RGB565 and existing native touch input.
Do not silently scale the generated design or replace its custom fonts. All 10
custom fonts are built from the original generated C files. Enable LVGL
Montserrat 14, 18, 24 and 46 as well; E402 currently omits 18 and 46. Retain LVGL
9.x compatibility APIs used by the generated 9.2.2 export. Required widgets are
canvas, image, label, button, slider, roller, bar, arc, dropdown, textarea, keyboard
and spinbox (the latter helpers are in the retained generated helper unit), plus
the simple theme and text font support. Kconfig lists these dependencies.

Install all 45 coffee assets with their original dimensions, transparency and
notices via the native provider. Image decoding, LVGL filesystem integration,
asset packing and cache policy belong to that provider. No ESP-IDF partition,
esp_mmap_assets, ESP codec API, BSP or FreeRTOS dependency is used by this app.
The preserved `coffee/ui/CMakeLists.txt` is upstream provenance; only this
application's top-level CMake file is used for native compilation.

The preview allocates one 240 x 240 RGB565 LVGL draw buffer (at least 115,200 bytes
plus alignment). The backend separately budgets its camera buffers, six-second
audio buffer and worker stacks. All 12 generated screen trees are instantiated
when coffee opens, plus theme-property bookkeeping and animations. Measure actual
peak native heap and image cache usage during integration; no peak-memory or
performance acceptance result is inferred from source or object compilation.
Use the configured native application stack budget; validate it with the full
screen loop on the board after firmware integration is authorized.

## Measurements and validation boundary

The one-line overlay reports real monotonic render-event duration, intervals
between observed render starts, and capture timestamp deltas for frames presented
to the canvas. It shows `--` without sufficient/fresh samples. Draw statistics
belong to the shared display while this Activity is resumed, include the overlay
and other visible system content, and exclude proof of physical LCD scanout.
Camera intervals include skipped captures; captured and presented counters are
shown separately. There are no hardcoded FPS, fabricated frames or success states.

The handoff's `receipts/` directory records targeted RISC-V object compilation
against the existing E402 compile database. The two missing Montserrat options
are enabled only for those private compile checks. Object compilation does not
validate provider linking, display appearance, actual drivers, touch routes,
shutdown latency or board performance. Root must perform those checks on the
integrated application before accepting it as the final platform demo.

## Upstream attribution

`coffee/ui/` retains the SquareLine Studio generation notices, original fonts,
image sources and project metadata. The adapted dashboard retains Espressif's
2026 copyright and CC0-1.0 identifier from `main/main.c`. `coffee_assets.h` retains
Espressif's Apache-2.0 notice. New native bridge files carry Apache-2.0 identifiers.
Do not replace or remove these notices. Provider-supplied assets must retain their
original notices/license files as well; this candidate does not relicense them.
