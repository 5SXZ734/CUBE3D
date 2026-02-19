// renderer_d3d12.cpp - Direct3D 12 implementation with FULL texture support
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <GLFW/glfw3.h>
#ifndef GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#endif
#include <GLFW/glfw3native.h>

#include "renderer.h"

#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <wrl/client.h>

#include <cstdio>
#include <string>
#include <unordered_map>
#include <cstring>
#include <vector>

#include "stb_image.h"
#include "dds_loader.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

using Microsoft::WRL::ComPtr;
using namespace DirectX;

static const UINT FRAME_COUNT = 2;
static const UINT MAX_TEXTURES = 64;  // Max textures we can have loaded

// ==================== D3D12 Texture ====================
struct D3D12Texture {
    ComPtr<ID3D12Resource> resource;
    UINT srvDescriptorIndex;  // Index in descriptor heap
    int width;
    int height;
};

// ==================== Helper Functions ====================
inline UINT CalcConstantBufferByteSize(UINT byteSize) {
    return (byteSize + 255) & ~255;
}

inline D3D12_RESOURCE_BARRIER TransitionBarrier(
    ID3D12Resource* resource,
    D3D12_RESOURCE_STATES stateBefore,
    D3D12_RESOURCE_STATES stateAfter)
{
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = resource;
    barrier.Transition.StateBefore = stateBefore;
    barrier.Transition.StateAfter = stateAfter;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    return barrier;
}

inline D3D12_HEAP_PROPERTIES DefaultHeapProps() {
    D3D12_HEAP_PROPERTIES props = {};
    props.Type = D3D12_HEAP_TYPE_DEFAULT;
    props.CreationNodeMask = 1;
    props.VisibleNodeMask = 1;
    return props;
}

inline D3D12_HEAP_PROPERTIES UploadHeapProps() {
    D3D12_HEAP_PROPERTIES props = {};
    props.Type = D3D12_HEAP_TYPE_UPLOAD;
    props.CreationNodeMask = 1;
    props.VisibleNodeMask = 1;
    return props;
}

inline D3D12_RESOURCE_DESC BufferDesc(UINT64 size) {
    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = size;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    return desc;
}

inline D3D12_RESOURCE_DESC Tex2DDesc(UINT width, UINT height, DXGI_FORMAT format) {
    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = width;
    desc.Height = height;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags = D3D12_RESOURCE_FLAG_NONE;
    return desc;
}

// ==================== D3D12 Mesh ====================
struct D3D12Mesh {
    ComPtr<ID3D12Resource> vertexBuffer;
    ComPtr<ID3D12Resource> indexBuffer;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
    D3D12_INDEX_BUFFER_VIEW indexBufferView;
    uint32_t indexCount;
};

// ==================== Constant Buffer ====================
// lightDir and useTexture moved to root constants
struct CBData {
    XMMATRIX world;  // 64 bytes
    // lightDir moved to root constant b2
    // useTexture moved to root constant b3
};

// Helper to convert Mat4 to XMMATRIX
static XMMATRIX Mat4ToXM(const Mat4& m) {
    return XMMATRIX(
        m.m[0], m.m[1], m.m[2], m.m[3],
        m.m[4], m.m[5], m.m[6], m.m[7],
        m.m[8], m.m[9], m.m[10], m.m[11],
        m.m[12], m.m[13], m.m[14], m.m[15]
    );
}

// ==================== D3D12 Shader ====================
struct D3D12Shader {
    ComPtr<ID3D12RootSignature> rootSignature;
    ComPtr<ID3D12PipelineState> pipelineState;
    CBData cbData;
};

// ==================== HLSL Shader Source ====================
static const char* g_hlslSrc = R"(
cbuffer CB : register(b0)
{
    float4x4 uWorld;
};

// Root constants for MVP matrix (b1 register)
cbuffer MVPConstants : register(b1)
{
    float4x4 uMVP;
};

// Root constants for lightDir (b2 register)
cbuffer LightDirConstant : register(b2)
{
    float3 uLightDir;
};

// Root constant for useTexture (b3 register)
cbuffer UseTextureConstant : register(b3)
{
    float uUseTexture;
};

// Root constant for useNormalMap (b4 register)
cbuffer UseNormalMapConstant : register(b4)
{
    float uUseNormalMap;
};

Texture2D    gTex        : register(t0);
Texture2D    gNormalMap  : register(t1);  // Normal map
SamplerState gSampler    : register(s0);

struct VSIn {
    float3 aPos       : POSITION;
    float3 aNrm       : NORMAL;
    float4 aCol       : COLOR;
    float2 aTexCoord  : TEXCOORD0;
    float3 aTangent   : TANGENT;
    float3 aBitangent : BITANGENT;
};

struct VSOut {
    float4 pos      : SV_Position;
    float3 nrmW     : TEXCOORD0;
    float4 col      : COLOR;
    float2 texCoord : TEXCOORD1;
    float3x3 TBN    : TEXCOORD2;  // TBN matrix (uses TEXCOORD2,3,4)
};

VSOut VSMain(VSIn v)
{
    VSOut o;
    o.pos = mul(float4(v.aPos, 1.0), uMVP);
    
    // Transform tangent space to world space
    float3 T = normalize(mul(float4(v.aTangent, 0.0), uWorld).xyz);
    float3 B = normalize(mul(float4(v.aBitangent, 0.0), uWorld).xyz);
    float3 N = normalize(mul(float4(v.aNrm, 0.0), uWorld).xyz);
    
    // Build TBN matrix
    o.TBN = float3x3(T, B, N);
    o.nrmW = N;
    
    o.col = v.aCol;
    o.texCoord = v.aTexCoord;
    return o;
}

float4 PSMain(VSOut i) : SV_Target
{
    // Get normal (from normal map or vertex)
    float3 N = i.nrmW;
    if (uUseNormalMap > 0.5) {
        // Sample normal map and convert from [0,1] to [-1,1]
        float3 normalMapSample = gNormalMap.Sample(gSampler, i.texCoord).rgb;
        float3 tangentNormal = normalize(normalMapSample * 2.0 - 1.0);
        
        // Transform to world space
        N = normalize(mul(tangentNormal, i.TBN));
    }
    
    // DEBUG: Visualize normal as color (remove after testing)
    // return float4(N * 0.5 + 0.5, 1.0);
    
    // Calculate lighting
    float3 L = normalize(-uLightDir);
    float ndl = max(dot(N, L), 0.0);
    float ambient = 0.18;
    float diff = ambient + ndl * 0.82;
    
    // Choose base color: texture or vertex color
    float4 baseColor = i.col;
    if (uUseTexture > 0.5) {
        baseColor = gTex.Sample(gSampler, i.texCoord);
    }
    
    return float4(baseColor.rgb * diff, baseColor.a);
}
)";

// ==================== D3D12 Renderer ====================
class D3D12Renderer : public IRenderer {
private:
    HWND m_hwnd;
    ComPtr<ID3D12Device>            m_device;
    ComPtr<ID3D12CommandQueue>      m_commandQueue;
    ComPtr<IDXGISwapChain3>         m_swapChain;
    ComPtr<ID3D12DescriptorHeap>    m_rtvHeap;
    ComPtr<ID3D12DescriptorHeap>    m_dsvHeap;
    ComPtr<ID3D12DescriptorHeap>    m_cbvSrvHeap;  // CBV + SRV descriptors
    ComPtr<ID3D12Resource>          m_renderTargets[FRAME_COUNT];
    ComPtr<ID3D12Resource>          m_depthStencil;
    ComPtr<ID3D12CommandAllocator>  m_commandAllocators[FRAME_COUNT];
    ComPtr<ID3D12GraphicsCommandList> m_commandList;
    
    // Dedicated command list/allocator for uploads during initialization
    ComPtr<ID3D12CommandAllocator> m_uploadAllocator;
    ComPtr<ID3D12GraphicsCommandList> m_uploadCommandList;

    ComPtr<ID3D12Resource> m_constantBuffers[FRAME_COUNT];  // One per frame!
    UINT8* m_cbvDataBegin[FRAME_COUNT];                     // Mapped pointers

    ComPtr<ID3D12Fence> m_fence;
    UINT64 m_fenceValues[FRAME_COUNT];
    UINT64 m_currentFenceValue;
    HANDLE m_fenceEvent;

    UINT m_rtvDescriptorSize;
    UINT m_dsvDescriptorSize;
    UINT m_cbvSrvDescriptorSize;
    UINT m_frameIndex;

    std::unordered_map<uint32_t, D3D12Mesh>    m_meshes;
    std::unordered_map<uint32_t, D3D12Shader>  m_shaders;
    std::unordered_map<uint32_t, D3D12Texture> m_textures;

    uint32_t m_nextMeshHandle    = 1;
    uint32_t m_nextShaderHandle  = 1;
    uint32_t m_nextTextureHandle = 1;
    uint32_t m_currentShader     = 0;
    UINT     m_nextSrvIndex      = 2;  // Start at 2 (0=CBV, 1=dummy texture)

    int m_width  = 1280;
    int m_height = 720;
    float m_clearColor[4] = {0,0,0,1};
    
    // Store view-projection for instanced rendering
    XMMATRIX m_viewProj;
    bool m_hasViewProj = false;
    bool m_inInstancedDraw = false;
    bool m_skipCBUpdate = false;  // Skip CB update in drawMesh when we've already done it
    bool m_depthTestEnabled  = true;
    bool m_cullingEnabled    = false;
    
    // Store normal map binding
    uint32_t m_boundNormalMap = 0;

    // ==== Helpers ====
    HWND getHWND(GLFWwindow* w) { return glfwGetWin32Window(w); }

    void createRenderTargets() {
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
        for (UINT i = 0; i < FRAME_COUNT; i++) {
            m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i]));
            m_device->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, rtvHandle);
            rtvHandle.ptr += m_rtvDescriptorSize;
        }
    }

    void createDepthStencil() {
        D3D12_RESOURCE_DESC depthDesc = {};
        depthDesc.Dimension          = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        depthDesc.Width              = m_width;
        depthDesc.Height             = m_height;
        depthDesc.DepthOrArraySize   = 1;
        depthDesc.MipLevels          = 1;
        depthDesc.Format             = DXGI_FORMAT_D32_FLOAT;
        depthDesc.SampleDesc.Count   = 1;
        depthDesc.Flags              = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

        D3D12_CLEAR_VALUE clearVal = {};
        clearVal.Format = DXGI_FORMAT_D32_FLOAT;
        clearVal.DepthStencil.Depth = 1.0f;

        D3D12_HEAP_PROPERTIES heapProps = DefaultHeapProps();
        m_device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE,
            &depthDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &clearVal,
            IID_PPV_ARGS(&m_depthStencil));

        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format        = DXGI_FORMAT_D32_FLOAT;
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

        m_device->CreateDepthStencilView(m_depthStencil.Get(), &dsvDesc,
            m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
    }

    void waitForGpu() {
        if (!m_fence || !m_commandQueue) return;
        m_currentFenceValue++;
        m_commandQueue->Signal(m_fence.Get(), m_currentFenceValue);
        if (m_fence->GetCompletedValue() < m_currentFenceValue) {
            m_fence->SetEventOnCompletion(m_currentFenceValue, m_fenceEvent);
            WaitForSingleObject(m_fenceEvent, INFINITE);
        }
    }

    void moveToNextFrame() {
        const UINT64 currentFenceValue = m_currentFenceValue;
        m_commandQueue->Signal(m_fence.Get(), currentFenceValue);
        m_currentFenceValue++;

        m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

        if (m_fence->GetCompletedValue() < m_fenceValues[m_frameIndex]) {
            m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent);
            WaitForSingleObject(m_fenceEvent, INFINITE);
        }

        m_fenceValues[m_frameIndex] = currentFenceValue;
    }

    bool compileShader(const char* src, const char* entry, const char* target,
                       ComPtr<ID3DBlob>& out) {
        UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
        flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
        ComPtr<ID3DBlob> err;
        HRESULT hr = D3DCompile(src, strlen(src), nullptr, nullptr, nullptr,
                                entry, target, flags, 0, out.GetAddressOf(),
                                err.GetAddressOf());
        if (FAILED(hr)) {
            if (err) std::fprintf(stderr, "Shader error: %s\n",
                                  (const char*)err->GetBufferPointer());
            return false;
        }
        return true;
    }

public:
    D3D12Renderer() : m_currentFenceValue(1), m_frameIndex(0) {
        for (UINT i = 0; i < FRAME_COUNT; i++) {
            m_fenceValues[i] = 0;
            m_cbvDataBegin[i] = nullptr;
        }
    }

    virtual ~D3D12Renderer() { shutdown(); }

    // ================================================================
    bool initialize(GLFWwindow* window) override {
        m_hwnd = getHWND(window);
        glfwGetFramebufferSize(window, &m_width, &m_height);

        // Enable debug layer
#ifdef _DEBUG
        {
            ComPtr<ID3D12Debug> debugController;
            if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
                debugController->EnableDebugLayer();
        }
#endif

        // Create device
        HRESULT hr = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0,
                                       IID_PPV_ARGS(&m_device));
        if (FAILED(hr)) {
            std::fprintf(stderr, "D3D12CreateDevice failed\n");
            return false;
        }

        // Command queue
        D3D12_COMMAND_QUEUE_DESC queueDesc = {};
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue));

        // Swap chain
        ComPtr<IDXGIFactory4> factory;
        CreateDXGIFactory1(IID_PPV_ARGS(&factory));

        DXGI_SWAP_CHAIN_DESC1 scDesc = {};
        scDesc.Width       = m_width;
        scDesc.Height      = m_height;
        scDesc.Format      = DXGI_FORMAT_R8G8B8A8_UNORM;
        scDesc.SampleDesc.Count = 1;
        scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        scDesc.BufferCount = FRAME_COUNT;
        scDesc.SwapEffect  = DXGI_SWAP_EFFECT_FLIP_DISCARD;

        ComPtr<IDXGISwapChain1> sc1;
        factory->CreateSwapChainForHwnd(m_commandQueue.Get(), m_hwnd, &scDesc,
                                       nullptr, nullptr, &sc1);
        sc1.As(&m_swapChain);
        m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

        // Descriptor heaps
        {
            D3D12_DESCRIPTOR_HEAP_DESC rtvDesc = {};
            rtvDesc.NumDescriptors = FRAME_COUNT;
            rtvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
            m_device->CreateDescriptorHeap(&rtvDesc, IID_PPV_ARGS(&m_rtvHeap));

            D3D12_DESCRIPTOR_HEAP_DESC dsvDesc = {};
            dsvDesc.NumDescriptors = 1;
            dsvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
            m_device->CreateDescriptorHeap(&dsvDesc, IID_PPV_ARGS(&m_dsvHeap));

            D3D12_DESCRIPTOR_HEAP_DESC cbvSrvDesc = {};
            cbvSrvDesc.NumDescriptors = 1 + MAX_TEXTURES;  // 1 CB + textures
            cbvSrvDesc.Type  = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            cbvSrvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            m_device->CreateDescriptorHeap(&cbvSrvDesc, IID_PPV_ARGS(&m_cbvSrvHeap));

            m_rtvDescriptorSize    = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
            m_dsvDescriptorSize    = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
            m_cbvSrvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        }

        createRenderTargets();
        createDepthStencil();

        // Command allocators
        for (UINT i = 0; i < FRAME_COUNT; i++) {
            m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                IID_PPV_ARGS(&m_commandAllocators[i]));
        }

        // Command list
        m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
            m_commandAllocators[m_frameIndex].Get(), nullptr,
            IID_PPV_ARGS(&m_commandList));
        m_commandList->Close();
        
        // Create dedicated upload command list for init-time resource uploads
        m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(&m_uploadAllocator));
        m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
            m_uploadAllocator.Get(), nullptr,
            IID_PPV_ARGS(&m_uploadCommandList));
        m_uploadCommandList->Close();

        // Constant buffers - one per frame for double buffering!
        {
            UINT cbSize = CalcConstantBufferByteSize(sizeof(CBData));
            D3D12_RESOURCE_DESC cbDesc = BufferDesc(cbSize);
            D3D12_HEAP_PROPERTIES uploadProps = UploadHeapProps();
            
            for (UINT i = 0; i < FRAME_COUNT; i++) {
                m_device->CreateCommittedResource(&uploadProps, D3D12_HEAP_FLAG_NONE,
                    &cbDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                    IID_PPV_ARGS(&m_constantBuffers[i]));

                D3D12_RANGE readRange = {0, 0};
                m_constantBuffers[i]->Map(0, &readRange, reinterpret_cast<void**>(&m_cbvDataBegin[i]));
            }

            // Create CBV at descriptor index 0 for frame 0's constant buffer
            // (We'll update the root CBV descriptor each frame)
            D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
            cbvDesc.BufferLocation = m_constantBuffers[0]->GetGPUVirtualAddress();
            cbvDesc.SizeInBytes    = cbSize;

            D3D12_CPU_DESCRIPTOR_HANDLE cbvHandle = m_cbvSrvHeap->GetCPUDescriptorHandleForHeapStart();
            m_device->CreateConstantBufferView(&cbvDesc, cbvHandle);
        }

        // Fence
        m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));
        m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

        std::printf("Direct3D 12 Renderer initialized\n");
        
        // Create 1x1 black dummy texture for when no texture is bound
        // This occupies SRV index 1
        {
            unsigned char blackPixel[4] = {0, 0, 0, 255};  // Black, opaque
            
            D3D12_RESOURCE_DESC texDesc = Tex2DDesc(1, 1, DXGI_FORMAT_R8G8B8A8_UNORM);
            D3D12_HEAP_PROPERTIES defaultProps = DefaultHeapProps();
            ComPtr<ID3D12Resource> dummyTexture;
            m_device->CreateCommittedResource(&defaultProps, D3D12_HEAP_FLAG_NONE,
                &texDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                IID_PPV_ARGS(&dummyTexture));

            // Upload
            D3D12_RESOURCE_DESC uploadDesc = BufferDesc(256);  // 256 bytes for alignment
            D3D12_HEAP_PROPERTIES uploadProps = UploadHeapProps();
            ComPtr<ID3D12Resource> uploadBuffer;
            m_device->CreateCommittedResource(&uploadProps, D3D12_HEAP_FLAG_NONE,
                &uploadDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                IID_PPV_ARGS(&uploadBuffer));

            void* mapped;
            uploadBuffer->Map(0, nullptr, &mapped);
            memcpy(mapped, blackPixel, 4);
            uploadBuffer->Unmap(0, nullptr);

            // Copy via command list (use dedicated upload command list)
            m_uploadAllocator->Reset();
            m_uploadCommandList->Reset(m_uploadAllocator.Get(), nullptr);

            D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
            footprint.Footprint.Format   = DXGI_FORMAT_R8G8B8A8_UNORM;
            footprint.Footprint.Width    = 1;
            footprint.Footprint.Height   = 1;
            footprint.Footprint.Depth    = 1;
            footprint.Footprint.RowPitch = 256;

            D3D12_TEXTURE_COPY_LOCATION srcLoc = {};
            srcLoc.pResource = uploadBuffer.Get();
            srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            srcLoc.PlacedFootprint = footprint;

            D3D12_TEXTURE_COPY_LOCATION dstLoc = {};
            dstLoc.pResource = dummyTexture.Get();
            dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            dstLoc.SubresourceIndex = 0;

            m_uploadCommandList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);

            D3D12_RESOURCE_BARRIER barrier = TransitionBarrier(dummyTexture.Get(),
                D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            m_uploadCommandList->ResourceBarrier(1, &barrier);

            m_uploadCommandList->Close();
            ID3D12CommandList* lists[] = { m_uploadCommandList.Get() };
            m_commandQueue->ExecuteCommandLists(1, lists);
            waitForGpu();

            // Create SRV at index 1 (index 0 is CBV)
            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MipLevels = 1;

            D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = m_cbvSrvHeap->GetCPUDescriptorHandleForHeapStart();
            srvHandle.ptr += 1 * m_cbvSrvDescriptorSize;  // Index 1
            m_device->CreateShaderResourceView(dummyTexture.Get(), &srvDesc, srvHandle);
            
            // Also create dummy at index 2 for normal map slot
            // Normal maps should have RGB(128,128,255) which is (0,0,1) in tangent space
            D3D12_CPU_DESCRIPTOR_HANDLE srvHandle2 = m_cbvSrvHeap->GetCPUDescriptorHandleForHeapStart();
            srvHandle2.ptr += 2 * m_cbvSrvDescriptorSize;  // Index 2
            m_device->CreateShaderResourceView(dummyTexture.Get(), &srvDesc, srvHandle2);
            
            // Initialize staging slots (63, 64) with dummy descriptors
            D3D12_CPU_DESCRIPTOR_HANDLE stagingDiffuse = m_cbvSrvHeap->GetCPUDescriptorHandleForHeapStart();
            stagingDiffuse.ptr += (MAX_TEXTURES - 1) * m_cbvSrvDescriptorSize;  // Slot 63
            m_device->CreateShaderResourceView(dummyTexture.Get(), &srvDesc, stagingDiffuse);
            
            D3D12_CPU_DESCRIPTOR_HANDLE stagingNormal = m_cbvSrvHeap->GetCPUDescriptorHandleForHeapStart();
            stagingNormal.ptr += MAX_TEXTURES * m_cbvSrvDescriptorSize;  // Slot 64
            m_device->CreateShaderResourceView(dummyTexture.Get(), &srvDesc, stagingNormal);
            
            std::printf("Created dummy textures at SRV indices 1, 2, 63 (staging diffuse), 64 (staging normal)\n");
        }
        
        return true;
    }

    // ================================================================
    void shutdown() override {
        waitForGpu();

        m_meshes.clear();
        m_shaders.clear();
        m_textures.clear();

        for (UINT i = 0; i < FRAME_COUNT; i++) {
            if (m_constantBuffers[i]) {
                m_constantBuffers[i]->Unmap(0, nullptr);
                m_constantBuffers[i].Reset();
            }
        }
        
        if (m_fenceEvent) {
            CloseHandle(m_fenceEvent);
            m_fenceEvent = nullptr;
        }
        
        m_fence.Reset();
        m_uploadCommandList.Reset();
        m_uploadAllocator.Reset();
        m_commandList.Reset();
        for (auto& ca : m_commandAllocators) ca.Reset();
        m_depthStencil.Reset();
        for (auto& rt : m_renderTargets) rt.Reset();
        m_cbvSrvHeap.Reset();
        m_dsvHeap.Reset();
        m_rtvHeap.Reset();
        m_swapChain.Reset();
        m_commandQueue.Reset();
        m_device.Reset();
    }

    // ================================================================
    void beginFrame() override {
        // Wait for this frame's allocator to be free before resetting it
        if (m_fence->GetCompletedValue() < m_fenceValues[m_frameIndex]) {
            m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent);
            WaitForSingleObject(m_fenceEvent, INFINITE);
        }
        
        m_commandAllocators[m_frameIndex]->Reset();
        m_commandList->Reset(m_commandAllocators[m_frameIndex].Get(), nullptr);

        D3D12_RESOURCE_BARRIER barrier = TransitionBarrier(
            m_renderTargets[m_frameIndex].Get(),
            D3D12_RESOURCE_STATE_PRESENT,
            D3D12_RESOURCE_STATE_RENDER_TARGET);
        m_commandList->ResourceBarrier(1, &barrier);

        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
        rtvHandle.ptr += m_frameIndex * m_rtvDescriptorSize;

        D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();

        m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);
        m_commandList->ClearRenderTargetView(rtvHandle, m_clearColor, 0, nullptr);
        m_commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

        D3D12_VIEWPORT vp = { 0, 0, (float)m_width, (float)m_height, 0, 1 };
        D3D12_RECT scissor = { 0, 0, (LONG)m_width, (LONG)m_height };
        m_commandList->RSSetViewports(1, &vp);
        m_commandList->RSSetScissorRects(1, &scissor);
    }

    void endFrame() override {
        D3D12_RESOURCE_BARRIER barrier = TransitionBarrier(
            m_renderTargets[m_frameIndex].Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PRESENT);
        m_commandList->ResourceBarrier(1, &barrier);

        m_commandList->Close();
        ID3D12CommandList* lists[] = { m_commandList.Get() };
        m_commandQueue->ExecuteCommandLists(1, lists);

        m_swapChain->Present(1, 0);
        moveToNextFrame();
    }

    void setClearColor(float r, float g, float b, float a) override {
        m_clearColor[0]=r; m_clearColor[1]=g;
        m_clearColor[2]=b; m_clearColor[3]=a;
    }

    void setViewport(int w, int h) override {
        waitForGpu();
        m_width = w; m_height = h;

        for (auto& rt : m_renderTargets) rt.Reset();
        m_swapChain->ResizeBuffers(FRAME_COUNT, w, h, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
        m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

        createRenderTargets();
        m_depthStencil.Reset();
        createDepthStencil();
    }

    // ================================================================
    uint32_t createMesh(const Vertex* verts, uint32_t vCount,
                        const uint16_t* idx, uint32_t iCount) override {
        D3D12Mesh mesh;
        mesh.indexCount = iCount;

        UINT vbSize = vCount * sizeof(Vertex);
        UINT ibSize = iCount * sizeof(uint16_t);

        D3D12_RESOURCE_DESC vbDesc = BufferDesc(vbSize);
        D3D12_RESOURCE_DESC ibDesc = BufferDesc(ibSize);

        D3D12_HEAP_PROPERTIES uploadProps = UploadHeapProps();

        m_device->CreateCommittedResource(&uploadProps, D3D12_HEAP_FLAG_NONE,
            &vbDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(&mesh.vertexBuffer));

        m_device->CreateCommittedResource(&uploadProps, D3D12_HEAP_FLAG_NONE,
            &ibDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(&mesh.indexBuffer));

        void* vbData; void* ibData;
        mesh.vertexBuffer->Map(0, nullptr, &vbData);
        mesh.indexBuffer->Map(0, nullptr, &ibData);
        memcpy(vbData, verts, vbSize);
        memcpy(ibData, idx, ibSize);
        mesh.vertexBuffer->Unmap(0, nullptr);
        mesh.indexBuffer->Unmap(0, nullptr);

        mesh.vertexBufferView.BufferLocation = mesh.vertexBuffer->GetGPUVirtualAddress();
        mesh.vertexBufferView.SizeInBytes    = vbSize;
        mesh.vertexBufferView.StrideInBytes  = sizeof(Vertex);

        mesh.indexBufferView.BufferLocation = mesh.indexBuffer->GetGPUVirtualAddress();
        mesh.indexBufferView.SizeInBytes    = ibSize;
        mesh.indexBufferView.Format         = DXGI_FORMAT_R16_UINT;

        uint32_t h = m_nextMeshHandle++;
        m_meshes[h] = std::move(mesh);
        return h;
    }

    void destroyMesh(uint32_t h) override {
        auto it = m_meshes.find(h);
        if (it != m_meshes.end()) {
            waitForGpu();
            m_meshes.erase(it);
        }
    }

    // ================================================================
    uint32_t createShader(const char* /*vs*/, const char* /*fs*/) override {
        D3D12Shader shader;

        ComPtr<ID3DBlob> vsBlob, psBlob;
        if (!compileShader(g_hlslSrc, "VSMain", "vs_5_0", vsBlob)) {
            std::fprintf(stderr, "Failed to compile vertex shader\n");
            return 0;
        }
        if (!compileShader(g_hlslSrc, "PSMain", "ps_5_0", psBlob)) {
            std::fprintf(stderr, "Failed to compile pixel shader\n");
            return 0;
        }

        // Root signature: 
        // [0] = CBV(b0) for world matrix only (64 bytes)
        // [1] = SRV descriptor table for diffuse texture (t0) - SINGLE descriptor
        // [2] = SRV descriptor table for normal map (t1) - SINGLE descriptor  
        // [3] = Root constants for MVP matrix (16 floats)
        // [4] = Root constants for lightDir (3 floats)
        // [5] = Root constant for useTexture (1 float)
        // [6] = Root constant for useNormalMap (1 float)
        D3D12_ROOT_PARAMETER rootParams[7] = {};
        
        rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        rootParams[0].Descriptor.ShaderRegister = 0;
        rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        // Diffuse texture (t0) - separate table
        D3D12_DESCRIPTOR_RANGE diffuseRange = {};
        diffuseRange.RangeType          = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        diffuseRange.NumDescriptors     = 1;  // Just diffuse
        diffuseRange.BaseShaderRegister = 0;  // t0
        diffuseRange.OffsetInDescriptorsFromTableStart = 0;

        rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParams[1].DescriptorTable.NumDescriptorRanges = 1;
        rootParams[1].DescriptorTable.pDescriptorRanges   = &diffuseRange;
        rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        
        // Normal map (t1) - separate table
        D3D12_DESCRIPTOR_RANGE normalRange = {};
        normalRange.RangeType          = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        normalRange.NumDescriptors     = 1;  // Just normal
        normalRange.BaseShaderRegister = 1;  // t1
        normalRange.OffsetInDescriptorsFromTableStart = 0;

        rootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParams[2].DescriptorTable.NumDescriptorRanges = 1;
        rootParams[2].DescriptorTable.pDescriptorRanges   = &normalRange;
        rootParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        
        // Root constants for MVP matrix (16 floats)
        rootParams[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        rootParams[3].Constants.ShaderRegister = 1;  // b1 register
        rootParams[3].Constants.RegisterSpace = 0;
        rootParams[3].Constants.Num32BitValues = 16;  // 4x4 matrix
        rootParams[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
        
        // Root constants for lightDir (3 floats)
        rootParams[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        rootParams[4].Constants.ShaderRegister = 2;  // b2 register
        rootParams[4].Constants.RegisterSpace = 0;
        rootParams[4].Constants.Num32BitValues = 3;  // float3
        rootParams[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        
        // Root constant for useTexture (1 float)
        rootParams[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        rootParams[5].Constants.ShaderRegister = 3;  // b3 register
        rootParams[5].Constants.RegisterSpace = 0;
        rootParams[5].Constants.Num32BitValues = 1;  // 1 float
        rootParams[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        
        // Root constant for useNormalMap (1 float)
        rootParams[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        rootParams[6].Constants.ShaderRegister = 4;  // b4 register
        rootParams[6].Constants.RegisterSpace = 0;
        rootParams[6].Constants.Num32BitValues = 1;  // 1 float
        rootParams[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        D3D12_STATIC_SAMPLER_DESC sampler = {};
        sampler.Filter         = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        sampler.AddressU       = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampler.AddressV       = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampler.AddressW       = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampler.MaxLOD         = D3D12_FLOAT32_MAX;
        sampler.ShaderRegister = 0;
        sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        D3D12_ROOT_SIGNATURE_DESC rsDesc = {};
        rsDesc.NumParameters     = 7;  // CBV + diffuse table + normal table + MVP + lightDir + useTexture + useNormalMap
        rsDesc.pParameters       = rootParams;
        rsDesc.NumStaticSamplers = 1;
        rsDesc.pStaticSamplers   = &sampler;
        rsDesc.Flags             = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        ComPtr<ID3DBlob> signature, error;
        HRESULT hr = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1,
                                   &signature, &error);
        if (FAILED(hr)) {
            if (error) {
                std::fprintf(stderr, "Root signature serialization failed: %s\n",
                           (const char*)error->GetBufferPointer());
            }
            return 0;
        }
        
        hr = m_device->CreateRootSignature(0, signature->GetBufferPointer(),
            signature->GetBufferSize(), IID_PPV_ARGS(&shader.rootSignature));
        if (FAILED(hr)) {
            std::fprintf(stderr, "CreateRootSignature failed: 0x%08X\n", hr);
            return 0;
        }

        // Input layout
        D3D12_INPUT_ELEMENT_DESC layout[] = {
            { "POSITION",  0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "NORMAL",    0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "COLOR",     0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD",  0, DXGI_FORMAT_R32G32_FLOAT,       0, 40, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TANGENT",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 48, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "BITANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 60, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        };

        // PSO
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.pRootSignature = shader.rootSignature.Get();
        psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
        psoDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };
        psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        psoDesc.RasterizerState.CullMode = m_cullingEnabled ? D3D12_CULL_MODE_BACK : D3D12_CULL_MODE_NONE;
        psoDesc.RasterizerState.FrontCounterClockwise = TRUE;
        psoDesc.RasterizerState.DepthClipEnable = TRUE;
        psoDesc.DepthStencilState.DepthEnable = m_depthTestEnabled;
        psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
        psoDesc.InputLayout = { layout, (UINT)std::size(layout) };
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        psoDesc.SampleDesc.Count = 1;

        hr = m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&shader.pipelineState));
        if (FAILED(hr)) {
            std::fprintf(stderr, "CreateGraphicsPipelineState failed: 0x%08X\n", hr);
            return 0;
        }
        
        std::printf("D3D12: Shader created successfully\n");
        std::printf("D3D12: sizeof(CBData) = %zu bytes\n", sizeof(CBData));
        std::printf("D3D12: offsetof(CBData, world) = %zu\n", offsetof(CBData, world));
        std::printf("D3D12: (lightDir and useTexture moved to root constants)\n");

        shader.cbData.world = XMMatrixIdentity();

        uint32_t h = m_nextShaderHandle++;
        m_shaders[h] = std::move(shader);
        return h;
    }

    void destroyShader(uint32_t h) override { m_shaders.erase(h); }

    void useShader(uint32_t h) override {
        auto it = m_shaders.find(h);
        if (it == m_shaders.end()) return;
        m_currentShader = h;
        D3D12Shader& s = it->second;
        m_commandList->SetPipelineState(s.pipelineState.Get());
        m_commandList->SetGraphicsRootSignature(s.rootSignature.Get());

        ID3D12DescriptorHeap* heaps[] = { m_cbvSrvHeap.Get() };
        m_commandList->SetDescriptorHeaps(1, heaps);
    }

    // ================================================================
    void setUniformMat4(uint32_t h, const char* name, const Mat4& m) override {
        static bool debugOnce = true;
        if (debugOnce) {
            printf("D3D12 setUniformMat4: shader=%u, name=%s\n", h, name);
            debugOnce = false;
        }
        
        auto it = m_shaders.find(h);
        if (it == m_shaders.end()) {
            printf("D3D12 setUniformMat4: Shader %u NOT FOUND!\n", h);
            return;
        }
        XMMATRIX xm = XMMatrixTranspose(XMLoadFloat4x4((const XMFLOAT4X4*)m.m));
        
        if (strcmp(name, "uMVP") == 0) {
            // MVP is now a root constant, not in the constant buffer
            // Store for later use in drawMesh/drawMeshInstanced
            if (!m_inInstancedDraw) {
                m_viewProj = xm;
                m_hasViewProj = true;
                
                static bool once = true;
                if (once) {
                    printf("D3D12: Stored viewProj for root constants, hasViewProj=%d\n", m_hasViewProj);
                    once = false;
                }
            }
        } else if (strcmp(name, "uWorld") == 0) {
            it->second.cbData.world = xm;
        }
    }

    Vec3 m_lightDir = {0, -1, 0};  // Store for root constant pushing
    float m_useNormalMap = 0.0f;    // Store for root constant pushing
    
    void setUniformVec3(uint32_t h, const char* name, const Vec3& v) override {
        if (strcmp(name, "uLightDir") == 0) {
            m_lightDir = v;  // Store for later use in draw calls
        }
    }

    void setUniformInt(uint32_t h, const char* name, int value) override {
        // Root constants - store for use in draw calls
        if (strcmp(name, "uUseNormalMap") == 0) {
            m_useNormalMap = (float)value;
        }
        // useTexture is set per-draw-call, not stored globally
    }

    // ================================================================
    uint32_t createTexture(const char* filepath) override {
        if (m_nextSrvIndex >= (1 + MAX_TEXTURES)) {
            std::fprintf(stderr, "Texture limit reached (%d)\n", MAX_TEXTURES);
            return 0;
        }

        int w, h, ch;
        unsigned char* data = nullptr;
        
        // Check if DDS
        const char* ext = strrchr(filepath, '.');
        if (ext && (strcmp(ext, ".dds") == 0 || strcmp(ext, ".DDS") == 0)) {
            data = DDSLoader::Load(filepath, &w, &h, &ch);
            if (data) {
                std::printf("Loaded DDS texture: %s %dx%d (%d channels)\n", filepath, w, h, ch);
            }
        }
        
        // Fall back to stb_image - FORCE 4 channels (RGBA)
        if (!data) {
            stbi_set_flip_vertically_on_load(false);
            data = stbi_load(filepath, &w, &h, &ch, 4);  // Force RGBA
            if (data) {
                std::printf("Loaded texture: %s %dx%d (original %d channels, forced to 4)\n", 
                           filepath, w, h, ch);
                ch = 4;  // We forced it to 4
            }
        }
        
        if (!data) {
            std::fprintf(stderr, "Texture load failed: %s\n", filepath);
            return 0;
        }
        
        std::printf("D3D12: Creating texture %dx%d, SRV index %d\n", w, h, m_nextSrvIndex);

        D3D12Texture tex;
        tex.width  = w;
        tex.height = h;
        tex.srvDescriptorIndex = m_nextSrvIndex++;

        // Create GPU resource
        D3D12_RESOURCE_DESC texDesc = Tex2DDesc(w, h, DXGI_FORMAT_R8G8B8A8_UNORM);
        D3D12_HEAP_PROPERTIES defaultProps = DefaultHeapProps();
        m_device->CreateCommittedResource(&defaultProps, D3D12_HEAP_FLAG_NONE,
            &texDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
            IID_PPV_ARGS(&tex.resource));

        // D3D12 requires 256-byte aligned row pitch for texture uploads
        UINT rowPitch = (w * 4 + 255) & ~255;  // Align to 256 bytes
        UINT64 uploadSize = rowPitch * h;
        
        // Upload buffer with aligned size
        D3D12_RESOURCE_DESC uploadDesc = BufferDesc(uploadSize);
        D3D12_HEAP_PROPERTIES uploadProps = UploadHeapProps();
        ComPtr<ID3D12Resource> uploadBuffer;
        m_device->CreateCommittedResource(&uploadProps, D3D12_HEAP_FLAG_NONE,
            &uploadDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(&uploadBuffer));

        // Copy data to upload buffer with row padding
        void* mapped;
        uploadBuffer->Map(0, nullptr, &mapped);
        unsigned char* dst = (unsigned char*)mapped;
        for (int y = 0; y < h; y++) {
            memcpy(dst + y * rowPitch, data + y * w * 4, w * 4);
        }
        uploadBuffer->Unmap(0, nullptr);
        
        if (ext && (strcmp(ext, ".dds") == 0 || strcmp(ext, ".DDS") == 0))
            delete[] data;
        else
            stbi_image_free(data);

        // Copy to GPU texture via dedicated upload command list
        // Wait for any previous upload to complete before resetting
        if (m_currentFenceValue > 0) {
            if (m_fence->GetCompletedValue() < m_currentFenceValue) {
                m_fence->SetEventOnCompletion(m_currentFenceValue, m_fenceEvent);
                WaitForSingleObject(m_fenceEvent, INFINITE);
            }
        }
        m_uploadAllocator->Reset();
        m_uploadCommandList->Reset(m_uploadAllocator.Get(), nullptr);

        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
        footprint.Footprint.Format   = DXGI_FORMAT_R8G8B8A8_UNORM;
        footprint.Footprint.Width    = w;
        footprint.Footprint.Height   = h;
        footprint.Footprint.Depth    = 1;
        footprint.Footprint.RowPitch = rowPitch;  // Must be 256-byte aligned!

        D3D12_TEXTURE_COPY_LOCATION srcLoc = {};
        srcLoc.pResource        = uploadBuffer.Get();
        srcLoc.Type             = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        srcLoc.PlacedFootprint  = footprint;

        D3D12_TEXTURE_COPY_LOCATION dstLoc = {};
        dstLoc.pResource = tex.resource.Get();
        dstLoc.Type      = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dstLoc.SubresourceIndex = 0;

        m_uploadCommandList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);

        D3D12_RESOURCE_BARRIER barrier = TransitionBarrier(tex.resource.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        m_uploadCommandList->ResourceBarrier(1, &barrier);

        m_uploadCommandList->Close();
        ID3D12CommandList* lists[] = { m_uploadCommandList.Get() };
        m_commandQueue->ExecuteCommandLists(1, lists);
        waitForGpu();

        // Create SRV
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format                  = DXGI_FORMAT_R8G8B8A8_UNORM;
        srvDesc.ViewDimension           = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels     = 1;

        D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = m_cbvSrvHeap->GetCPUDescriptorHandleForHeapStart();
        srvHandle.ptr += tex.srvDescriptorIndex * m_cbvSrvDescriptorSize;
        m_device->CreateShaderResourceView(tex.resource.Get(), &srvDesc, srvHandle);

        uint32_t handle = m_nextTextureHandle++;
        m_textures[handle] = std::move(tex);
        return handle;
    }

    void destroyTexture(uint32_t h) override {
        auto it = m_textures.find(h);
        if (it != m_textures.end()) {
            waitForGpu();
            m_textures.erase(it);
        }
    }
    
    uint32_t createTextureFromData(const uint8_t* data, int width, int height, int channels) override {
        if (!data) {
            std::fprintf(stderr, "D3D12: createTextureFromData - null data\n");
            return 0;
        }
        
        if (m_nextSrvIndex >= (1 + MAX_TEXTURES)) {
            std::fprintf(stderr, "D3D12: Texture limit reached\n");
            return 0;
        }
        
        std::printf("D3D12: Creating texture from data (%dx%d, %d channels)\n", width, height, channels);
        
        // Convert to RGBA if needed
        std::vector<uint8_t> rgba_data;
        const uint8_t* upload_data = data;
        int upload_channels = channels;
        
        if (channels == 3) {
            rgba_data.resize(width * height * 4);
            for (int i = 0; i < width * height; i++) {
                rgba_data[i * 4 + 0] = data[i * 3 + 0];
                rgba_data[i * 4 + 1] = data[i * 3 + 1];
                rgba_data[i * 4 + 2] = data[i * 3 + 2];
                rgba_data[i * 4 + 3] = 255;
            }
            upload_data = rgba_data.data();
            upload_channels = 4;
        }
        
        D3D12Texture tex;
        tex.width = width;
        tex.height = height;
        tex.srvDescriptorIndex = m_nextSrvIndex++;
        
        // Create GPU resource
        D3D12_RESOURCE_DESC texDesc = Tex2DDesc(width, height, DXGI_FORMAT_R8G8B8A8_UNORM);
        D3D12_HEAP_PROPERTIES defaultProps = DefaultHeapProps();
        m_device->CreateCommittedResource(&defaultProps, D3D12_HEAP_FLAG_NONE,
            &texDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
            IID_PPV_ARGS(&tex.resource));
        
        // D3D12 requires 256-byte aligned row pitch
        UINT rowPitch = (width * 4 + 255) & ~255;
        UINT64 uploadSize = rowPitch * height;
        
        // Upload buffer
        D3D12_RESOURCE_DESC uploadDesc = BufferDesc(uploadSize);
        D3D12_HEAP_PROPERTIES uploadProps = UploadHeapProps();
        ComPtr<ID3D12Resource> uploadBuffer;
        m_device->CreateCommittedResource(&uploadProps, D3D12_HEAP_FLAG_NONE,
            &uploadDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(&uploadBuffer));
        
        // Copy data with row padding
        void* mapped;
        uploadBuffer->Map(0, nullptr, &mapped);
        unsigned char* dst = (unsigned char*)mapped;
        for (int y = 0; y < height; y++) {
            memcpy(dst + y * rowPitch, upload_data + y * width * upload_channels, width * upload_channels);
        }
        uploadBuffer->Unmap(0, nullptr);
        
        // Upload via command list
        if (m_currentFenceValue > 0) {
            if (m_fence->GetCompletedValue() < m_currentFenceValue) {
                m_fence->SetEventOnCompletion(m_currentFenceValue, m_fenceEvent);
                WaitForSingleObject(m_fenceEvent, INFINITE);
            }
        }
        m_uploadAllocator->Reset();
        m_uploadCommandList->Reset(m_uploadAllocator.Get(), nullptr);
        
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
        footprint.Footprint.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        footprint.Footprint.Width = width;
        footprint.Footprint.Height = height;
        footprint.Footprint.Depth = 1;
        footprint.Footprint.RowPitch = rowPitch;
        
        D3D12_TEXTURE_COPY_LOCATION srcLoc = {};
        srcLoc.pResource = uploadBuffer.Get();
        srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        srcLoc.PlacedFootprint = footprint;
        
        D3D12_TEXTURE_COPY_LOCATION dstLoc = {};
        dstLoc.pResource = tex.resource.Get();
        dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dstLoc.SubresourceIndex = 0;
        
        m_uploadCommandList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);
        
        D3D12_RESOURCE_BARRIER barrier = TransitionBarrier(tex.resource.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        m_uploadCommandList->ResourceBarrier(1, &barrier);
        
        m_uploadCommandList->Close();
        ID3D12CommandList* lists[] = { m_uploadCommandList.Get() };
        m_commandQueue->ExecuteCommandLists(1, lists);
        waitForGpu();
        
        // Create SRV
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        
        D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = m_cbvSrvHeap->GetCPUDescriptorHandleForHeapStart();
        srvHandle.ptr += tex.srvDescriptorIndex * m_cbvSrvDescriptorSize;
        m_device->CreateShaderResourceView(tex.resource.Get(), &srvDesc, srvHandle);
        
        uint32_t handle = m_nextTextureHandle++;
        m_textures[handle] = std::move(tex);
        std::printf("D3D12: Texture created successfully, handle=%u, SRV index=%d\n", 
                   handle, tex.srvDescriptorIndex);
        return handle;
    }
    
    void bindTextureToUnit(uint32_t textureHandle, int unit) override {
        // D3D12: Store normal map handle for binding in descriptor table
        if (unit == 1) {
            m_boundNormalMap = textureHandle;
            std::printf("D3D12: Normal map bound, handle=%u\n", textureHandle);
        }
    }

    // ================================================================
    void drawMesh(uint32_t meshHandle, uint32_t textureHandle = 0) override {
        auto meshIt = m_meshes.find(meshHandle);
        if (meshIt == m_meshes.end()) return;

        // Update CB for current frame (world, lightDir, useTexture)
        auto shIt = m_shaders.find(m_currentShader);
        if (shIt != m_shaders.end() && !m_skipCBUpdate) {
            D3D12Shader& sh = shIt->second;
            // useTexture moved to root constant, no longer in CB
            
            memcpy(m_cbvDataBegin[m_frameIndex], &sh.cbData, sizeof(CBData));
            
            // Bind the current frame's constant buffer (b0)
            m_commandList->SetGraphicsRootConstantBufferView(0,
                m_constantBuffers[m_frameIndex]->GetGPUVirtualAddress());
        } else if (shIt != m_shaders.end()) {
            // Still need to bind the CB even if we skip the update
            m_commandList->SetGraphicsRootConstantBufferView(0,
                m_constantBuffers[m_frameIndex]->GetGPUVirtualAddress());
        }
        
        // Push MVP matrix as root constants (root parameter 3, b1 register) - moved to slot 3
        if (m_hasViewProj) {
            XMFLOAT4X4 mvpData;
            XMStoreFloat4x4(&mvpData, m_viewProj);
            m_commandList->SetGraphicsRoot32BitConstants(3, 16, &mvpData, 0);
        }
        
        // Push lightDir as root constants (root parameter 4, b2 register) - moved to slot 4
        float lightDirData[3] = {m_lightDir.x, m_lightDir.y, m_lightDir.z};
        m_commandList->SetGraphicsRoot32BitConstants(4, 3, lightDirData, 0);
        
        static int lightDebug = 0;
        if (lightDebug < 3) {
            std::printf("D3D12: Pushing lightDir=(%f, %f, %f)\n", 
                       m_lightDir.x, m_lightDir.y, m_lightDir.z);
            lightDebug++;
        }
        
        // Push useTexture as root constant (root parameter 5, b3 register) - moved to slot 5
        float useTextureValue = (textureHandle > 0) ? 1.0f : 0.0f;
        m_commandList->SetGraphicsRoot32BitConstants(5, 1, &useTextureValue, 0);
        
        // Push useNormalMap as root constant (root parameter 6, b4 register) - moved to slot 6
        m_commandList->SetGraphicsRoot32BitConstants(6, 1, &m_useNormalMap, 0);

        // With separate descriptor tables, we can bind directly to each texture's SRV!
        // No copying needed - just point to the actual SRV indices
        
        D3D12_GPU_DESCRIPTOR_HANDLE srvGpuBase = m_cbvSrvHeap->GetGPUDescriptorHandleForHeapStart();
        
        // Bind diffuse texture table (root parameter 1, t0)
        if (textureHandle > 0) {
            auto texIt = m_textures.find(textureHandle);
            if (texIt != m_textures.end()) {
                D3D12_GPU_DESCRIPTOR_HANDLE diffuseGpu = srvGpuBase;
                diffuseGpu.ptr += texIt->second.srvDescriptorIndex * m_cbvSrvDescriptorSize;
                m_commandList->SetGraphicsRootDescriptorTable(1, diffuseGpu);
            } else {
                // Bind dummy at slot 1
                D3D12_GPU_DESCRIPTOR_HANDLE dummyGpu = srvGpuBase;
                dummyGpu.ptr += 1 * m_cbvSrvDescriptorSize;
                m_commandList->SetGraphicsRootDescriptorTable(1, dummyGpu);
            }
        } else {
            // Bind dummy at slot 1
            D3D12_GPU_DESCRIPTOR_HANDLE dummyGpu = srvGpuBase;
            dummyGpu.ptr += 1 * m_cbvSrvDescriptorSize;
            m_commandList->SetGraphicsRootDescriptorTable(1, dummyGpu);
        }
        
        // Bind normal map table (root parameter 2, t1)
        if (m_boundNormalMap > 0) {
            auto normalTexIt = m_textures.find(m_boundNormalMap);
            if (normalTexIt != m_textures.end()) {
                D3D12_GPU_DESCRIPTOR_HANDLE normalGpu = srvGpuBase;
                normalGpu.ptr += normalTexIt->second.srvDescriptorIndex * m_cbvSrvDescriptorSize;
                m_commandList->SetGraphicsRootDescriptorTable(2, normalGpu);
            } else {
                // Bind dummy
                D3D12_GPU_DESCRIPTOR_HANDLE dummyGpu = srvGpuBase;
                dummyGpu.ptr += 1 * m_cbvSrvDescriptorSize;
                m_commandList->SetGraphicsRootDescriptorTable(2, dummyGpu);
            }
        } else {
            // Bind dummy
            D3D12_GPU_DESCRIPTOR_HANDLE dummyGpu = srvGpuBase;
            dummyGpu.ptr += 1 * m_cbvSrvDescriptorSize;
            m_commandList->SetGraphicsRootDescriptorTable(2, dummyGpu);
        }

        D3D12Mesh& mesh = meshIt->second;
        m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_commandList->IASetVertexBuffers(0, 1, &mesh.vertexBufferView);
        m_commandList->IASetIndexBuffer(&mesh.indexBufferView);
        m_commandList->DrawIndexedInstanced(mesh.indexCount, 1, 0, 0, 0);
    }
    
    // Instanced drawing
    void drawMeshInstanced(uint32_t meshHandle, uint32_t textureHandle,
                          const InstanceData* instances, uint32_t instanceCount) override {
        if (instanceCount == 0 || !instances) return;
        if (!m_hasViewProj) {
            printf("D3D12: drawMeshInstanced called but no viewProj stored!\n");
            return;
        }
        
        auto shIt = m_shaders.find(m_currentShader);
        if (shIt == m_shaders.end()) return;
        
        static int debugCount = 0;
        static int totalDraws = 0;
        if (debugCount < 5) {
            printf("D3D12 drawMeshInstanced: mesh=%u, tex=%u, count=%u, useTexture will be=%f\n", 
                   meshHandle, textureHandle, instanceCount, (textureHandle > 0) ? 1.0f : 0.0f);
            printf("  ViewProj stored: %d\n", m_hasViewProj ? 1 : 0);
            debugCount++;
        }
        
        // Set flag to prevent viewProj from being overwritten
        m_inInstancedDraw = true;
        
        // Fallback implementation: draw one at a time using stored viewProj
        // CRITICAL: D3D12 has one CB per frame, so we can't use drawMesh which shares the CB
        // We must inline everything and hope the GPU reads CB values at draw time (it doesn't!)
        
        static bool printedOnce = false;
        if (!printedOnce && instanceCount > 0) {
            printf("D3D12 drawMeshInstanced DETAILED DEBUG:\n");
            printf("  Mesh: %u, Texture: %u, InstanceCount: %u\n", meshHandle, textureHandle, instanceCount);
            printf("  Frame index: %u\n", m_frameIndex);
            printf("  CB address: %p\n", m_cbvDataBegin[m_frameIndex]);
            printf("  First instance world[12]=%f (X position)\n", instances[0].worldMatrix.m[12]);
            printf("  Last instance world[12]=%f (X position)\n", instances[instanceCount-1].worldMatrix.m[12]);
            printedOnce = true;
        }
        
        auto meshIt = m_meshes.find(meshHandle);
        if (meshIt == m_meshes.end()) {
            printf("D3D12: Mesh %u not found!\n", meshHandle);
            m_inInstancedDraw = false;
            return;
        }
        D3D12Mesh& mesh = meshIt->second;
        
        for (uint32_t i = 0; i < instanceCount; i++) {
            // World matrix from instance data needs to be transposed to match viewProj
            XMMATRIX world = Mat4ToXM(instances[i].worldMatrix);
            world = XMMatrixTranspose(world);
            
            // Both world and viewProj are transposed
            // Multiply in correct order: viewProj * world (both already transposed)
            XMMATRIX mvp = XMMatrixMultiply(m_viewProj, world);
            
            // Update constant buffer with world matrix (b0)
            shIt->second.cbData.world = world;
            
            memcpy(m_cbvDataBegin[m_frameIndex], &shIt->second.cbData, sizeof(CBData));
            
            // Bind CB (b0)
            m_commandList->SetGraphicsRootConstantBufferView(0,
                m_constantBuffers[m_frameIndex]->GetGPUVirtualAddress());
            
            // Push MVP as root constants (root parameter 3, b1) - updated from 2 to 3
            XMFLOAT4X4 mvpData;
            XMStoreFloat4x4(&mvpData, mvp);
            m_commandList->SetGraphicsRoot32BitConstants(3, 16, &mvpData, 0);
            
            // Push lightDir as root constants (root parameter 4, b2) - updated from 3 to 4
            float lightDirData[3] = {m_lightDir.x, m_lightDir.y, m_lightDir.z};
            m_commandList->SetGraphicsRoot32BitConstants(4, 3, lightDirData, 0);
            
            // Push useTexture as root constant (root parameter 5, b3) - updated from 4 to 5
            float useTextureValue = (textureHandle > 0) ? 1.0f : 0.0f;
            m_commandList->SetGraphicsRoot32BitConstants(5, 1, &useTextureValue, 0);
            
            // Push useNormalMap as root constant (root parameter 6, b4) - updated from 5 to 6
            m_commandList->SetGraphicsRoot32BitConstants(6, 1, &m_useNormalMap, 0);
            
            // Bind 2 separate descriptor tables - no copying!
            D3D12_GPU_DESCRIPTOR_HANDLE srvGpuBase = m_cbvSrvHeap->GetGPUDescriptorHandleForHeapStart();
            
            // Bind diffuse (root parameter 1)
            if (textureHandle > 0) {
                auto texIt = m_textures.find(textureHandle);
                if (texIt != m_textures.end()) {
                    D3D12_GPU_DESCRIPTOR_HANDLE diffuseGpu = srvGpuBase;
                    diffuseGpu.ptr += texIt->second.srvDescriptorIndex * m_cbvSrvDescriptorSize;
                    m_commandList->SetGraphicsRootDescriptorTable(1, diffuseGpu);
                } else {
                    D3D12_GPU_DESCRIPTOR_HANDLE dummyGpu = srvGpuBase;
                    dummyGpu.ptr += 1 * m_cbvSrvDescriptorSize;
                    m_commandList->SetGraphicsRootDescriptorTable(1, dummyGpu);
                }
            } else {
                D3D12_GPU_DESCRIPTOR_HANDLE dummyGpu = srvGpuBase;
                dummyGpu.ptr += 1 * m_cbvSrvDescriptorSize;
                m_commandList->SetGraphicsRootDescriptorTable(1, dummyGpu);
            }
            
            // Bind normal map (root parameter 2)
            if (m_boundNormalMap > 0) {
                auto normalTexIt = m_textures.find(m_boundNormalMap);
                if (normalTexIt != m_textures.end()) {
                    D3D12_GPU_DESCRIPTOR_HANDLE normalGpu = srvGpuBase;
                    normalGpu.ptr += normalTexIt->second.srvDescriptorIndex * m_cbvSrvDescriptorSize;
                    m_commandList->SetGraphicsRootDescriptorTable(2, normalGpu);
                } else {
                    D3D12_GPU_DESCRIPTOR_HANDLE dummyGpu = srvGpuBase;
                    dummyGpu.ptr += 1 * m_cbvSrvDescriptorSize;
                    m_commandList->SetGraphicsRootDescriptorTable(2, dummyGpu);
                }
            } else {
                D3D12_GPU_DESCRIPTOR_HANDLE dummyGpu = srvGpuBase;
                dummyGpu.ptr += 1 * m_cbvSrvDescriptorSize;
                m_commandList->SetGraphicsRootDescriptorTable(2, dummyGpu);
            }
            
            // Draw this instance
            m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            m_commandList->IASetVertexBuffers(0, 1, &mesh.vertexBufferView);
            m_commandList->IASetIndexBuffer(&mesh.indexBufferView);
            m_commandList->DrawIndexedInstanced(mesh.indexCount, 1, 0, 0, 0);
            
            totalDraws++;
        }
        
        static bool printedTotal = false;
        if (totalDraws >= 800 && !printedTotal) {
            printf("D3D12: Total draws = %d\n", totalDraws);
            printedTotal = true;
        }
        
        m_inInstancedDraw = false;
    }

    void setDepthTest(bool enable) override { m_depthTestEnabled = enable; }
    void setCulling(bool enable) override { m_cullingEnabled = enable; }
};

// ==================== Factory ====================
IRenderer* createD3D12Renderer() {
    return new D3D12Renderer();
}
