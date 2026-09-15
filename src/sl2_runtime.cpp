#include "sl2_runtime.h"

#include <sl.h>
#include <sl_dlss_g.h>
#include <sl_pcl.h>
#include <sl_reflex.h>

#include <array>
#include <string>
#include <print>

namespace
{
HMODULE g_module{};

std::wstring moduleDirectory() noexcept
{
    HMODULE self{};
    if (!GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&moduleDirectory),
            &self))
    {
        return {};
    }

    std::array<wchar_t, 32768> path{};
    const DWORD length = GetModuleFileNameW(self, path.data(), static_cast<DWORD>(path.size()));
    if (length == 0 || length == path.size())
    {
        return {};
    }

    std::wstring directory(path.data(), length);
    const auto separator = directory.find_last_of(L"\\/");
    if (separator == std::wstring::npos)
    {
        return {};
    }
    directory.resize(separator + 1);
    return directory;
}

std::wstring executableDirectory() noexcept
{
    HMODULE self{};
    if (!GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(GetModuleHandleW(nullptr)),
            &self))
    {
        return {};
    }

    std::array<wchar_t, 32768> path{};
    const DWORD length = GetModuleFileNameW(self, path.data(), static_cast<DWORD>(path.size()));
    if (length == 0 || length == path.size())
    {
        return {};
    }

    std::wstring directory(path.data(), length);
    const auto separator = directory.find_last_of(L"\\/");
    if (separator == std::wstring::npos)
    {
        return {};
    }
    directory.resize(separator + 1);
    return directory;
}

bool hasRequiredCoreSurface() noexcept
{
    constexpr std::array names{
        "slInit",
        "slShutdown",
        "slIsFeatureSupported",
        "slSetFeatureLoaded",
        "slSetTagForFrame",
        "slSetConstants",
        "slEvaluateFeature",
        "slGetFeatureFunction",
        "slGetNewFrameToken",
        "slSetD3DDevice",
    };
    for (const char* name : names)
    {
        if (GetProcAddress(g_module, name) == nullptr)
        {
            std::println("[STK] Module {} is missing `{}` function!", reinterpret_cast<void*>(g_module), name);
            return false;
        }
    }
    return true;
}

static_assert(sl::kFeatureDLSS_G == 1000);
static_assert(sl::kFeatureReflex == 3);
static_assert(sl::kFeaturePCL == 4);

decltype(&CreateDXGIFactory) _slCreateDXGIFactory;
decltype(&D3D12CreateDevice) _slD3D12CreateDevice;
PFun_slSetD3DDevice* _slSetD3DDevice;
PFun_slSetConstants* _slSetConstants;
PFun_slGetNewFrameToken* _slGetNewFrameToken;
PFun_slSetTagForFrame* _slSetTagForFrame;
PFun_slSetFeatureLoaded* _slSetFeatureLoaded;

bool loadFunctions() noexcept
{
    using namespace bmi::sl2;

    _slCreateDXGIFactory = reinterpret_cast<decltype(&::CreateDXGIFactory)>(procedure("CreateDXGIFactory"));
    if (!_slCreateDXGIFactory) return false;
    _slD3D12CreateDevice = reinterpret_cast<decltype(&::D3D12CreateDevice)>(procedure("D3D12CreateDevice"));
    if (!_slD3D12CreateDevice) return false;
    _slSetD3DDevice = reinterpret_cast<PFun_slSetD3DDevice*>(procedure("slSetD3DDevice"));
    if (!_slSetD3DDevice) return false;
    _slSetConstants = reinterpret_cast<PFun_slSetConstants*>(procedure("slSetConstants"));
    if (!_slSetConstants) return false;
    _slGetNewFrameToken = reinterpret_cast<PFun_slGetNewFrameToken*>(procedure("slGetNewFrameToken"));
    if (!_slGetNewFrameToken) return false;
    _slSetTagForFrame = reinterpret_cast<PFun_slSetTagForFrame*>(procedure("slSetTagForFrame"));
    if (!_slSetTagForFrame) return false;
    _slSetFeatureLoaded = reinterpret_cast<PFun_slSetFeatureLoaded*>(procedure("slSetFeatureLoaded"));
    if (!_slSetFeatureLoaded) return false;

    return true;
}

#if !defined(NDEBUG)
static std::string_view log_type_to_str(sl::LogType type)
{
    switch (type)
    {
        using enum sl::LogType;
        case eInfo: return "[I]";
        case eWarn: return "[W]";
        case eError: return "[E]";
        default: return "[UNK]";
    }
}

void log_callback(sl::LogType type, const char* msg)
{
    std::println("[STK] {} {}", log_type_to_str(type), msg);
}
#endif

PFun_slReflexGetState* _slReflexGetState;
PFun_slReflexSetOptions* _slReflexSetOptions;
PFun_slReflexSleep* _slReflexSleep;
PFun_slPCLSetMarker* _slPCLSetMarker;
PFun_slDLSSGSetOptions* _slDLSSGSetOptions;

bool loadPluginFunctions()
{
    using namespace bmi::sl2;

    auto slGetFeatureFunction = reinterpret_cast<PFun_slGetFeatureFunction*>(procedure("slGetFeatureFunction"));

    if(SL_FAILED(res, slGetFeatureFunction(sl::kFeatureReflex, "slReflexGetState", *reinterpret_cast<void**>(&_slReflexGetState))))
    {
        return false;
    }
    if(SL_FAILED(res, slGetFeatureFunction(sl::kFeatureReflex, "slReflexSetOptions", *reinterpret_cast<void**>(&_slReflexSetOptions))))
    {
        return false;
    }
    if(SL_FAILED(res, slGetFeatureFunction(sl::kFeatureReflex, "slReflexSleep", *reinterpret_cast<void**>(&_slReflexSleep))))
    {
        return false;
    }
    if(SL_FAILED(res, slGetFeatureFunction(sl::kFeaturePCL, "slPCLSetMarker", *reinterpret_cast<void**>(&_slPCLSetMarker))))
    {
        return false;
    }
    if(SL_FAILED(res, slGetFeatureFunction(sl::kFeatureDLSS_G, "slDLSSGSetOptions", *reinterpret_cast<void**>(&_slDLSSGSetOptions))))
    {
        // DLSS-G is a soft fail?
        // return false;
    }
    
    return true;
}
}

namespace bmi::sl2
{
bool load() noexcept
{
    if (g_module != nullptr)
    {
        return true;
    }

    // const std::wstring path = moduleDirectory() + L"STK_MOD/sl2.interposer.dll";
    const std::wstring path = L"STK_MOD/sl.interposer.dll";
    if (path.empty())
    {
        return false;
    }

    // TODO(mrsteyk): he was boiling opsec in a kettle
    // if(!sl::security::verifyEmbeddedSignature(PATH_TO_SL_IN_YOUR_BUILD + "/sl.interposer.dll"))

    // g_module = LoadLibraryExW(path.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    g_module = LoadLibraryW(path.c_str());
    if (g_module == nullptr || !hasRequiredCoreSurface())
    {
        std::println("[STK] Failed to load sl2 interposer! {:X}", GetLastError());
        unload();
        return false;
    }
    return loadFunctions();
}

void unload() noexcept
{
    if (g_module != nullptr)
    {
        FreeLibrary(g_module);
        g_module = nullptr;
    }
}

HMODULE module() noexcept
{
    return g_module;
}

FARPROC procedure(const char* name) noexcept
{
    return g_module != nullptr ? GetProcAddress(g_module, name) : nullptr;
}

bool init(int app_id) noexcept
{
    auto slInit = reinterpret_cast<PFun_slInit*>(bmi::sl2::procedure("slInit"));

    sl::Preferences prefs2{};
    prefs2.applicationId = app_id;
    prefs2.engine = sl::EngineType::eUnreal;
    prefs2.engineVersion = "4.26";
    prefs2.flags = sl::PreferenceFlags::eAllowOTA | sl::PreferenceFlags::eLoadDownloadedPlugins | sl::PreferenceFlags::eUseFrameBasedResourceTagging | sl::PreferenceFlags::eDisableCLStateTracking;
    // prefs2.logMessageCallback = prefs->logMessageCallback;
    prefs2.renderAPI = sl::RenderAPI::eD3D12;
    #if !defined(NDEBUG)
    prefs2.logLevel = sl::LogLevel::eDefault;
    prefs2.logMessageCallback = &log_callback;
    #else
    prefs2.logLevel = sl::LogLevel::eOff;
    #endif
    std::array<sl::Feature, 4> features{{
        sl::kFeatureReflex,
        sl::kFeaturePCL,
        // sl::kFeatureDLSS,
        sl::kFeatureDLSS_G,
    }};
    prefs2.numFeaturesToLoad = (uint32_t)features.size();
    prefs2.featuresToLoad = features.data();
    std::wstring plugins_path = executableDirectory() + L"STK_MOD";
    // DWORD tmp; WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), plugins_path.c_str(), plugins_path.size(), &tmp, nullptr);
    std::array<const wchar_t*, 1> plugin_paths{{
        plugins_path.c_str(),
    }};
    prefs2.numPathsToPlugins = (uint32_t)plugin_paths.size();
    prefs2.pathsToPlugins = plugin_paths.data();

    auto res = slInit(prefs2, sl::kSDKVersion);
    std::println("[STK] slInit = {}", (int)res);

    if (res != sl::Result::eOk)
    {
        return false;
    }

    return true;
}

HRESULT CreateDXGIFactory(REFIID riid, void** ppFactory)
{
    return _slCreateDXGIFactory(riid, ppFactory);
}

HRESULT D3D12CreateDevice(IUnknown *pAdapter, D3D_FEATURE_LEVEL MinimumFeatureLevel, REFIID riid, void **ppDevice)
{
    return _slD3D12CreateDevice(pAdapter, MinimumFeatureLevel, riid, ppDevice);
}

sl::Result SetD3DDevice(void* device)
{
    auto ret = _slSetD3DDevice(device);

    if (!loadPluginFunctions())
    {
        std::println("[STK] SetD3DDevice: failed to load plugin functions!!!");
    }

    return ret;
}

sl::Result GetNewFrameToken(sl::FrameToken*& token, const uint32_t* frameIndex)
{
    return _slGetNewFrameToken(token, frameIndex);
}
sl::Result SetConstants(const sl::Constants& values, const sl::FrameToken& frame, const sl::ViewportHandle& viewport)
{
    return _slSetConstants(values, frame, viewport);
}
sl::Result SetTagForFrame(const sl::FrameToken& frame, const sl::ViewportHandle& viewport, const sl::ResourceTag* tags, uint32_t numTags, sl::CommandBuffer* cmdBuffer)
{
    return _slSetTagForFrame(frame, viewport, tags, numTags, cmdBuffer);
}
sl::Result SetFeatureLoaded(sl::Feature feature, bool loaded)
{
    return _slSetFeatureLoaded(feature, loaded);
}

sl::Result ReflexGetState(sl::ReflexState& state)
{
    return _slReflexGetState(state);
}
sl::Result ReflexSetOptions(sl::ReflexOptions& options)
{
    return _slReflexSetOptions(options);
}
sl::Result ReflexSleep(const sl::FrameToken& frame)
{
    return _slReflexSleep(frame);
}
sl::Result PCLSetMarker(sl::PCLMarker marker, const sl::FrameToken& frame)
{
    return _slPCLSetMarker(marker, frame);
}

sl::Result DLSSGSetOptions(const sl::ViewportHandle& viewport, sl::DLSSGOptions& options)
{
    return _slDLSSGSetOptions(viewport, options);
}
}

