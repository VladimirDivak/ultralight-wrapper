#include <Ultralight/Ultralight.h>
#include <JavaScriptCore/JavaScript.h>
#include <AppCore/Platform.h>
#include <iostream>
#include <string>
#include <fstream>
#include <vector>

using namespace ultralight;

typedef void (*DebugLogCallback)(const char *message);
typedef void (*JavaScriptEvent)(const char *eventName, const char *args);

class CustomLogger : public Logger
{
    DebugLogCallback g_DebugLogCallback;
    std::string logPath;

public:
    CustomLogger(DebugLogCallback callback)
    {
        logPath = "D:\\ultralight.log";
        g_DebugLogCallback = callback;

        std::ofstream log_file(logPath.c_str(), std::ios_base::app);
        if (!log_file.is_open())
        {
            std::ofstream create_file(logPath.c_str());
        }
    }

    void UpdateCallback(DebugLogCallback callback)
    {
        g_DebugLogCallback = callback;
    }

    virtual void LogMessage(LogLevel log_level, const String &message) override
    {
        std::ofstream log_file(logPath, std::ios_base::app);
        if (log_file.is_open())
        {
            std::string utf8_message = message.utf8().data();
            log_file << utf8_message << std::endl;
            log_file.close();
        }
        else
        {
            std::string error_msg = "Failed to open log file: " + logPath;
            if (g_DebugLogCallback)
                g_DebugLogCallback(error_msg.c_str());
        }

        if (g_DebugLogCallback)
            g_DebugLogCallback(message.utf8().data());
    }

    ~CustomLogger()
    {
        g_DebugLogCallback = nullptr;
    }
};

class CustomViewListener : public ViewListener
{
    DebugLogCallback g_DebugLogCallback;

public:
    CustomViewListener(DebugLogCallback callback)
    {
        g_DebugLogCallback = callback;
    }

    void UpdateCallback(DebugLogCallback callback)
    {
        g_DebugLogCallback = callback;
    }

    virtual void OnAddConsoleMessage(ultralight::View *caller, const ultralight::ConsoleMessage &message) override
    {
        if (g_DebugLogCallback)
        {
            std::string log_message = "[Browser Console]: " + std::string(message.message().utf8().data());
            g_DebugLogCallback(log_message.c_str());
        }
    }

    ~CustomViewListener()
    {
        g_DebugLogCallback = nullptr;
    }
};

class CustomLoadListener : public LoadListener
{
    static JavaScriptEvent javascript_event;

    static JSValueRef OnSendMessageToEngineWithArgs(JSContextRef ctx, JSObjectRef function,
                                            JSObjectRef thisObject, size_t argumentCount,
                                            const JSValueRef arguments[], JSValueRef *exception)
    {
        if (!javascript_event)
        {
            if (exception)
            {
                *exception = JSValueMakeString(ctx, JSStringCreateWithUTF8CString("JavaScript event callback is not set."));
            }
            return JSValueMakeNull(ctx);
        }

        if (argumentCount < 1 || argumentCount > 2)
        {
            if (exception)
            {
                *exception = JSValueMakeString(ctx, JSStringCreateWithUTF8CString("Invalid number of arguments."));
            }
            return JSValueMakeNull(ctx);
        }

        JSStringRef jsString = JSValueToStringCopy(ctx, arguments[0], exception);
        size_t bufferSize = JSStringGetMaximumUTF8CStringSize(jsString);
        std::vector<char> message(bufferSize);
        JSStringGetUTF8CString(jsString, message.data(), bufferSize);
        JSStringRelease(jsString);

        std::string args;
        if (argumentCount == 2)
        {
            JSStringRef jsArgString = JSValueToStringCopy(ctx, arguments[1], exception);
            size_t argBufferSize = JSStringGetMaximumUTF8CStringSize(jsArgString);
            std::vector<char> argBuffer(argBufferSize);
            JSStringGetUTF8CString(jsArgString, argBuffer.data(), argBufferSize);
            args = std::string(argBuffer.data());
            JSStringRelease(jsArgString);
        }

        javascript_event(message.data(), args.empty() ? nullptr : args.c_str());
        
        return JSValueMakeNull(ctx);
    }

public:
    CustomLoadListener(JavaScriptEvent callback)
    {
        javascript_event = callback;
    }

    void UpdateCallback(JavaScriptEvent callback)
    {
        javascript_event = callback;
    }

    virtual void OnDOMReady(View *caller, uint64_t frame_id, bool is_main_frame, const String &url) override
    {
        auto scoped_context = caller->LockJSContext();
        JSContextRef ctx = (*scoped_context);
        JSStringRef name = JSStringCreateWithUTF8CString("sendMessageToEngine");
        JSObjectRef func = JSObjectMakeFunctionWithCallback(ctx, name, OnSendMessageToEngineWithArgs);
        JSObjectRef globalObj = JSContextGetGlobalObject(ctx);
        JSObjectSetProperty(ctx, globalObj, name, func, 0, nullptr);
        JSStringRelease(name);
    }

    ~CustomLoadListener()
    {
        javascript_event = nullptr;
    }
};

JavaScriptEvent CustomLoadListener::javascript_event = nullptr;

RefPtr<Renderer> renderer;
RefPtr<View> view;

CustomLogger *logger;
CustomViewListener *view_listener;
CustomLoadListener *load_listener;

static int x, y;
static bool isInitialized;

extern "C"
{
    __declspec(dllexport) void ULInitialize(const char *path, const char *cache, DebugLogCallback callback, JavaScriptEvent js_event_callback)
    {
        if (!isInitialized)
        {
            logger = new CustomLogger(callback);
            view_listener = new CustomViewListener(callback);
            load_listener = new CustomLoadListener(js_event_callback);

            Config config;
            config.cache_path = cache;
            config.user_stylesheet = "body { background: transparent; }";

            Platform::instance().set_config(config);
            Platform::instance().set_logger(logger);
            Platform::instance().set_font_loader(GetPlatformFontLoader());
            Platform::instance().set_file_system(GetPlatformFileSystem(path));
        }
        else
        {
            logger->UpdateCallback(callback);
            view_listener->UpdateCallback(callback);
            load_listener->UpdateCallback(js_event_callback);
        }
    }

    __declspec(dllexport) void ULCreate(uint32_t width, uint32_t height, const char *url)
    {
        if (!isInitialized)
        {
            renderer = Renderer::Create();

            ViewConfig view_config;
            view_config.is_transparent = true;
            view_config.enable_javascript = true;

            view = renderer->CreateView(width, height, view_config, nullptr);
            view->set_view_listener(view_listener);
            view->set_load_listener(load_listener);
        }

        view->LoadURL(url);
    }

    __declspec(dllexport) void ULUpdate()
    {
        renderer->Update();
        renderer->Render();
    }

    __declspec(dllexport) void ULRefresh()
    {
        renderer->RefreshDisplay(0);
    }

    __declspec(dllexport) void ULResize(int width, int height)
    {
        view->Resize(width, height);
    }

    __declspec(dllexport) void ULSetMousePosition(int mouseX, int mouseY)
    {
        x = mouseX;
        y = mouseY;

        MouseEvent mouse_event;
        mouse_event.type = MouseEvent::kType_MouseMoved;
        mouse_event.x = x;
        mouse_event.y = y;
        mouse_event.button = MouseEvent::kButton_None;

        view->FireMouseEvent(mouse_event);
    }

    __declspec(dllexport) void ULSetScroll(int power)
    {
        ScrollEvent scroll_event;
        scroll_event.type = ScrollEvent::kType_ScrollByPixel;
        scroll_event.delta_x = 0;
        scroll_event.delta_y = power;

        view->FireScrollEvent(scroll_event);
    }

    __declspec(dllexport) void ULInvokeJavaScript(const char *data)
    {
        view->EvaluateScript(data);
    }

    __declspec(dllexport) void ULSetMouseDown()
    {
        MouseEvent mouse_event;
        mouse_event.type = MouseEvent::kType_MouseDown;
        mouse_event.button = MouseEvent::kButton_Left;
        mouse_event.x = x;
        mouse_event.y = y;

        view->FireMouseEvent(mouse_event);
    }

    __declspec(dllexport) void ULSetMouseUp()
    {
        MouseEvent mouse_event;
        mouse_event.type = MouseEvent::kType_MouseUp;
        mouse_event.button = MouseEvent::kButton_Left;
        mouse_event.x = x;
        mouse_event.y = y;

        view->FireMouseEvent(mouse_event);
    }

    __declspec(dllexport) void *ULCopyBitmapToTexture()
    {
        if (view->surface() == nullptr)
        {
            logger->LogMessage(LogLevel::Error, "surface is not found!");
            return nullptr;
        }

        BitmapSurface *bitmap_surface = static_cast<BitmapSurface *>(view->surface());
        RefPtr<Bitmap> bitmap = bitmap_surface->bitmap();

        if (!view->surface()->dirty_bounds().IsEmpty())
        {
            bitmap->LockPixels();
            void *pixels = bitmap->raw_pixels();
            bitmap->UnlockPixels();

            view->surface()->ClearDirtyBounds();
            return pixels;
        }
        else
        {
            return nullptr;
        }
    }

    __declspec(dllexport) void ULReset()
    {
        logger->UpdateCallback(nullptr);
        view_listener->UpdateCallback(nullptr);
        load_listener->UpdateCallback(nullptr);

        x = 0;
        y = 0;

        if (!isInitialized)
            isInitialized = true;
    }

    __declspec(dllexport) void ULDispose()
    {
        view.reset();
        view = nullptr;

        renderer.reset();
        renderer = nullptr;

        // delete logger;
        // logger = nullptr;

        // delete view_listener;
        // view_listener = nullptr;

        // delete load_listener;
        // load_listener = nullptr;
    }
}