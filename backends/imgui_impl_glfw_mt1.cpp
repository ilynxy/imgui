// dear imgui: Platform Backend for GLFW with separated event/render queue
// Currently this needs to be used along with a OpenGL3 Renderer
// (Info: GLFW is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan graphics context creation, etc.)
// Requires: GLFW 3.4+

#include "imgui.h"
#ifndef IMGUI_DISABLE
#include "imgui_impl_glfw_mt1.h"

#if defined(__has_include)
#if !__has_include(<X11/Xlib.h>) || !__has_include(<X11/extensions/Xrandr.h>)
#define IMGUI_IMPL_GLFW_DISABLE_X11
#endif
#if !__has_include(<wayland-client.h>)
#define IMGUI_IMPL_GLFW_DISABLE_WAYLAND
#endif
#endif

// GLFW
#if !defined(IMGUI_IMPL_GLFW_DISABLE_X11) && (defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__DragonFly__))
#define GLFW_HAS_X11        1
#else
#define GLFW_HAS_X11        0
#endif
#if !defined(IMGUI_IMPL_GLFW_DISABLE_WAYLAND) && (defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__DragonFly__))
#define GLFW_HAS_WAYLAND    1
#else
#define GLFW_HAS_WAYLAND    0
#endif
#include <GLFW/glfw3.h>
#ifdef _WIN32
#undef APIENTRY
#ifndef GLFW_EXPOSE_NATIVE_WIN32    // for glfwGetWin32Window()
#define GLFW_EXPOSE_NATIVE_WIN32
#endif
#include <GLFW/glfw3native.h>
#elif defined(__APPLE__)
#ifndef GLFW_EXPOSE_NATIVE_COCOA    // for glfwGetCocoaWindow()
#define GLFW_EXPOSE_NATIVE_COCOA
#endif
#include <GLFW/glfw3native.h>
#elif GLFW_HAS_X11
#ifndef GLFW_EXPOSE_NATIVE_X11      // for glfwGetX11Display(), glfwGetX11Window() on Freedesktop (Linux, BSD, etc.)
#define GLFW_EXPOSE_NATIVE_X11
#include <X11/Xatom.h>
#include <dlfcn.h>              // for dlopen()
#endif
#include <GLFW/glfw3native.h>
#undef Status                   // X11 headers are leaking this.
#endif

#ifndef _WIN32
#include <unistd.h>             // for usleep()
#endif
#include <stdio.h>              // for snprintf()

// We gather version tests as define in order to easily see which features are version-dependent.
#define GLFW_VERSION_COMBINED           (GLFW_VERSION_MAJOR * 1000 + GLFW_VERSION_MINOR * 100 + GLFW_VERSION_REVISION)
// We gather version tests as define in order to easily see which features are version-dependent.
#define GLFW_VERSION_COMBINED           (GLFW_VERSION_MAJOR * 1000 + GLFW_VERSION_MINOR * 100 + GLFW_VERSION_REVISION)
#define GLFW_HAS_CREATECURSOR           (GLFW_VERSION_COMBINED >= 3100) // 3.1+ glfwCreateCursor()
#define GLFW_HAS_WINDOW_TOPMOST         (GLFW_VERSION_COMBINED >= 3200) // 3.2+ GLFW_FLOATING
#define GLFW_HAS_WINDOW_HOVERED         (GLFW_VERSION_COMBINED >= 3300) // 3.3+ GLFW_HOVERED
#define GLFW_HAS_WINDOW_ALPHA           (GLFW_VERSION_COMBINED >= 3300) // 3.3+ glfwSetWindowOpacity
#define GLFW_HAS_PER_MONITOR_DPI        (GLFW_VERSION_COMBINED >= 3300) // 3.3+ glfwGetMonitorContentScale
#if defined(__EMSCRIPTEN__) || defined(__SWITCH__)                      // no Vulkan support in GLFW for Emscripten or homebrew Nintendo Switch
#define GLFW_HAS_VULKAN                 (0)
#else
#define GLFW_HAS_VULKAN                 (GLFW_VERSION_COMBINED >= 3200) // 3.2+ glfwCreateWindowSurface
#endif
#define GLFW_HAS_FOCUS_WINDOW           (GLFW_VERSION_COMBINED >= 3200) // 3.2+ glfwFocusWindow
#define GLFW_HAS_FOCUS_ON_SHOW          (GLFW_VERSION_COMBINED >= 3300) // 3.3+ GLFW_FOCUS_ON_SHOW
#define GLFW_HAS_MONITOR_WORK_AREA      (GLFW_VERSION_COMBINED >= 3300) // 3.3+ glfwGetMonitorWorkarea
#define GLFW_HAS_OSX_WINDOW_POS_FIX     (GLFW_VERSION_COMBINED >= 3301) // 3.3.1+ Fixed: Resizing window repositions it on MacOS #1553
#ifdef GLFW_RESIZE_NESW_CURSOR          // Let's be nice to people who pulled GLFW between 2019-04-16 (3.4 define) and 2019-11-29 (cursors defines) // FIXME: Remove when GLFW 3.4 is released?
#define GLFW_HAS_NEW_CURSORS            (GLFW_VERSION_COMBINED >= 3400) // 3.4+ GLFW_RESIZE_ALL_CURSOR, GLFW_RESIZE_NESW_CURSOR, GLFW_RESIZE_NWSE_CURSOR, GLFW_NOT_ALLOWED_CURSOR
#else
#define GLFW_HAS_NEW_CURSORS            (0)
#endif
#ifdef GLFW_MOUSE_PASSTHROUGH           // Let's be nice to people who pulled GLFW between 2019-04-16 (3.4 define) and 2020-07-17 (passthrough)
#define GLFW_HAS_MOUSE_PASSTHROUGH      (GLFW_VERSION_COMBINED >= 3400) // 3.4+ GLFW_MOUSE_PASSTHROUGH
#else
#define GLFW_HAS_MOUSE_PASSTHROUGH      (0)
#endif
#define GLFW_HAS_GAMEPAD_API            (GLFW_VERSION_COMBINED >= 3300) // 3.3+ glfwGetGamepadState() new api
#define GLFW_HAS_GETKEYNAME             (GLFW_VERSION_COMBINED >= 3200) // 3.2+ glfwGetKeyName()
#define GLFW_HAS_GETERROR               (GLFW_VERSION_COMBINED >= 3300) // 3.3+ glfwGetError()
#define GLFW_HAS_GETPLATFORM            (GLFW_VERSION_COMBINED >= 3400) // 3.4+ glfwGetPlatform()

// Map GLFWWindow* to ImGuiContext*.
// - Would be simpler if we could use glfwSetWindowUserPointer()/glfwGetWindowUserPointer(), but this is a single and shared resource.
// - Would be simpler if we could use e.g. std::map<> as well. But we don't.
// - This is not particularly optimized as we expect size to be small and queries to be rare.
struct ImGui_ImplGlfw_WindowToContext { GLFWwindow* Window; ImGuiContext* Context; };
static ImVector<ImGui_ImplGlfw_WindowToContext> g_ContextMap;
static void ImGui_ImplGlfw_ContextMap_Add(GLFWwindow* window, ImGuiContext* ctx) { g_ContextMap.push_back(ImGui_ImplGlfw_WindowToContext{ window, ctx }); }
static void ImGui_ImplGlfw_ContextMap_Remove(GLFWwindow* window)                 { for (ImGui_ImplGlfw_WindowToContext& entry : g_ContextMap) if (entry.Window == window) { g_ContextMap.erase_unsorted(&entry); if (g_ContextMap.empty()) g_ContextMap.clear(); return; } }
static ImGuiContext* ImGui_ImplGlfw_ContextMap_Get(GLFWwindow* window)           { for (ImGui_ImplGlfw_WindowToContext& entry : g_ContextMap) if (entry.Window == window) return entry.Context; return nullptr; }

enum GlfwClientApi
{
    GlfwClientApi_OpenGL,
    GlfwClientApi_Vulkan,
    GlfwClientApi_Unknown,  // Anything else fits here.
};

#if GLFW_HAS_X11
typedef Atom (*PFN_XInternAtom)(Display*, const char* ,Bool);
typedef int  (*PFN_XChangeProperty)(Display*, Window, Atom, Atom, int, int, const unsigned char*, int);
typedef int  (*PFN_XChangeWindowAttributes)(Display*, Window, unsigned long, XSetWindowAttributes*);
typedef int  (*PFN_XFlush)(Display*);
#endif


struct ImGui_ImplGlfw_MT_Data
{
    ImGuiContext*           Context;
    GLFWwindow*             Window;
    GlfwClientApi           ClientApi;
    char                    BackendPlatformName[32];
    double                  Time;
    bool                    IsWayland;
#if GLFW_HAS_CREATECURSOR
    GLFWcursor*             MouseCursors[ImGuiMouseCursor_COUNT];
    GLFWcursor*             LastMouseCursor;
#endif
    ImVec2                  LastValidMousePos;
    bool                    MouseIgnoreButtonUp;

    int                     window_width;
    int                     window_height;
    int                     framebuffer_width;
    int                     framebuffer_height;

///    GLFWwindow*             MouseWindow;
///    bool                    MouseIgnoreButtonUpWaitForFocusLoss;
///    GLFWwindow*             KeyOwnerWindows[GLFW_KEY_LAST];
///    bool                    InstalledCallbacks;
///    bool                    CallbacksChainForAllWindows;
///#ifdef EMSCRIPTEN_USE_EMBEDDED_GLFW3
///    const char*             CanvasSelector;
///#endif
///
///    // Chain GLFW callbacks: our callbacks will call the user's previously installed callbacks, if any.
///    GLFWwindowfocusfun      PrevUserCallbackWindowFocus;
///    GLFWcursorposfun        PrevUserCallbackCursorPos;
///    GLFWcursorenterfun      PrevUserCallbackCursorEnter;
///    GLFWmousebuttonfun      PrevUserCallbackMousebutton;
///    GLFWscrollfun           PrevUserCallbackScroll;
///    GLFWkeyfun              PrevUserCallbackKey;
///    GLFWcharfun             PrevUserCallbackChar;
///    GLFWmonitorfun          PrevUserCallbackMonitor;
///#ifdef _WIN32
///    WNDPROC                 PrevWndProc;
///#endif
///
#if GLFW_HAS_X11
    // Module and function pointers loaded at initialization to avoid linking statically with X11.
    void*                       X11Module;
    PFN_XInternAtom             XInternAtom;
    PFN_XChangeProperty         XChangeProperty;
    PFN_XChangeWindowAttributes XChangeWindowAttributes;
    PFN_XFlush                  XFlush;
#endif

    ImGui_ImplGlfw_MT_Data()   { memset((void*)this, 0, sizeof(*this)); }
};

namespace ImGui { extern ImGuiIO& GetIO(ImGuiContext*); }
static ImGui_ImplGlfw_MT_Data* ImGui_ImplGlfw_GetBackendData()
{
    // Get data for current context
    return ImGui::GetCurrentContext() ? (ImGui_ImplGlfw_MT_Data*)ImGui::GetIO().BackendPlatformUserData : nullptr;
}

static ImGui_ImplGlfw_MT_Data* ImGui_ImplGlfw_GetBackendData(GLFWwindow* window)
{
    // Get data for a given GLFW window, regardless of current context (since GLFW events are sent together)
    ImGuiContext* ctx = ImGui_ImplGlfw_ContextMap_Get(window);
    return (ImGui_ImplGlfw_MT_Data*)ImGui::GetIO(ctx).BackendPlatformUserData;
}


static bool ImGui_ImplGlfw_IsWayland()
{
#if !GLFW_HAS_WAYLAND
    return false;
#elif GLFW_HAS_GETPLATFORM
    return glfwGetPlatform() == GLFW_PLATFORM_WAYLAND;
#else
    const char* version = glfwGetVersionString();
    if (strstr(version, "Wayland") == nullptr) // e.g. Ubuntu 22.04 ships with GLFW 3.3.6 compiled without Wayland
        return false;
#ifdef GLFW_EXPOSE_NATIVE_X11
    if (glfwGetX11Display() != nullptr)
        return false;
#endif
    return true;
#endif
}

static float ImGui_ImplGlfw_GetContentScaleForMonitor(GLFWmonitor* monitor)
{
#if GLFW_HAS_WAYLAND
    if (ImGui_ImplGlfw_IsWayland()) // We can't access our bd->IsWayland cache for a monitor.
        return 1.0f;
#endif
#if GLFW_HAS_PER_MONITOR_DPI && !(defined(__APPLE__) || defined(__EMSCRIPTEN__) || defined(__ANDROID__))
    float x_scale, y_scale;
    glfwGetMonitorContentScale(monitor, &x_scale, &y_scale);
    return x_scale;
#else
    IM_UNUSED(monitor);
    return 1.0f;
#endif
}

static void ImGui_ImplGlfw_UpdateMonitors()
{
    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
    int monitors_count = 0;
    GLFWmonitor** glfw_monitors = glfwGetMonitors(&monitors_count);

    bool updated_monitors = false;
    for (int n = 0; n < monitors_count; n++)
    {
        ImGuiPlatformMonitor monitor;
        int x, y;
        glfwGetMonitorPos(glfw_monitors[n], &x, &y);
        const GLFWvidmode* vid_mode = glfwGetVideoMode(glfw_monitors[n]);
        if (vid_mode == nullptr)
            continue; // Failed to get Video mode (e.g. Emscripten does not support this function)
        if (vid_mode->width <= 0 || vid_mode->height <= 0)
            continue; // Failed to query suitable monitor info (#9195)
        monitor.MainPos = monitor.WorkPos = ImVec2((float)x, (float)y);
        monitor.MainSize = monitor.WorkSize = ImVec2((float)vid_mode->width, (float)vid_mode->height);
#if GLFW_HAS_MONITOR_WORK_AREA
        int w, h;
        glfwGetMonitorWorkarea(glfw_monitors[n], &x, &y, &w, &h);
        if (w > 0 && h > 0) // Workaround a small GLFW issue reporting zero on monitor changes: https://github.com/glfw/glfw/pull/1761
        {
            monitor.WorkPos = ImVec2((float)x, (float)y);
            monitor.WorkSize = ImVec2((float)w, (float)h);
        }
#endif
        float scale = ImGui_ImplGlfw_GetContentScaleForMonitor(glfw_monitors[n]);
        if (scale == 0.0f)
            continue; // Some accessibility applications are declaring virtual monitors with a DPI of 0 (#7902)
        monitor.DpiScale = scale;
        monitor.PlatformHandle = (void*)glfw_monitors[n]; // [...] GLFW doc states: "guaranteed to be valid only until the monitor configuration changes"

        // Preserve existing monitor list until a valid one is added.
        // Happens on macOS sleeping (#5683) and seemingly occasionally on Windows (#9195)
        if (!updated_monitors)
            platform_io.Monitors.resize(0);
        updated_monitors = true;
        platform_io.Monitors.push_back(monitor);
    }
}

static void ImGui_ImplGlfw_InitWindowSizeAndFramebufferScale(ImGui_ImplGlfw_MT_Data* bd)
{
  glfwGetWindowSize(bd->Window, &bd->window_width, &bd->window_height);
  glfwGetFramebufferSize(bd->Window, &bd->framebuffer_width, &bd->framebuffer_height);
}

static void ImGui_ImplGlfw_GetWindowSizeAndFramebufferScale(ImGui_ImplGlfw_MT_Data* bd, ImVec2* out_size, ImVec2* out_framebuffer_scale)
{
  int w = bd->window_width;
  int h = bd->window_height;
  int display_w = bd->framebuffer_width;
  int display_h = bd->framebuffer_height;

  float fb_scale_x = (w > 0) ? (float)display_w / (float)w : 1.0f;
  float fb_scale_y = (h > 0) ? (float)display_h / (float)h : 1.0f;
#if GLFW_HAS_WAYLAND
  //ImGui_ImplGlfw_Data* bd = ImGui_ImplGlfw_GetBackendData(window);
  if (!bd->IsWayland)
      fb_scale_x = fb_scale_y = 1.0f;
#endif
  if (out_size != nullptr)
      *out_size = ImVec2((float)w, (float)h);
  if (out_framebuffer_scale != nullptr)
      *out_framebuffer_scale = ImVec2(fb_scale_x, fb_scale_y);
}

static bool ImGui_ImplGlfw_Init(GLFWwindow* window, bool /*install_callbacks*/, GlfwClientApi client_api)
{
    ImGuiIO& io = ImGui::GetIO();
    IMGUI_CHECKVERSION();
    IM_ASSERT(io.BackendPlatformUserData == nullptr && "Already initialized a platform backend!");
    //printf("GLFW_VERSION: %d.%d.%d (%d)", GLFW_VERSION_MAJOR, GLFW_VERSION_MINOR, GLFW_VERSION_REVISION, GLFW_VERSION_COMBINED);

    // Setup backend capabilities flags
    ImGui_ImplGlfw_MT_Data* bd = IM_NEW(ImGui_ImplGlfw_MT_Data)();
    snprintf(bd->BackendPlatformName, sizeof(bd->BackendPlatformName), "imgui_impl_glfw_mt (%d)", GLFW_VERSION_COMBINED);
    io.BackendPlatformUserData = (void*)bd;
    io.BackendPlatformName = bd->BackendPlatformName;
#if GLFW_HAS_CREATECURSOR
    io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;         // We can honor GetMouseCursor() values (optional)
#endif
    io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;          // We can honor io.WantSetMousePos requests (optional, rarely used)

    bool has_viewports = false;
#ifndef __EMSCRIPTEN__
    has_viewports = false; // !!!
#if GLFW_HAS_GETPLATFORM
    if (glfwGetPlatform() == GLFW_PLATFORM_WAYLAND)
        has_viewports = false;
#endif
    if (has_viewports)
        io.BackendFlags |= ImGuiBackendFlags_PlatformHasViewports;  // We can create multi-viewports on the Platform side (optional)
#endif
#if GLFW_HAS_MOUSE_PASSTHROUGH || GLFW_HAS_WINDOW_HOVERED
    io.BackendFlags |= ImGuiBackendFlags_HasMouseHoveredViewport; // We can call io.AddMouseViewportEvent() with correct data (optional)
#endif

    bd->Context = ImGui::GetCurrentContext();
    bd->Window = window;
    bd->Time = 0.0;
    bd->IsWayland = ImGui_ImplGlfw_IsWayland();
    ImGui_ImplGlfw_ContextMap_Add(window, bd->Context);

    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
#if GLFW_VERSION_COMBINED < 3300
    platform_io.Platform_SetClipboardTextFn = [](ImGuiContext*, const char* text) { glfwSetClipboardString(ImGui_ImplGlfw_GetBackendData()->Window, text); };
    platform_io.Platform_GetClipboardTextFn = [](ImGuiContext*) { return glfwGetClipboardString(ImGui_ImplGlfw_GetBackendData()->Window); };
#else
    platform_io.Platform_SetClipboardTextFn = [](ImGuiContext*, const char* text) { glfwSetClipboardString(nullptr, text); };
    platform_io.Platform_GetClipboardTextFn = [](ImGuiContext*) { return glfwGetClipboardString(nullptr); };
#endif

#ifdef __EMSCRIPTEN__
    platform_io.Platform_OpenInShellFn = [](ImGuiContext*, const char* url) { ImGui_ImplGlfw_EmscriptenOpenURL(url); return true; };
#endif

    // Create mouse cursors
    // (By design, on X11 cursors are user configurable and some cursors may be missing. When a cursor doesn't exist,
    // GLFW will emit an error which will often be printed by the app, so we temporarily disable error reporting.
    // Missing cursors will return nullptr and our _UpdateMouseCursor() function will use the Arrow cursor instead.)
#if GLFW_HAS_CREATECURSOR
    GLFWerrorfun prev_error_callback = glfwSetErrorCallback(nullptr);
    bd->MouseCursors[ImGuiMouseCursor_Arrow] = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
    bd->MouseCursors[ImGuiMouseCursor_TextInput] = glfwCreateStandardCursor(GLFW_IBEAM_CURSOR);
    bd->MouseCursors[ImGuiMouseCursor_ResizeNS] = glfwCreateStandardCursor(GLFW_VRESIZE_CURSOR);
    bd->MouseCursors[ImGuiMouseCursor_ResizeEW] = glfwCreateStandardCursor(GLFW_HRESIZE_CURSOR);
    bd->MouseCursors[ImGuiMouseCursor_Hand] = glfwCreateStandardCursor(GLFW_HAND_CURSOR);
#if GLFW_HAS_NEW_CURSORS
    bd->MouseCursors[ImGuiMouseCursor_ResizeAll] = glfwCreateStandardCursor(GLFW_RESIZE_ALL_CURSOR);
    bd->MouseCursors[ImGuiMouseCursor_ResizeNESW] = glfwCreateStandardCursor(GLFW_RESIZE_NESW_CURSOR);
    bd->MouseCursors[ImGuiMouseCursor_ResizeNWSE] = glfwCreateStandardCursor(GLFW_RESIZE_NWSE_CURSOR);
    bd->MouseCursors[ImGuiMouseCursor_NotAllowed] = glfwCreateStandardCursor(GLFW_NOT_ALLOWED_CURSOR);
#else
    bd->MouseCursors[ImGuiMouseCursor_ResizeAll] = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
    bd->MouseCursors[ImGuiMouseCursor_ResizeNESW] = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
    bd->MouseCursors[ImGuiMouseCursor_ResizeNWSE] = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
    bd->MouseCursors[ImGuiMouseCursor_NotAllowed] = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
#endif
    glfwSetErrorCallback(prev_error_callback);
#endif
#if GLFW_HAS_GETERROR && !defined(__EMSCRIPTEN__) // Eat errors (see #5908)
    (void)glfwGetError(nullptr);
#endif

    // Chain GLFW callbacks: our callbacks will call the user's previously installed callbacks, if any.
///    if (install_callbacks)
///        ImGui_ImplGlfw_InstallCallbacks(window);

    // Update monitor a first time during init
    // (note: monitor callback are broken in GLFW 3.2 and earlier, see github.com/glfw/glfw/issues/784)
    ImGui_ImplGlfw_UpdateMonitors();
///    glfwSetMonitorCallback(ImGui_ImplGlfw_MonitorCallback);

    ImGui_ImplGlfw_InitWindowSizeAndFramebufferScale(bd);
    ImGui_ImplGlfw_GetWindowSizeAndFramebufferScale(
          bd, &io.DisplaySize, &io.DisplayFramebufferScale);

    // Set platform dependent data in viewport
    ImGuiViewport* main_viewport = ImGui::GetMainViewport();
    main_viewport->PlatformHandle = (void*)bd->Window;
#ifdef _WIN32
    main_viewport->PlatformHandleRaw = glfwGetWin32Window(bd->Window);
#elif defined(__APPLE__)
    main_viewport->PlatformHandleRaw = (void*)glfwGetCocoaWindow(bd->Window);
#else
    IM_UNUSED(main_viewport);
#endif
///    if (has_viewports)
///        ImGui_ImplGlfw_InitMultiViewportSupport();

///    // Windows: register a WndProc hook so we can intercept some messages.
///#ifdef _WIN32
///    HWND hwnd = (HWND)main_viewport->PlatformHandleRaw;
///    ::SetPropA(hwnd, "IMGUI_BACKEND_DATA", bd);
///    bd->PrevWndProc = (WNDPROC)::GetWindowLongPtrW(hwnd, GWLP_WNDPROC);
///    IM_ASSERT(bd->PrevWndProc != nullptr);
///    ::SetWindowLongPtrW((HWND)main_viewport->PlatformHandleRaw, GWLP_WNDPROC, (LONG_PTR)ImGui_ImplGlfw_WndProc);
///#endif

#if GLFW_HAS_X11
    if (!bd->IsWayland)
    {
        // Load X11 module dynamically. Copied from the way that GLFW does it in x11_init.c
#if defined(__CYGWIN__)
        const char* x11_module_path = "libX11-6.so";
#elif defined(__OpenBSD__) || defined(__NetBSD__)
        const char* x11_module_path = "libX11.so";
#else
        const char* x11_module_path = "libX11.so.6";
#endif
        bd->X11Module = dlopen(x11_module_path, RTLD_LAZY | RTLD_LOCAL);
        bd->XInternAtom = (PFN_XInternAtom)dlsym(bd->X11Module, "XInternAtom");
        bd->XChangeProperty = (PFN_XChangeProperty)dlsym(bd->X11Module, "XChangeProperty");
        bd->XChangeWindowAttributes = (PFN_XChangeWindowAttributes)dlsym(bd->X11Module, "XChangeWindowAttributes");
        bd->XFlush = (PFN_XFlush)dlsym(bd->X11Module, "XFlush");
        IM_ASSERT(bd->XInternAtom != nullptr && bd->XChangeProperty != nullptr && bd->XChangeWindowAttributes != nullptr && bd->XFlush != nullptr);
    }
#endif

    // Emscripten: the same application can run on various platforms, so we detect the Apple platform at runtime
    // to override io.ConfigMacOSXBehaviors from its default (which is always false in Emscripten).
#ifdef __EMSCRIPTEN__
#if EMSCRIPTEN_USE_PORT_CONTRIB_GLFW3 >= 34020240817
    if (emscripten::glfw3::IsRuntimePlatformApple())
    {
        io.ConfigMacOSXBehaviors = true;

        // Due to how the browser (poorly) handles the Meta Key, this line essentially disables repeats when used.
        // This means that Meta + V only registers a single key-press, even if the keys are held.
        // This is a compromise for dealing with this issue in ImGui since ImGui implements key repeat itself.
        // See https://github.com/pongasoft/emscripten-glfw/blob/v3.4.0.20240817/docs/Usage.md#the-problem-of-the-super-key
        emscripten::glfw3::SetSuperPlusKeyTimeouts(10, 10);
    }
#endif
#endif

    bd->ClientApi = client_api;
    return true;
}

bool ImGui_ImplGlfw_MT_InitForOpenGL(GLFWwindow* window, bool install_callbacks)
{
    return ImGui_ImplGlfw_Init(window, install_callbacks, GlfwClientApi_OpenGL);
}

///bool ImGui_ImplGlfw_InitForVulkan(GLFWwindow* window, bool install_callbacks)
///{
///    return ImGui_ImplGlfw_Init(window, install_callbacks, GlfwClientApi_Vulkan);
///}
///
///bool ImGui_ImplGlfw_InitForOther(GLFWwindow* window, bool install_callbacks)
///{
///    return ImGui_ImplGlfw_Init(window, install_callbacks, GlfwClientApi_Unknown);
///}

void ImGui_ImplGlfw_MT_Shutdown()
{
    ImGui_ImplGlfw_MT_Data* bd = ImGui_ImplGlfw_GetBackendData();
    IM_ASSERT(bd != nullptr && "No platform backend to shutdown, or already shutdown?");

    ImGuiIO& io = ImGui::GetIO();
    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();

///    ImGui_ImplGlfw_ShutdownMultiViewportSupport();
///    if (bd->InstalledCallbacks)
///        ImGui_ImplGlfw_RestoreCallbacks(bd->Window);
#ifdef EMSCRIPTEN_USE_EMBEDDED_GLFW3
    if (bd->CanvasSelector)
        emscripten_set_wheel_callback(bd->CanvasSelector, nullptr, false, nullptr);
#endif
#if GLFW_HAS_CREATECURSOR
    for (ImGuiMouseCursor cursor_n = 0; cursor_n < ImGuiMouseCursor_COUNT; cursor_n++)
        glfwDestroyCursor(bd->MouseCursors[cursor_n]);
#endif
    // Windows: restore our WndProc hook
///#ifdef _WIN32
///    ImGuiViewport* main_viewport = ImGui::GetMainViewport();
///    ::SetPropA((HWND)main_viewport->PlatformHandleRaw, "IMGUI_BACKEND_DATA", nullptr);
///    ::SetWindowLongPtrW((HWND)main_viewport->PlatformHandleRaw, GWLP_WNDPROC, (LONG_PTR)bd->PrevWndProc);
///    bd->PrevWndProc = nullptr;
///#endif

#if GLFW_HAS_X11
    if (bd->X11Module != nullptr)
        dlclose(bd->X11Module);
#endif

    io.BackendPlatformName = nullptr;
    io.BackendPlatformUserData = nullptr;
    io.BackendFlags &= ~(ImGuiBackendFlags_HasMouseCursors | ImGuiBackendFlags_HasSetMousePos | ImGuiBackendFlags_HasGamepad | ImGuiBackendFlags_PlatformHasViewports | ImGuiBackendFlags_HasMouseHoveredViewport);
    platform_io.ClearPlatformHandlers();
    ImGui_ImplGlfw_ContextMap_Remove(bd->Window);
    IM_DELETE(bd);
}

void ImGui_ImplGlfw_MT_NewFrame()
{
    ImGuiIO& io = ImGui::GetIO();
    ImGui_ImplGlfw_MT_Data* bd = ImGui_ImplGlfw_GetBackendData();
    IM_ASSERT(bd != nullptr && "Context or backend not initialized! Did you call ImGui_ImplGlfw_InitForXXX()?");

    // Setup main viewport size (every frame to accommodate for window resizing)
    ImGui_ImplGlfw_GetWindowSizeAndFramebufferScale(bd, &io.DisplaySize, &io.DisplayFramebufferScale);
///    ImGui_ImplGlfw_UpdateMonitors();

    // Setup time step
    // (Accept glfwGetTime() not returning a monotonically increasing value. Seems to happens on disconnecting peripherals and probably on VMs and Emscripten, see #6491, #6189, #6114, #3644)
    double current_time = glfwGetTime();
    if (current_time <= bd->Time)
        current_time = bd->Time + 0.00001f;
    io.DeltaTime = bd->Time > 0.0 ? (float)(current_time - bd->Time) : (float)(1.0f / 60.0f);
    bd->Time = current_time;

///    bd->MouseIgnoreButtonUp = false;
///    ImGui_ImplGlfw_UpdateMouseData();
///    ImGui_ImplGlfw_UpdateMouseCursor();

    // Update game controllers (if enabled and available)
///    ImGui_ImplGlfw_UpdateGamepads();
}

// X11 does not include current pressed/released modifier key in 'mods' flags submitted by GLFW
// See https://github.com/ocornut/imgui/issues/6034 and https://github.com/glfw/glfw/issues/1630
static void ImGui_ImplGlfw_MT_UpdateKeyModifiers(ImGuiIO& io, GLFWwindow* window)
{
    io.AddKeyEvent(ImGuiMod_Ctrl,  (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) || (glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS));
    io.AddKeyEvent(ImGuiMod_Shift, (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT)   == GLFW_PRESS) || (glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT)   == GLFW_PRESS));
    io.AddKeyEvent(ImGuiMod_Alt,   (glfwGetKey(window, GLFW_KEY_LEFT_ALT)     == GLFW_PRESS) || (glfwGetKey(window, GLFW_KEY_RIGHT_ALT)     == GLFW_PRESS));
    io.AddKeyEvent(ImGuiMod_Super, (glfwGetKey(window, GLFW_KEY_LEFT_SUPER)   == GLFW_PRESS) || (glfwGetKey(window, GLFW_KEY_RIGHT_SUPER)   == GLFW_PRESS));
}

void ImGui_ImplGlfw_MT_MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    ImGui_ImplGlfw_MT_Data* bd = ImGui_ImplGlfw_GetBackendData(window);
//    if (bd->PrevUserCallbackMousebutton != nullptr && ImGui_ImplGlfw_ShouldChainCallback(bd, window))
//        bd->PrevUserCallbackMousebutton(window, button, action, mods);

    // Workaround for Linux: ignore mouse up events which are following an focus loss following a viewport creation
    if (bd->MouseIgnoreButtonUp && action == GLFW_RELEASE)
        return;

    ImGuiIO& io = ImGui::GetIO(bd->Context);
    ImGui_ImplGlfw_MT_UpdateKeyModifiers(io, window);
    if (button >= 0 && button < ImGuiMouseButton_COUNT)
        io.AddMouseButtonEvent(button, action == GLFW_PRESS);
}

void ImGui_ImplGlfw_MT_CursorPosCallback(GLFWwindow* window, double x, double y)
{
    ImGui_ImplGlfw_MT_Data* bd = ImGui_ImplGlfw_GetBackendData(window);
///    if (bd->PrevUserCallbackCursorPos != nullptr && ImGui_ImplGlfw_ShouldChainCallback(bd, window))
///        bd->PrevUserCallbackCursorPos(window, x, y);

    ImGuiIO& io = ImGui::GetIO(bd->Context);
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        int window_x, window_y;
        glfwGetWindowPos(window, &window_x, &window_y);
        x += window_x;
        y += window_y;
    }
    io.AddMousePosEvent((float)x, (float)y);
    bd->LastValidMousePos = ImVec2((float)x, (float)y);
}

void ImGui_ImplGlfw_MT_ScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
  ImGui_ImplGlfw_MT_Data* bd = ImGui_ImplGlfw_GetBackendData(window);
///  if (bd->PrevUserCallbackScroll != nullptr && ImGui_ImplGlfw_ShouldChainCallback(bd, window))
///      bd->PrevUserCallbackScroll(window, xoffset, yoffset);

#ifdef EMSCRIPTEN_USE_EMBEDDED_GLFW3
  // Ignore GLFW events: will be processed in ImGui_ImplEmscripten_WheelCallback().
  return;
#endif

  ImGuiIO& io = ImGui::GetIO(bd->Context);
  io.AddMouseWheelEvent((float)xoffset, (float)yoffset);
}

void ImGui_ImplGlfw_MT_WindowSizeCallback(GLFWwindow* window, int width, int height)
{
  ImGui_ImplGlfw_MT_Data* bd = ImGui_ImplGlfw_GetBackendData(window);
  bd->window_width = width;
  bd->window_height = height;
}

void ImGui_ImplGlfw_MT_FramebufferSizeCallback(GLFWwindow* window, int width, int height)
{
  ImGui_ImplGlfw_MT_Data* bd = ImGui_ImplGlfw_GetBackendData(window);
  bd->framebuffer_width = width;
  bd->framebuffer_height = height;
}

void ImGui_ImplGlfw_MT_WindowContentScaleCallback(GLFWwindow* /*window*/, float xscale, float yscale)
{
  auto vp = ImGui::GetMainViewport();
  vp->DpiScale = (xscale + yscale) / 2;
}

#endif // #ifndef IMGUI_DISABLE
