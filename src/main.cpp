#include <Ultralight/Ultralight.h>
#include <AppCore/Platform.h>
#include <iostream>
#include <string>
#include <fstream>
// #include "GPUDriverD3D11.h"
// #include "GPUContextD3D11.h"

using namespace ultralight;

typedef void (*DebugLogCallback)(const char *message);

class UnityLogger : public Logger
{
    DebugLogCallback g_DebugLogCallback;
    std::string logPath;

public:
    UnityLogger(DebugLogCallback callback)
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
            g_DebugLogCallback(error_msg.c_str());
        }

        g_DebugLogCallback(message.utf8().data());
    }

    ~UnityLogger()
    {
        g_DebugLogCallback = nullptr;
    }
};
class UnityViewListener : public ViewListener
{
    DebugLogCallback g_DebugLogCallback;

public:
    UnityViewListener(DebugLogCallback callback)
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

    ~UnityViewListener()
    {
        g_DebugLogCallback = nullptr;
    }
};

RefPtr<Renderer> renderer;
RefPtr<View> view;

UnityLogger *logger;
UnityViewListener *view_listener;

// GPUContextD3D11 *gpu_context;
// GPUDriverD3D11 *gpu_driver;

static int x, y;
static bool isInitialized;

extern "C"
{
    __declspec(dllexport) void ULInitialize(const char *path, const char *cache, DebugLogCallback callback)
    {
        if (!isInitialized)
        {
            logger = new UnityLogger(callback);
            view_listener = new UnityViewListener(callback);
            // gpu_context = new GPUContextD3D11();
            // gpu_driver = new GPUDriverD3D11(gpu_context);

            Config config;
            config.cache_path = cache;
            config.user_stylesheet = "body { background: transparent; }";

            // Platform::instance().set_gpu_driver(gpu_driver);
            Platform::instance().set_config(config);
            Platform::instance().set_logger(logger);
            Platform::instance().set_font_loader(GetPlatformFontLoader());
            Platform::instance().set_file_system(GetPlatformFileSystem(path));
        }
        else
        {
            logger->UpdateCallback(callback);
            view_listener->UpdateCallback(callback);
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
            // view_config.is_accelerated = true;

            view = renderer->CreateView(width, height, view_config, nullptr);
            view->set_view_listener(view_listener);

            view->LoadURL(url);
        }
        else view->Reload();
    }

    __declspec(dllexport) void ULUpdate()
    {
        renderer->Update();
        // gpu_driver->BeginSynchronize();

        renderer->Render();
        // gpu_driver->EndSynchronize();

        // if (gpu_driver->HasCommandsPending())
        // {
        //     gpu_driver->DrawCommandList();
        // }
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

    // __declspec(dllexport) void *ULGet2DTexture()
    // {
    //     RenderTarget render_target = view->render_target();
    //     return nullptr;
    // }

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

    __declspec(dllexport) void ULDispose()
    {
        x = 0;
        y = 0;

        if (!isInitialized)
            isInitialized = true;
    }
}
