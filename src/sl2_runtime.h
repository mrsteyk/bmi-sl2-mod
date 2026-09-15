#pragma once

#include <windows.h>
#include <sl_core_types.h>
#include <sl_reflex.h>
#include <sl_dlss.h>
#include <sl_dlss_g.h>
#include <dxgi.h>
#include <d3d12.h>

namespace bmi::sl2
{
bool load() noexcept;
void unload() noexcept;
HMODULE module() noexcept;
FARPROC procedure(const char* name) noexcept;
bool init(int app_id) noexcept;

HRESULT CreateDXGIFactory(REFIID riid, void** ppFactory);
HRESULT D3D12CreateDevice(IUnknown *pAdapter, D3D_FEATURE_LEVEL MinimumFeatureLevel, REFIID riid, void **ppDevice);

sl::Result SetD3DDevice(void* device);

sl::Result GetNewFrameToken(sl::FrameToken*& token, const uint32_t* frameIndex);
sl::Result SetConstants(const sl::Constants& values, const sl::FrameToken& frame, const sl::ViewportHandle& viewport);
sl::Result SetTagForFrame(const sl::FrameToken& frame, const sl::ViewportHandle& viewport, const sl::ResourceTag* tags, uint32_t numTags, sl::CommandBuffer* cmdBuffer);
sl::Result SetFeatureLoaded(sl::Feature feature, bool loaded);

sl::Result ReflexGetState(sl::ReflexState& state);
sl::Result ReflexSetOptions(sl::ReflexOptions& options);
sl::Result ReflexSleep(const sl::FrameToken& frame);
sl::Result PCLSetMarker(sl::PCLMarker marker, const sl::FrameToken& frame);

sl::Result DLSSGSetOptions(const sl::ViewportHandle& viewport, sl::DLSSGOptions& options);
}
