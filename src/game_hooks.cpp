#include "game_hooks.h"

#include "sl2_runtime.h"
#include "sl1_structs.h"

#include <safetyhook.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <print>

namespace
{
#if defined(NDEBUG)
constexpr bool STK_DEBUG = false;
#else
constexpr bool STK_DEBUG = true;
#endif
constexpr std::uintptr_t kExpectedImageBase = 0x140000000;
constexpr std::uintptr_t kSlInitPtrRVA = 0x5149EE0;
constexpr std::uintptr_t kFStreamLineDXFunctionsCtorRVA = 0xBD2750;

// BrightMemoryInfinite-Win64-Shipping.exe RVAs verified in the current IDA database.
// Hook implementations are intentionally omitted until the matching SL1.5 structures
// are ported. Installing pass-through hooks now proves address resolution and leaves
// an explicit replacement seam for each old core API call.
enum class Target : std::uintptr_t
{
    SlEvaluateFeature = 0x00BD1570,
    SlGetFeatureSettings = 0x00BD1580,
    SlIsFeatureSupported = 0x00BD1590,
    SlSetConstants = 0x00BD15A0,
    SlSetFeatureConstants = 0x00BD15B0,
    SlSetFeatureEnabled = 0x00BD15C0,
    SlSetTag = 0x00BD15D0,
};

struct HookSlot
{
    Target target;
    safetyhook::InlineHook hook;
};

// Jump function hooks
std::array<HookSlot, 7> g_hooks{{
    {Target::SlEvaluateFeature, {}},
    {Target::SlGetFeatureSettings, {}},
    {Target::SlIsFeatureSupported, {}},
    {Target::SlSetConstants, {}},
    {Target::SlSetFeatureConstants, {}},
    {Target::SlSetFeatureEnabled, {}},
    {Target::SlSetTag, {}},
}};

// Evaluate is a special function called inline.
safetyhook::InlineHook slInit_hook;

std::byte* executableBase() noexcept
{
    return reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
}

static sl::ViewportHandle viewport{};
static sl::FrameToken* frame_token{};
static uint32_t last_frame = std::numeric_limits<uint32_t>::max();

using SlInit = bool (*)(const sl1::Preferences* prefs, int app_id);
bool hookSlInit(const sl1::Preferences* prefs, int app_id)
{
    return bmi::sl2::init(app_id);
}

HRESULT D3D12CreateDevice_d(IUnknown *pAdapter, D3D_FEATURE_LEVEL MinimumFeatureLevel, REFIID riid, void **ppDevice)
{
    auto ret = bmi::sl2::D3D12CreateDevice(pAdapter, MinimumFeatureLevel, riid, ppDevice);
    if (SUCCEEDED(ret) && ppDevice)
    {
        bmi::sl2::SetD3DDevice(*ppDevice);
        if constexpr (STK_DEBUG) std::println("[STK] slSetD3DDevice called with {}", *ppDevice);
    }

    return ret;
}

safetyhook::InlineHook FStreamLineDXFunctions__FStreamLineDXFunctions{};
uintptr_t FStreamLineDXFunctions__FStreamLineDXFunctions_hk(uintptr_t a1, int a2)
{
    auto ret = FStreamLineDXFunctions__FStreamLineDXFunctions.call<uintptr_t>(a1, a2);

    *(void**)(a1 + 16) = bmi::sl2::procedure("CreateDXGIFactory");
    *(void**)(a1 + 24) = bmi::sl2::procedure("CreateDXGIFactory1");
    *(void**)(a1 + 32) = bmi::sl2::procedure("CreateDXGIFactory2");
    *(void**)(a1 + 40) = bmi::sl2::procedure("DXGIGetDebugInterface1");
    *(void**)(a1 + 48) = &D3D12CreateDevice_d;
    *(void**)(a1 + 56) = bmi::sl2::procedure("D3D11CreateDevice");

    if constexpr (STK_DEBUG) std::println("[STK] Replaced Interposer functions after FStreamLineDXFunctions::FStreamLineDXFunctions.");

    return ret;
}

using slEvaluateFeature = bool (*)(sl::CommandBuffer* cmdBuffer, sl::Feature feature, uint32_t frameIndex, uint32_t id);
bool hookSlEvaluateFeature(sl::CommandBuffer* cmdBuffer, sl::Feature feature, uint32_t frameIndex, uint32_t id)
{
    // if constexpr (STK_DEBUG) std::println("[STK] hookSlEvaluateFeature({}, {}, {}, {})", cmdBuffer, feature, frameIndex, id);

    if (feature == sl::kFeatureReflex)
    {
        // Shit like:
        // slEvaluateFeature(nullptr, sl::Feature::eFeatureReflex, myFrameIndex, sl::ReflexMarker::eReflexMarkerSimulationStart)

        if (last_frame != frameIndex)
        {
            if (SL_FAILED(res, bmi::sl2::GetNewFrameToken(frame_token, &frameIndex)))
            {
                if constexpr (STK_DEBUG) std::println("[STK] hookSlEvaluateFeature for Reflex failed to get new frame token {}!!!", (int)res);
            }
            last_frame = frameIndex;
        }

        enum ReflexMarker1
        {
            eReflexMarkerSimulationStart,
            eReflexMarkerSimulationEnd,
            eReflexMarkerRenderSubmitStart,
            eReflexMarkerRenderSubmitEnd,
            eReflexMarkerPresentStart,
            eReflexMarkerPresentEnd,
            eReflexMarkerInputSample,
            eReflexMarkerTriggerFlash,    
            eReflexMarkerPCLatencyPing,
            //! Special marker
            eReflexMarkerSleep = 0x1000,
        };

        switch (id)
        {
            case eReflexMarkerSimulationStart: {
                if (SL_FAILED(res, bmi::sl2::PCLSetMarker(sl::PCLMarker::eSimulationStart, *frame_token)))
                {
                    if constexpr (STK_DEBUG) std::println("[STK] hookSlEvaluateFeature for Reflex failed to eSimulationStart {}!!!", (int)res);
                    return false;
                }
            } break;
            case eReflexMarkerSimulationEnd: {
                if (SL_FAILED(res, bmi::sl2::PCLSetMarker(sl::PCLMarker::eSimulationEnd, *frame_token)))
                {
                    if constexpr (STK_DEBUG) std::println("[STK] hookSlEvaluateFeature for Reflex failed to eSimulationEnd {}!!!", (int)res);
                    return false;
                }
            } break;
            //
            case eReflexMarkerRenderSubmitStart: {
                if (SL_FAILED(res, bmi::sl2::PCLSetMarker(sl::PCLMarker::eRenderSubmitStart, *frame_token)))
                {
                    if constexpr (STK_DEBUG) std::println("[STK] hookSlEvaluateFeature for Reflex failed to eRenderSubmitStart {}!!!", (int)res);
                    return false;
                }
            } break;
            case eReflexMarkerRenderSubmitEnd: {
                if (SL_FAILED(res, bmi::sl2::PCLSetMarker(sl::PCLMarker::eRenderSubmitEnd, *frame_token)))
                {
                    if constexpr (STK_DEBUG) std::println("[STK] hookSlEvaluateFeature for Reflex failed to eRenderSubmitEnd {}!!!", (int)res);
                    return false;
                }
            } break;
            //
            case eReflexMarkerPresentStart: {
                if (SL_FAILED(res, bmi::sl2::PCLSetMarker(sl::PCLMarker::ePresentStart, *frame_token)))
                {
                    if constexpr (STK_DEBUG) std::println("[STK] hookSlEvaluateFeature for Reflex failed to ePresentStart {}!!!", (int)res);
                    return false;
                }
            } break;
            case eReflexMarkerPresentEnd: {
                if (SL_FAILED(res, bmi::sl2::PCLSetMarker(sl::PCLMarker::ePresentEnd, *frame_token)))
                {
                    if constexpr (STK_DEBUG) std::println("[STK] hookSlEvaluateFeature for Reflex failed to ePresentEnd {}!!!", (int)res);
                    return false;
                }
            } break;
            // !!!DEPRECATED!!!
            case eReflexMarkerInputSample: return true;
            //
            case eReflexMarkerTriggerFlash: {
                if (SL_FAILED(res, bmi::sl2::PCLSetMarker(sl::PCLMarker::eTriggerFlash, *frame_token)))
                {
                    if constexpr (STK_DEBUG) std::println("[STK] hookSlEvaluateFeature for Reflex failed to eTriggerFlash {}!!!", (int)res);
                    return false;
                }
            } break;
            case eReflexMarkerPCLatencyPing: {
                if (SL_FAILED(res, bmi::sl2::PCLSetMarker(sl::PCLMarker::ePCLatencyPing, *frame_token)))
                {
                    if constexpr (STK_DEBUG) std::println("[STK] hookSlEvaluateFeature for Reflex failed to ePCLatencyPing {}!!!", (int)res);
                    return false;
                }
            } break;
            //
            case eReflexMarkerSleep: {
                if (SL_FAILED(res, bmi::sl2::ReflexSleep(*frame_token)))
                {
                    if constexpr (STK_DEBUG) std::println("[STK] hookSlEvaluateFeature for Reflex failed to sleep {}!!!", (int)res);
                    return false;
                }
            } break;
            default: {
                    if constexpr (STK_DEBUG) std::println("[STK] hookSlEvaluateFeature for Reflex unknown marker {}!!!", id);
                return false;
            } break;
        }
    }
    else {
        if constexpr (STK_DEBUG) std::println("[STK] hookSlEvaluateFeature unknown feature {}!!!", feature);
    }

    return true;
}

using SlGetFeatureSettings = bool (*)(sl::Feature feature, const void* consts, void* settings);
bool hookSlGetFeatureSettings(sl::Feature feature, const void* consts, void* settings)
{
    // if constexpr (STK_DEBUG) std::println("[STK] hookSlGetFeatureSettings({}, {}, {})", feature, consts, settings);

    if (feature == sl::kFeatureReflex)
    {
        if (consts) if constexpr (STK_DEBUG) std::println("[STK] hookSlGetFeatureSettings for Reflex requested consts!!!");

        if (settings)
        {
            auto set = reinterpret_cast<sl1::ReflexSettings*>(settings);

            sl::ReflexState state{};
            if(SL_FAILED(res, bmi::sl2::ReflexGetState(state)))
            {
                // Handle error here, check the logs
                return false;
            }
            set->lowLatencyAvailable = state.lowLatencyAvailable;
            set->statsWindowMessage = state.statsWindowMessage;
            set->flashIndicatorDriverControlled = state.flashIndicatorDriverControlled;
            set->latencyReportAvailable = state.latencyReportAvailable;
            if (state.latencyReportAvailable)
            {
                for (int i = 0; i < sl::kReflexFrameReportCount; i++)
                {
                    set->frameReport[i].frameID = state.frameReport[i].frameID;
                    set->frameReport[i].inputSampleTime = state.frameReport[i].inputSampleTime;
                    set->frameReport[i].simStartTime = state.frameReport[i].simStartTime;
                    set->frameReport[i].simEndTime = state.frameReport[i].simEndTime;
                    set->frameReport[i].renderSubmitStartTime = state.frameReport[i].renderSubmitStartTime;
                    set->frameReport[i].renderSubmitEndTime = state.frameReport[i].renderSubmitEndTime;
                    set->frameReport[i].presentStartTime = state.frameReport[i].presentStartTime;
                    set->frameReport[i].presentEndTime = state.frameReport[i].presentEndTime;
                    set->frameReport[i].driverStartTime = state.frameReport[i].driverStartTime;
                    set->frameReport[i].driverEndTime = state.frameReport[i].driverEndTime;
                    set->frameReport[i].osRenderQueueStartTime = state.frameReport[i].osRenderQueueStartTime;
                    set->frameReport[i].osRenderQueueEndTime = state.frameReport[i].osRenderQueueEndTime;
                    set->frameReport[i].gpuRenderStartTime = state.frameReport[i].gpuRenderStartTime;
                    set->frameReport[i].gpuRenderEndTime = state.frameReport[i].gpuRenderEndTime;
                    set->frameReport[i].gpuActiveRenderTimeUs = state.frameReport[i].gpuActiveRenderTimeUs;
                    set->frameReport[i].gpuFrameTimeUs = state.frameReport[i].gpuFrameTimeUs;
                }
            }
        }

        return true;
    } else {
        if constexpr (STK_DEBUG) std::println("[STK] hookSlGetFeatureSettings unknown feature {}!!!", feature);
    }

    return true;
}

IDXGIFactory* factory;

using SlIsFeatureSupported = bool (*)(std::uint32_t feature, std::uint32_t* adapterMask);
bool hookSlIsFeatureSupported(std::uint32_t feature, std::uint32_t* adapterMask)
{
    auto slIsFeatureSupported = reinterpret_cast<PFun_slIsFeatureSupported*>(bmi::sl2::procedure("slIsFeatureSupported"));

    sl::AdapterInfo adapter{};
    uint32_t ret_mask = 0;
    // if (SUCCEEDED(CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&factory)))
    {
        IDXGIAdapter* dxgi_adapter;
        uint32_t i = 0;
        while (factory->EnumAdapters(i, &dxgi_adapter) != DXGI_ERROR_NOT_FOUND)
        {
            DXGI_ADAPTER_DESC desc{};
            if (SUCCEEDED(dxgi_adapter->GetDesc(&desc)))
            {
                sl::AdapterInfo adapterInfo{};
                adapterInfo.deviceLUID = (uint8_t*)&desc.AdapterLuid;
                adapterInfo.deviceLUIDSizeInBytes = sizeof(LUID);
                if (SL_FAILED(result, slIsFeatureSupported(feature, adapterInfo)))
                {
                    switch (result)
                    {
                        // case sl::Result::eErrorOSOutOfDate: std::println("[STK] hookSlIsFeatureSupported feature {} - adapter {} - eErrorOSOutOfDate", feature, i); break;
                        // case sl::Result::eErrorDriverOutOfDate: std::println("[STK] hookSlIsFeatureSupported feature {} - adapter {} - eErrorDriverOutOfDate", feature, i); break;
                        // case sl::Result::eErrorNoSupportedAdapterFound: std::println("[STK] hookSlIsFeatureSupported feature {} - adapter {} - eErrorNoSupportedAdapterFound", feature, i); break;
                        // case sl::Result::eErrorFeatureNotSupported: std::println("[STK] hookSlIsFeatureSupported feature {} - adapter {} - eErrorFeatureNotSupported", feature, i); break;
                        // default: std::println("[STK] hookSlIsFeatureSupported feature {} - adapter {} - UNKNOWN {}", feature, i, (int)result); break;
                    };
                }
                else
                {
                    // Feature is supported on this adapter!
                    ret_mask = 1 << i;
                    // std::println("[STK] hookSlIsFeatureSupported feature {} - adapter {} - OK", feature, i);
                }
            }
            i++;
            dxgi_adapter->Release();
        }
        // factory->Release();
    }
    // std::println("[STK] slIsFeatureSupported({}, ...) = {}", feature, (int)ret);
    bool is_ok = ret_mask != 0;
    if (adapterMask) *adapterMask = ret_mask;
    return is_ok;
}

using SlSetConstants = bool (*)(const sl1::Constants* values, uint32_t frameIndex, uint32_t id);
bool hookSlSetConstants(const sl1::Constants* values, uint32_t frameIndex, uint32_t id)
{
    // if constexpr (STK_DEBUG) std::println("[STK] hookSlSetConstants({}, {}, {})", values, frameIndex, id);
    if (!values) return false;

    sl::Constants common;

    common.cameraViewToClip = values->cameraViewToClip;
    common.clipToCameraView = values->clipToCameraView;
    common.clipToLensClip = values->clipToLensClip;
    common.clipToPrevClip = values->clipToPrevClip;
    common.prevClipToClip = values->prevClipToClip;
        
    common.jitterOffset = values->jitterOffset;
    common.mvecScale = values->mvecScale;
    common.cameraPinholeOffset = values->cameraPinholeOffset;
    common.cameraPos = values->cameraPos;
    common.cameraUp = values->cameraUp;
    common.cameraRight = values->cameraRight;
    common.cameraFwd = values->cameraFwd;
        
    common.cameraNear = values->cameraNear;
    common.cameraFar = values->cameraFar;
    common.cameraFOV = values->cameraFOV;
    common.cameraAspectRatio = values->cameraAspectRatio;
    common.motionVectorsInvalidValue = values->motionVectorsInvalidValue;

    common.depthInverted = values->depthInverted;
    common.cameraMotionIncluded = values->cameraMotionIncluded;
    common.motionVectors3D = values->motionVectors3D;
    common.reset = values->reset;
    common.orthographicProjection = values->orthographicProjection;
    common.motionVectorsDilated = values->motionVectorsDilated;
    common.motionVectorsJittered = values->motionVectorsJittered;

    if (last_frame != frameIndex)
    {
        if (SL_FAILED(res, bmi::sl2::GetNewFrameToken(frame_token, &frameIndex)))
        {
            if constexpr (STK_DEBUG) std::println("[STK] hookSlSetConstants failed to get new frame token {}!!!", (int)res);
        }
        last_frame = frameIndex;
    }

    if (SL_FAILED(res, bmi::sl2::SetConstants(common, *frame_token, viewport)))
    {
        if constexpr (STK_DEBUG) std::println("[STK] hookSlSetConstants failed to set common constants {}!!!", (int)res);
        return false;
    }

    return true;
}
using SlSetFeatureConstants  = bool (*)(sl::Feature feature, const void* consts, uint32_t frameIndex, uint32_t id);
bool hookSlSetFeatureConstants(sl::Feature feature, const void* consts, uint32_t frameIndex, uint32_t id)
{
    // if constexpr (STK_DEBUG) std::println("[STK] hookSlSetFeatureConstants({}, {}, {}, {})", feature, consts, frameIndex, id);

    if (feature == sl::kFeatureReflex)
    {
        if (consts)
        {
            auto c = reinterpret_cast<const sl1::ReflexConstants*>(consts);
            sl::ReflexOptions opts{};
            opts.mode = c->mode;
            opts.frameLimitUs = c->frameLimitUs;
            opts.useMarkersToOptimize = c->useMarkersToOptimize;
            opts.virtualKey = c->virtualKey;
            if (SL_FAILED(res, bmi::sl2::ReflexSetOptions(opts)))
            {
                if constexpr (STK_DEBUG) std::print("[STK] hookSlSetFeatureConstants for Reflex failed {}!!!", (int)res);
                return false;
            }
        }

        return true;
    }
    else if (feature == sl::kFeatureDLSS_G)
    {
        auto u32s = reinterpret_cast<const uint32_t*>(consts);
        // auto uptrs = reinterpret_cast<const uintptr_t*>(consts);

        sl::DLSSGOptions opts{};
        opts.mode = u32s[0] ? sl::DLSSGMode::eOn : sl::DLSSGMode::eOff;
        // This game is fucking weird, DLSSG plugin used allegedly supports multiple frame generation but internal plugin machinery hardcodes it to 1..1!!!
        opts.numFramesToGenerate = u32s[1];
        // Everything else in this 8 DWORD struct is set to 0.
        
        // Because I provide the hint buffer as a hud buffer it might work?
        // opts.enableUserInterfaceRecomposition = sl::Boolean::eTrue;

        // AAAAA
        opts.mode = u32s[0] ? sl::DLSSGMode::eDynamic : sl::DLSSGMode::eOff;
        // opts.numFramesToGenerate = 5;

        if (SL_FAILED(res, bmi::sl2::DLSSGSetOptions(viewport, opts)))
        {
            if constexpr (STK_DEBUG) std::println("[STK] hookSlSetFeatureConstants for DLSSG failed {}!!!", (int)res);
            return false;
        }

        return true;
    }
    else {
        if constexpr (STK_DEBUG) std::println("[STK] hookSlSetFeatureConstants unknown feature {}!!!", feature);
    }

    return true;
}

using SlSetFeatureEnabled = bool (*)(sl::Feature feature, bool enabled);
bool hookSlSetFeatureEnabled(sl::Feature feature, bool enabled)
{
    // if constexpr (STK_DEBUG) std::println("[STK] hookSlSetFeatureEnabled({}, {})", feature, enabled);

    if (feature == sl::kFeatureDLSS_G)
    {
        return bmi::sl2::SetFeatureLoaded(sl::kFeatureDLSS_G, enabled) == sl::Result::eOk;
    }

    return true;
}

using SlSetTag = bool (*)(const sl1::Resource *resource, sl::BufferType tag, uint32_t id, const sl::Extent* extent);
bool hookSlSetTag(const sl1::Resource *resource, sl::BufferType tag, uint32_t id, const sl::Extent* extent)
{
    // Valid values for tag are only 0, 1, 2, 23

    sl::Resource res{};
    res.type = resource->type;
    res.native = resource->native;
    res.state = resource->state;
    // Other things should be nullptr as this is D3D12 title

    sl::ResourceTag rt;
    rt.resource = &res;
    rt.extent = extent ? *extent : sl::Extent{};
    rt.lifecycle = sl::ResourceLifecycle::eValidUntilPresent;

    switch (tag)
    {
        case 0: {
            // Depth
            rt.type = sl::kBufferTypeDepth;
            if (SL_FAILED(ret, bmi::sl2::SetTagForFrame(*frame_token, viewport, &rt, 1, nullptr)))
            {
                if constexpr (STK_DEBUG) std::println("[STK] hookSlSetTag Depth SetTagForFrame failed {}!!!", (int)ret);
            }

            return true;
        } break;
        case 1: {
            // MVec
            rt.type = sl::kBufferTypeMotionVectors;
            if (SL_FAILED(ret, bmi::sl2::SetTagForFrame(*frame_token, viewport, &rt, 1, nullptr)))
            {
                if constexpr (STK_DEBUG) std::println("[STK] hookSlSetTag MVec SetTagForFrame failed {}!!!", (int)ret);
            }
        } break;
        case 2: {
            // HUDLessColor
            rt.type = sl::kBufferTypeHUDLessColor;
            if (SL_FAILED(ret, bmi::sl2::SetTagForFrame(*frame_token, viewport, &rt, 1, nullptr)))
            {
                if constexpr (STK_DEBUG) std::println("[STK] hookSlSetTag HUDLessColor SetTagForFrame failed {}!!!", (int)ret);
            }
        } break;
        case 23: {
            // I AM NOT SURE WHAT TO DO WITH THIS!!!
            // This is really old behaviour of asking for a hint!
            #if 0
            rt.type = sl::kBufferTypeUIColorAndAlpha;
            if (SL_FAILED(ret, bmi::sl2::SetTagForFrame(*frame_token, viewport, &rt, 1, nullptr)))
            {
                if constexpr (STK_DEBUG) std::println("[STK] hookSlSetTag UIHint SetTagForFrame failed {}!!!", (int)ret);
            }
            #endif
        } break;
        default: {
            if constexpr (STK_DEBUG) std::println("[STK] hookSlSetTag unknown tag {}!!!", tag);
            return false;
        } break;
    }

    return true;
}

bool installOne(HookSlot& slot, void* destination) noexcept
{
    const auto address = executableBase() + static_cast<std::uintptr_t>(slot.target);
    auto result = safetyhook::InlineHook::create(address, destination);
    if (!result)
    {
        if constexpr (STK_DEBUG) std::println("[STK] Failed to create hook with {}", (int)result.error().type);
        return false;
    }
    slot.hook = std::move(*result);
    return true;
}
}

namespace bmi::hooks
{
bool install() noexcept
{
    // Free original interposer library.
    FreeLibrary(GetModuleHandleW(L"sl.interposer.dll"));
    if (!bmi::sl2::load())
    {
        if constexpr (STK_DEBUG) std::println("[STK] Hooks Install precondition fail!");
        return false;
    }

    bmi::sl2::CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&factory);

    // slInit_hook = safetyhook::create_inline(*reinterpret_cast<void**>(executableBase() + kSlInitPtrRVA), reinterpret_cast<void*>(&hookSlInit));
    *reinterpret_cast<void**>(executableBase() + kSlInitPtrRVA) = &hookSlInit;
    FStreamLineDXFunctions__FStreamLineDXFunctions = safetyhook::create_inline(reinterpret_cast<void*>(executableBase() + kFStreamLineDXFunctionsCtorRVA), reinterpret_cast<void*>(&FStreamLineDXFunctions__FStreamLineDXFunctions_hk));

    bool ok = true;
    ok = ok && installOne(g_hooks[0], reinterpret_cast<void*>(&hookSlEvaluateFeature));
    ok = ok && installOne(g_hooks[1], reinterpret_cast<void*>(&hookSlGetFeatureSettings));
    ok = ok && installOne(g_hooks[2], reinterpret_cast<void*>(&hookSlIsFeatureSupported));
    ok = ok && installOne(g_hooks[3], reinterpret_cast<void*>(&hookSlSetConstants));
    ok = ok && installOne(g_hooks[4], reinterpret_cast<void*>(&hookSlSetFeatureConstants));
    ok = ok && installOne(g_hooks[5], reinterpret_cast<void*>(&hookSlSetFeatureEnabled));
    ok = ok && installOne(g_hooks[6], reinterpret_cast<void*>(&hookSlSetTag));

    return ok;
}

void uninstall() noexcept
{
    for (auto& slot : g_hooks)
    {
        slot.hook.reset();
    }
    bmi::sl2::unload();
}
}
