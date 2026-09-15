#pragma once

#include <cstdint>
#include <sl.h>
#include <sl_reflex.h>

namespace sl1 {
using Feature = uint32_t;

//! For cases when value has to be provided and we don't have good default
constexpr float INVALID_FLOAT = 3.40282346638528859811704183484516925440e38f;
constexpr uint32_t INVALID_UINT = 0xffffffff;

struct float2
{
    float2() : x(INVALID_FLOAT), y(INVALID_FLOAT) {}
    float2(float _x, float _y) : x(_x), y(_y) {}
    float x, y;
};
static_assert(sizeof(float2) == sizeof(::sl::float2));

struct float3
{
    float3() : x(INVALID_FLOAT), y(INVALID_FLOAT), z(INVALID_FLOAT) {}
    float3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}
    float x, y, z;
};
static_assert(sizeof(float3) == sizeof(::sl::float3));

struct float4
{
    float4() : x(INVALID_FLOAT), y(INVALID_FLOAT), z(INVALID_FLOAT), w(INVALID_FLOAT) {}
    float4(float _x, float _y, float _z, float _w) : x(_x), y(_y), z(_z), w(_w) {}
    float x, y, z, w;
};
static_assert(sizeof(float4) == sizeof(::sl::float4));

struct float4x4
{
    //! All access points take row index as a parameter
    inline float4& operator[](uint32_t i) { return row[i]; }
    inline const float4& operator[](uint32_t i) const { return row[i]; }
    inline void setRow(uint32_t i, const float4& v) { row[i] = v; }
    inline const float4& getRow(uint32_t i) { return row[i]; }
private:
    //! Row major matrix
    float4 row[4];
};
static_assert(sizeof(float4x4) == sizeof(::sl::float4x4));

struct Extent
{
    uint32_t top{};
    uint32_t left{};
    uint32_t width{};
    uint32_t height{};

    inline operator bool() const { return width != 0 && height != 0; }
    inline bool operator==(const Extent& rhs) const 
    { 
        return top == rhs.top && left == rhs.left &&
        width == rhs.width && height == rhs.height;
    }
    inline bool operator!=(const Extent& rhs) const
    {
        return !operator==(rhs);
    }
};
static_assert(sizeof(Extent) == sizeof(::sl::Extent));

//! For cases when value has to be provided and we don't have good default
enum Boolean : char
{
    eFalse,
    eTrue,
    eInvalid
};
static_assert(sizeof(Boolean) == sizeof(::sl::Boolean));

//! Common constants, all parameters must be provided unless they are marked as optional
struct Constants
{
    //! IMPORTANT: All matrices are row major (see float4x4 definition) and
    //! must NOT contain temporal AA jitter offset (if any). Any jitter offset
    //! should be provided as the additional parameter Constants::jitterOffset (see below)
            
    //! Specifies matrix transformation from the camera view to the clip space.
    ::sl::float4x4 cameraViewToClip;
    //! Specifies matrix transformation from the clip space to the camera view space.
    ::sl::float4x4 clipToCameraView;
    //! Optional - Specifies matrix transformation describing lens distortion in clip space.
    ::sl::float4x4 clipToLensClip;
    //! Specifies matrix transformation from the current clip to the previous clip space.
    //! clipToPrevClip = clipToView * viewToWorld * worldToViewPrev * viewToClipPrev
    ::sl::float4x4 clipToPrevClip;
    //! Specifies matrix transformation from the previous clip to the current clip space.
    //! prevClipToClip = clipToPrevClip.inverse()
    ::sl::float4x4 prevClipToClip;
        
    //! Specifies pixel space jitter offset
    ::sl::float2 jitterOffset;
    //! Specifies scale factors used to normalize motion vectors (so the values are in [-1,1] range)
    ::sl::float2 mvecScale;
    //! Optional - Specifies camera pinhole offset if used.
    ::sl::float2 cameraPinholeOffset;
    //! Specifies camera position in world space.
    ::sl::float3 cameraPos;
    //! Specifies camera up vector in world space.
    ::sl::float3 cameraUp;
    //! Specifies camera right vector in world space.
    ::sl::float3 cameraRight;
    //! Specifies camera forward vector in world space.
    ::sl::float3 cameraFwd;
        
    //! Specifies camera near view plane distance.
    float cameraNear = INVALID_FLOAT;
    //! Specifies camera far view plane distance.
    float cameraFar = INVALID_FLOAT;
    //! Specifies camera field of view in radians.
    float cameraFOV = INVALID_FLOAT;
    //! Specifies camera aspect ratio defined as view space width divided by height.
    float cameraAspectRatio = INVALID_FLOAT;
    //! Specifies which value represents an invalid (un-initialized) value in the motion vectors buffer
    //! NOTE: This is only required if `cameraMotionIncluded` is set to false and SL needs to compute it.
    float motionVectorsInvalidValue = INVALID_FLOAT;

    //! Specifies if depth values are inverted (value closer to the camera is higher) or not.
    ::sl::Boolean depthInverted = ::sl::Boolean::eInvalid;
    //! Specifies if camera motion is included in the MVec buffer.
    ::sl::Boolean cameraMotionIncluded = ::sl::Boolean::eInvalid;
    //! Specifies if motion vectors are 3D or not.
    ::sl::Boolean motionVectors3D = ::sl::Boolean::eInvalid;
    //! Specifies if previous frame has no connection to the current one (i.e. motion vectors are invalid)
    ::sl::Boolean reset = ::sl::Boolean::eInvalid;
    //! Specifies if application is not currently rendering game frames (paused in menu, playing video cut-scenes)
    ::sl::Boolean notRenderingGameFrames = ::sl::Boolean::eInvalid;
    //! Specifies if orthographic projection is used or not.
    ::sl::Boolean orthographicProjection = ::sl::Boolean::eFalse;
    //! Specifies if motion vectors are already dilated or not.
    ::sl::Boolean motionVectorsDilated = ::sl::Boolean::eFalse;
    //! Specifies if motion vectors are jittered or not.
    ::sl::Boolean motionVectorsJittered = ::sl::Boolean::eFalse;

    //! Reserved for future expansion, must be set to null
    void* ext = {};
};

//! Resource description
struct ResourceDesc
{
    //! Indicates the type of resource
    ::sl::ResourceType type = ::sl::ResourceType::eTex2d;
    //! D3D12_RESOURCE_DESC/VkImageCreateInfo/VkBufferCreateInfo
    void* desc{};
    //! Initial state as D3D12_RESOURCE_STATES or VkMemoryPropertyFlags
    uint32_t state = 0;
    //! CD3DX12_HEAP_PROPERTIES or nullptr
    void* heap{};
    //! Reserved for future expansion, must be set to null
    void* ext{};
};

//! Native resource
struct Resource
{
    //! Indicates the type of resource
    ::sl::ResourceType type = ::sl::ResourceType::eTex2d;
    //! ID3D11Resource/ID3D12Resource/VkBuffer/VkImage
    void* native{};
    //! vkDeviceMemory or nullptr
    void* memory{};
    //! VkImageView/VkBufferView or nullptr
    void* view{};
    //! State as D3D12_RESOURCE_STATES or VkImageLayout
    //! 
    //! IMPORTANT: State needs to be correct when tagged resources are actually used.
    //! 
    uint32_t state{};
    //! Reserved for future expansion, must be set to null
    void* ext{};
};

//! Resource allocation/deallocation callbacks
//!
//! Use these callbacks to gain full control over 
//! resource life cycle and memory allocation tracking.
//!
//! @param device - Device to be used (vkDevice or ID3D11Device or ID3D12Device)
//!
//! IMPORTANT: Textures must have the pixel shader resource
//! and the unordered access view flags set
using pfunResourceAllocateCallback = Resource(const ResourceDesc* desc, void* device);
using pfunResourceReleaseCallback = void(Resource* resource, void* device);

struct Preferences
{
    //! Optional - In non-production builds it is useful to enable debugging console window
    bool showConsole = false;
    //! Optional - Various logging levels
    sl::LogLevel logLevel = sl::LogLevel::eDefault;
    //! Optional - Absolute paths to locations where to look for plugins, first path in the list has the highest priority
    const wchar_t** pathsToPlugins = {};
    //! Optional - Number of paths to search
    uint32_t numPathsToPlugins = 0;
    //! Optional - Absolute path to location where logs and other data should be stored
    //! NOTE: Set this to nullptr in order to disable logging to a file
    const wchar_t* pathToLogsAndData = {};
    //! Optional - Allows resource allocation tracking on the host side
    pfunResourceAllocateCallback* allocateCallback = {};
    //! Optional - Allows resource deallocation tracking on the host side
    pfunResourceReleaseCallback* releaseCallback = {};
    //! Optional - Allows log message tracking including critical errors if they occur
    sl::PFun_LogMessageCallback* logMessageCallback = {};
    //! Optional - Flags used to enable or disable advanced options
    sl::PreferenceFlags flags{};
    //! Required - Features to load (assuming appropriate plugins are found), if not specified NO features will be loaded by default
    const Feature* featuresToLoad = {};
    //! Required - Number of features to load, only used when list is not a null pointer
    uint32_t numFeaturesToLoad = 0;
    //! Reserved for future expansion, must be set to null
    void* ext = {};
};
static_assert(sizeof(Preferences) == 0x58);
static_assert(offsetof(Preferences, logMessageCallback) == 0x30);

struct dlssg_settings // sizeof=0x18
{
    size_t qword0;
    float float8;
    uint32_t dwordC;
    uint32_t dword10;
    uint32_t dword14;
};

struct ReflexConstants
{
    //! Specifies which mode should be used
    ::sl::ReflexMode mode = ::sl::ReflexMode::eOff;
    //! Specifies if frame limiting is enabled (0 to disable, microseconds otherwise)
    uint32_t frameLimitUs = 0;
    //! Specifies if markers are used or not (this should always be true and markers should be placed correctly)
    bool useMarkersToOptimize = false;
    //! Specifies the hot-key which should be used instead of custom message for PC latency marker
    //! Possible values: VK_F13, VK_F14, VK_F15
    uint16_t virtualKey = 0;
    //! Reserved for future expansion, must be set to null
    void* ext = {};
};

struct ReflexReport
{
    //! Various latency related stats
    uint64_t frameID{};
    uint64_t inputSampleTime{};
    uint64_t simStartTime{};
    uint64_t simEndTime{};
    uint64_t renderSubmitStartTime{};
    uint64_t renderSubmitEndTime{};
    uint64_t presentStartTime{};
    uint64_t presentEndTime{};
    uint64_t driverStartTime{};
    uint64_t driverEndTime{};
    uint64_t osRenderQueueStartTime{};
    uint64_t osRenderQueueEndTime{};
    uint64_t gpuRenderStartTime{};
    uint64_t gpuRenderEndTime{};
    uint32_t gpuActiveRenderTimeUs{};
    uint32_t gpuFrameTimeUs{};
};

struct ReflexSettings
{
    //! Specifies if low-latency mode is available or not
    bool lowLatencyAvailable = false;
    //! Specifies if latency report is available or not
    bool latencyReportAvailable = false;
    //! Specifies low latency Windows message id (if ReflexConstants::virtualKey is 0)
    uint32_t statsWindowMessage;
    //! Reflex report per frame
    ReflexReport frameReport[64];
    //! Specifies ownership of flash indicator toggle (true = driver, false = application)
    bool flashIndicatorDriverControlled = false;    
    //! Reserved for future expansion, must be set to null
    void* ext = {};
};
static_assert(sizeof(ReflexSettings) == 7704);

enum ReflexMarker
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
}