# VSTGUI / VST3 Wine Cross-Compilation Toolchain

Complete guide for cross-compiling VSTGUI-based standalone apps and VST3 plugins
on Linux using MinGW, targeting Wine as runtime.

## Architecture Overview

```
Linux Host (Ubuntu)
  |
  |-- MinGW-w64 Cross-Compiler (x86_64-w64-mingw32-g++)
  |     |
  |     |-- VSTGUI Fork (giang17/vstgui, branch: feature/mingw-cross-compilation)
  |     |     4 code fixes + 2 build fixes for MinGW compatibility
  |     |
  |     |-- VST3 SDK (steinbergmedia/vst3sdk, cloned with --recursive)
  |     |     1 code fix (aligned_alloc) + symlink vstgui4 -> fork
  |     |
  |     +---> Output: Windows PE32+ DLL (.vst3) or EXE (.exe)
  |
  +-- Wine 11.0/11.1 Runtime
        |
        |-- D2D1 Patches (14 patches, v5.0-stable)
        |     Fix rendering: AA, arcs, fonts, CDT, stroke joins, outline AA
        |
        +---> Runs in DAW (REAPER) or standalone
```

### Repositories

| Repo | URL | Purpose |
|------|-----|---------|
| VSTGUI Fork | https://github.com/giang17/vstgui | MinGW cross-compilation patches |
| VST3 Test Plugin | https://github.com/giang17/vstgui-wine-test-plugin | VST3 plugin with D2D1 rendering tests |
| VSTGUI Test App | https://github.com/giang17/vstgui-wine-test-app | Standalone VSTGUI test application |

---

## Prerequisites

### Packages (Ubuntu/Debian)

```bash
# MinGW cross-compiler
sudo apt install mingw-w64 g++-mingw-w64-x86-64

# Build tools
sudo apt install cmake make

# Wine (11.0 stable or 11.1)
# See https://wiki.winehq.org/Ubuntu for installation
```

### Versions tested

- MinGW-w64 GCC 13 (Ubuntu 24.04 package)
- CMake 3.25+
- Wine 11.0 (stable, `/usr/bin/wine`) and Wine 11.1 (`/usr/local/bin/wine`)
- GPU: NVIDIA with D3D11 support (required for D2D1 rendering in Wine)

---

## Part 1: VSTGUI MinGW Fork

VSTGUI 4.15 does not compile with MinGW out of the box. Our fork applies 4 code
fixes and 2 build fixes, all guarded with `#ifdef __MINGW32__`.

**Branch**: `feature/mingw-cross-compilation` (based on `develop`)

### Fix 1: Task Executor — PPL replaced with std::async

**Problem**: Microsoft's Parallel Patterns Library (`<ppltasks.h>`) is not
available in MinGW.

**File**: `vstgui/lib/platform/win32/win32taskexecutor.cpp`

**Solution**: Replace `concurrency::task` with `std::async`:

```cpp
#ifdef __MINGW32__
#include <future>
#else
#include <ppltasks.h>
#endif

// Parallel queue: launch task with std::async
#ifdef __MINGW32__
f = std::make_shared<std::future<void>>(std::async(std::launch::async,
    [This = shared_from_this()]() {
        This->task();
        This->f = nullptr;
    }));
#else
f = std::make_shared<concurrency::task<void>>([This = shared_from_this()]() {
    This->task();
    This->f = nullptr;
});
#endif

// Serial queue: chain tasks via future::wait()
#ifdef __MINGW32__
if (lastFuture.valid())
{
    auto prev = std::make_shared<std::future<void>>(std::move(lastFuture));
    lastFuture = std::async(std::launch::async,
        [prev, task = std::move(task), This = shared_from_this()]() {
            prev->wait();
            task();
            This->numTasks--;
        });
}
else
{
    lastFuture = std::async(std::launch::async,
        [task = std::move(task), This = shared_from_this()]() {
            task();
            This->numTasks--;
        });
}
#else
// MS PPL task_completion_event chain...
#endif
```

### Fix 2: UTF-16 Conversion — Manual surrogate pair handling

**Problem**: `std::wstring_convert` is deprecated in C++17 and unavailable in
some MinGW configurations.

**File**: `vstgui/lib/platform/win32/win32frame.cpp`

**Solution**: Manual UTF-16 to UTF-32 conversion with surrogate pair handling:

```cpp
#ifdef __MINGW32__
std::u32string result;
result.reserve(s.size());
for (size_t i = 0; i < s.size(); ++i)
{
    char32_t codepoint;
    wchar_t c = s[i];
    if (c >= 0xD800 && c <= 0xDBFF && i + 1 < s.size())
    {
        wchar_t low = s[i + 1];
        if (low >= 0xDC00 && low <= 0xDFFF)
        {
            codepoint = 0x10000 + ((c - 0xD800) << 10) + (low - 0xDC00);
            ++i;
        }
        else
            codepoint = c;
    }
    else
        codepoint = c;
    result.push_back(codepoint);
}
return result;
#else
// std::wstring_convert implementation...
#endif
```

### Fix 3: DirectComposition — Stub IDCompositionVisual3

**Problem**: MinGW headers don't include `IDCompositionVisual3` (requires
Windows 10 SDK features not present in MinGW).

**File**: `vstgui/lib/platform/win32/win32directcomposition.cpp`

**Solution**: Stub `SetOpacity` calls (DirectComposition is not implemented in
Wine anyway, so this is effectively a no-op):

```cpp
#ifdef __MINGW32__
// IDCompositionVisual3::SetOpacity not available in MinGW headers.
// DirectComposition is not implemented in Wine, so this is a no-op.
auto hr = S_OK;
#else
COM::Ptr<IDCompositionVisual3> visual3;
if (FAILED(visualSurface->visual->QueryInterface(visual3.adoptPtr())))
    return;
auto hr = visual3->SetOpacity(1.f);
#endif
```

Applied in two locations within the same file.

### Fix 4: DWrite Font — GetMatchingFonts_ variant

**Problem**: MinGW's DWrite headers declare `GetMatchingFonts_` (with trailing
underscore) instead of `GetMatchingFonts`. This is a MinGW naming convention
to avoid conflicts.

**File**: `vstgui/lib/platform/win32/direct2d/d2dfont.cpp`

```cpp
#ifdef __MINGW32__
if (SUCCEEDED(fs->GetMatchingFonts_(name, fontWeight, fontStretch, fontStyle,
                                     matchingFonts.adoptPtr())))
#else
if (SUCCEEDED(fs->GetMatchingFonts(name, fontWeight, fontStretch, fontStyle,
                                    matchingFonts.adoptPtr())))
#endif
```

### Fix 5: Standalone CMakeLists.txt — Early return for MinGW

**Problem**: `vstgui_standalone` target has no MinGW support (platform detection
only handles MSVC/Linux/macOS). Build fails with empty source list.

**File**: `vstgui/standalone/CMakeLists.txt`

```cmake
# vstgui_standalone is not needed for VST3 plugins and has no MinGW support
if(MINGW)
    return()
endif()
```

### Fix 6: Toolchain file

**File**: `mingw-w64-toolchain.cmake`

```cmake
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)
set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libgcc -static-libstdc++")
add_definitions(-DUNICODE -D_UNICODE)
```

### Building the VSTGUI Fork

```bash
cd sources/vstgui
mkdir build-mingw && cd build-mingw
cmake .. -DCMAKE_TOOLCHAIN_FILE=../mingw-w64-toolchain.cmake \
         -DCMAKE_BUILD_TYPE=Release \
         -DVSTGUI_STANDALONE=OFF
cmake --build . -j$(nproc)
```

Output: `build-mingw/Release/libs/libvstgui.a`, `libvstgui_uidescription.a`

---

## Part 2: VST3 SDK MinGW Fixes

### Fix 1: aligned_alloc

**Problem**: MinGW's C++ runtime provides `_aligned_malloc` (like MSVC), not
`std::aligned_alloc` (POSIX). The SDK only checks for `_MSC_VER`.

**File**: `vst3sdk/public.sdk/source/vst/utility/alignedalloc.h`

**Solution**: Add `__MINGW32__` alongside `_MSC_VER` in three places:

```cpp
// Include
#if defined(_MSC_VER) || defined(__MINGW32__)
#include <malloc.h>
#endif

// Allocation
#if defined(_MSC_VER) || defined(__MINGW32__)
data = _aligned_malloc(numBytes, alignment);
#else
data = std::aligned_alloc(alignment, numBytes);
#endif

// Deallocation
#if defined(_MSC_VER) || defined(__MINGW32__)
_aligned_free(addr);
#else
std::free(addr);
#endif
```

### Fix 2: VSTGUI Integration via Symlink

The VST3 SDK ships its own copy of VSTGUI at `vst3sdk/vstgui4/`. To use our
patched fork instead:

```bash
cd sources/vst3sdk
mv vstgui4 vstgui4.orig
ln -s ../vstgui vstgui4
```

This makes the SDK build our MinGW-patched VSTGUI transparently.

---

## Part 3: VST3 Plugin CMake Configuration

Building a VST3 plugin with MinGW requires several non-obvious CMake settings.
Each one was discovered through build failures.

### Complete CMakeLists.txt with annotations

```cmake
cmake_minimum_required(VERSION 3.25.0)
project(my-plugin VERSION 1.0.0.1)

set(vst3sdk_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../vst3sdk")

# --- MinGW Cross-Compilation Workarounds ---

# Avoid Windows admin scripts that fail on Linux
set(SMTG_PLUGIN_TARGET_USER_PATH "${CMAKE_BINARY_DIR}/VST3" CACHE PATH "" FORCE)

# Plugin symlink creation uses Windows APIs — disable for cross-compilation
set(SMTG_CREATE_PLUGIN_LINK OFF CACHE BOOL "" FORCE)

# Disable SDK examples (not needed, speeds up build)
set(SMTG_ENABLE_VST3_HOSTING_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES OFF CACHE BOOL "" FORCE)

# Validator requires WinMain (GUI app), fails with MinGW console linking
set(SMTG_RUN_VST_VALIDATOR OFF CACHE BOOL "" FORCE)

# Enable VSTGUI support (uses symlinked fork)
set(SMTG_ENABLE_VSTGUI_SUPPORT ON CACHE BOOL "" FORCE)
set(SMTG_CREATE_BUNDLE_FOR_WINDOWS ON CACHE BOOL "" FORCE)

# vstgui_standalone has no MinGW support — disable
set(VSTGUI_STANDALONE OFF CACHE BOOL "" FORCE)

# TinyJS (UI scripting) fails: missing <cstdint> for int64_t in MinGW
# Not needed if using programmatic UI (no .uidesc XML)
set(VSTGUI_UISCRIPTING OFF CACHE BOOL "" FORCE)

# --- Include SDK ---
add_subdirectory(${vst3sdk_SOURCE_DIR} ${CMAKE_BINARY_DIR}/vst3sdk)

# --- Plugin Sources ---
set(plugin_sources
    source/processor.h source/processor.cpp
    source/controller.h source/controller.cpp
    source/editor.h source/editor.cpp
    source/pluginentry.cpp
    source/plugincids.h source/pluginparamids.h source/version.h
)

# CRITICAL: Must set this before smtg_add_vst3plugin, otherwise dllmain.cpp
# is not added to the target (variable is local to SDK's CMakeLists.txt)
set(public_sdk_SOURCE_DIR "${vst3sdk_SOURCE_DIR}/public.sdk")

smtg_add_vst3plugin(my-plugin ${plugin_sources})
target_compile_features(my-plugin PUBLIC cxx_std_17)

# --- Linking ---
# IMPORTANT: MinGW uses single-pass linking. vstgui_support depends on symbols
# from sdk (getPlatformModuleHandle, ModuleInitializer). Because sdk is linked
# first, its symbols are discarded before vstgui_support needs them.
# Solution: list sdk TWICE — before and after vstgui_support.
target_link_libraries(my-plugin
    PRIVATE
        sdk
        vstgui_support
        sdk                  # repeated for MinGW link order
        vstgui
        vstgui_uidescription
)

# System libraries required by VSTGUI and D2D1 rendering
if(SMTG_WIN)
    target_link_libraries(my-plugin
        PRIVATE
            d2d1 dwrite d3d11 dxgi   # D2D1/D3D rendering
            dcomp dwmapi              # DirectComposition, DWM
            shlwapi imm32 opengl32    # Shell, IME, OpenGL (VSTGUI deps)
    )
endif()
```

### Build commands

```bash
mkdir build-mingw && cd build-mingw
cmake .. -DCMAKE_TOOLCHAIN_FILE=../mingw-w64-toolchain.cmake \
         -DCMAKE_BUILD_TYPE=Release
cmake --build . --target my-plugin -j$(nproc)
```

The `--target` flag skips the Validator build (which would fail anyway).

---

## Part 4: Wine-Specific Fixes for VSTGUI

These fixes are needed at runtime when VSTGUI runs under Wine.

### Fix 1: Disable DirectComposition (10-second timeout)

Wine does not implement DirectComposition. VSTGUI attempts to initialize it
and waits ~10 seconds before falling back. Disable it explicitly:

```cpp
// In standalone app: after VSTGUI::init()
if (auto win32Factory = getPlatformFactory().asWin32Factory())
    win32Factory->disableDirectComposition();

// In VST3 plugin editor: at the start of open()
bool PLUGIN_API Editor::open(void* parent, const PlatformType& platformType)
{
    if (auto win32Factory = getPlatformFactory().asWin32Factory())
        win32Factory->disableDirectComposition();
    // ... create CFrame, add views ...
}
```

### Fix 2: WM_ERASEBKGND — Prevent garbage on initial paint

VSTGUI creates a child window inside the parent HWND. On Wine, the parent
window's background can show through as garbage before the child paints.

```cpp
case WM_ERASEBKGND:
    return 1;  // Suppress default erase — VSTGUI child covers the area
```

**Note**: This fix applies to standalone apps with a custom WndProc. VST3
plugins running inside a DAW typically don't need this (the DAW handles it).

### Fix 3: Deferred repaint with WM_TIMER

Race condition: the window appears before VSTGUI's child window has finished
its initial paint. A 50ms timer ensures the child window gets invalidated
after the message loop starts:

```cpp
// In WndProc:
case WM_TIMER:
    if (wParam == 1)
    {
        KillTimer(hwnd, 1);
        if (gFrame)
            gFrame->invalid();
        EnumChildWindows(hwnd, [](HWND child, LPARAM) -> BOOL {
            InvalidateRect(child, nullptr, TRUE);
            return TRUE;
        }, 0);
        InvalidateRect(hwnd, nullptr, TRUE);
        return 0;
    }
    break;

// After ShowWindow:
SetTimer(hwnd, 1, 50, nullptr);
```

### Fix 4: Kill Wine zombie processes

Wine can leave zombie processes from previous runs. These hold locks that
cause timing issues with window creation and painting.

```bash
# Kill all lingering Wine processes before testing
killall -9 wineserver wine winedevice.exe plugplay.exe 2>/dev/null
```

---

## Part 5: Standalone App vs. VST3 Plugin

### Standalone App

- Links against **pre-built** VSTGUI static libraries (`.a` files)
- Has its own `WinMain`, `WndProc`, message loop
- Creates `CFrame` directly in `WinMain`
- Needs Wine-specific fixes in WndProc (WM_ERASEBKGND, WM_TIMER)
- Output: `.exe`
- Build: link against `libvstgui.a` + `libvstgui_uidescription.a`

### VST3 Plugin

- VSTGUI built **from source** via VST3 SDK's `add_subdirectory()`
- Uses `smtg_add_vst3plugin()` macro (adds dllmain.cpp automatically)
- Editor inherits from `VSTGUIEditor` + `IControlListener`
- Creates `CFrame` in `Editor::open()`
- Wine fixes go in `Editor::open()` (DirectComposition) — no WndProc access
- Output: `.vst3` (PE32+ DLL in bundle directory structure)
- Build: link against `sdk`, `vstgui_support`, etc.

### Shared patterns

Both use **programmatic UI** — no `.uidesc` XML, no PNG bitmap resources.
All controls are custom-drawn using `CDrawContext` (drawRect, drawEllipse,
drawLine, etc.). This avoids resource embedding complexity with MinGW.

**Important**: VSTGUI's built-in `CKnob` and `CSlider` require bitmap handles
in their constructors. For MinGW cross-compilation without resource files,
use custom-drawn `CControl` subclasses instead.

---

## Part 6: VST3 Bundle Structure and Deployment

### Bundle directory layout

```
"D2D1 Wine Test.vst3"/
  Contents/
    x86_64-win/
      d2d1-wine-test.vst3    # The PE32+ DLL
```

### Creating the bundle after build

```bash
mkdir -p "D2D1 Wine Test.vst3/Contents/x86_64-win"
cp build-mingw/VST3/d2d1-wine-test.vst3 \
   "D2D1 Wine Test.vst3/Contents/x86_64-win/"
```

### Deploying to Wine

```bash
# Copy bundle to VST3 scan path
cp -r "D2D1 Wine Test.vst3" \
   ~/.wine/drive_c/Program\ Files/Common\ Files/VST3/

# Launch DAW
wine ~/.wine/drive_c/Program\ Files/REAPER\ \(x64\)/reaper.exe
```

REAPER (and other VST3 hosts) automatically scans the VST3 directory on
startup and discovers the plugin.

---

## Part 7: Wine D2D1 Patches (Reference)

Wine's D2D1 implementation has several rendering bugs that affect VSTGUI
applications. We maintain 14 patches (v5.0-stable) that fix:

| # | Patch | Category |
|---|-------|----------|
| 1-4 | Arc implementation, font rendering, CDT triangulation, miter limit | Base fixes |
| 5-7 | Shader-based anti-aliasing, text AA interaction | Visual quality |
| 8-9 | CDT cycle detection, stroke outline join fix | Stability |
| 10-11 | CDT segment skip, collinear diagnostics rate-limiting | Robustness |
| 12 | Debug marker removal, FIXME rate-limiting | Performance |
| 13 | 512-vertex limit removal | Compatibility |
| 14 | Straight line outline anti-aliasing | Visual quality |

Patches are available in `patches/v5-full/` (against Wine 11.1) and
`patches/v5-full-11.0/` (rebased for Wine 11.0 stable).

For detailed technical documentation, see `docs/d2d1-open-issues-and-progress.md`.

---

## Part 8: Known Issues and Workarounds

### Build issues

| Issue | Cause | Workaround |
|-------|-------|------------|
| `int64_t does not name a type` in TinyJS | Missing `<cstdint>` in MinGW | Set `VSTGUI_UISCRIPTING OFF` |
| `WinMain` undefined in validator | Validator is a GUI app, MinGW links as console | Set `SMTG_RUN_VST_VALIDATOR OFF` |
| `getPlatformModuleHandle` undefined | MinGW single-pass linker, wrong library order | List `sdk` twice in link libraries |
| `dllmain.cpp` not compiled | `public_sdk_SOURCE_DIR` not set outside SDK scope | Set it manually before `smtg_add_vst3plugin()` |
| `std::aligned_alloc` not found | MinGW uses `_aligned_malloc` like MSVC | Patch `alignedalloc.h` |

### Runtime issues

| Issue | Cause | Workaround |
|-------|-------|------------|
| 10-second startup delay | DirectComposition not implemented in Wine | `disableDirectComposition()` |
| Garbage/artifacts on first paint | VSTGUI child window race condition | WM_ERASEBKGND + WM_TIMER |
| GUI sometimes doesn't appear | Zombie Wine processes from previous runs | Kill all Wine processes first |
| `libEGL warning: egl: failed to create dri2 screen` | Normal EGL/GPU init message | Harmless, ignore |
| DXVK overrides affect timing | `.wine` prefix has d3d11=native,builtin | Use clean prefix without DXVK |

### Design constraints

- **No bitmap resources**: CKnob/CSlider require bitmaps. Use custom-drawn
  CControl subclasses for knobs and sliders.
- **No .uidesc XML**: Resource embedding with MinGW is non-trivial. Use
  programmatic UI with `CFrame`, `CViewContainer`, and custom views.
- **Programmatic UI only**: All views created in code via `new CView(CRect(...))`
  and added with `frame->addView()`.

---

## Quick Reference: Complete Build from Scratch

```bash
# 1. Clone repositories
git clone https://github.com/giang17/vstgui.git
git clone --recursive https://github.com/steinbergmedia/vst3sdk.git
git clone https://github.com/giang17/vstgui-wine-test-plugin.git

# 2. Checkout MinGW branch of VSTGUI fork
cd vstgui && git checkout feature/mingw-cross-compilation && cd ..

# 3. Symlink VSTGUI fork into VST3 SDK
cd vst3sdk && mv vstgui4 vstgui4.orig && ln -s ../vstgui vstgui4 && cd ..

# 4. Apply aligned_alloc fix to VST3 SDK
# Edit vst3sdk/public.sdk/source/vst/utility/alignedalloc.h
# Change _MSC_VER checks to: defined(_MSC_VER) || defined(__MINGW32__)

# 5. Build plugin
cd vstgui-wine-test-plugin
mkdir build-mingw && cd build-mingw
cmake .. -DCMAKE_TOOLCHAIN_FILE=../mingw-w64-toolchain.cmake \
         -DCMAKE_BUILD_TYPE=Release
cmake --build . --target d2d1-wine-test -j$(nproc)

# 6. Create bundle and deploy
mkdir -p "D2D1 Wine Test.vst3/Contents/x86_64-win"
cp VST3/d2d1-wine-test.vst3 "D2D1 Wine Test.vst3/Contents/x86_64-win/"
cp -r "D2D1 Wine Test.vst3" \
   ~/.wine/drive_c/Program\ Files/Common\ Files/VST3/

# 7. Test
wine ~/.wine/drive_c/Program\ Files/REAPER\ \(x64\)/reaper.exe
```
