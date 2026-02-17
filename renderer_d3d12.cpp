// renderer_d3d12.cpp - Direct3D 12 implementation
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// Include GLFW before defining GLFW_EXPOSE_NATIVE_WIN32
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

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

using Microsoft::WRL::ComPtr;
using namespace DirectX;

#include "stb_image.h"

static const UINT FRAME_COUNT = 2;

// ==================== D3D12 Texture ====================
struct D3D12Texture {
    ComPtr<ID3D12Resource> resource;
    D3D12_CPU_DESCRIPTOR_HANDLE srvHandle;
    int width;
    int height;
};

// ==================== Helper Functions (instead of d3dx12.h) ====================
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
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = resource;
    barrier.Transition.StateBefore = stateBefore;
    barrier.Transition.StateAfter = stateAfter;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    return barrier;
}

inline D3D12_HEAP_PROPERTIES DefaultHeapProps() {
    D3D12_HEAP_PROPERTIES props = {};
    props.Type = D3D12_HEAP_TYPE_DEFAULT;
    props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    props.CreationNodeMask = 1;
    props.VisibleNodeMask = 1;
    return props;
}

inline D3D12_HEAP_PROPERTIES UploadHeapProps() {
    D3D12_HEAP_PROPERTIES props = {};
    props.Type = D3D12_HEAP_TYPE_UPLOAD;
    props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    props.CreationNodeMask = 1;
    props.VisibleNodeMask = 1;
    return props;
}

inline D3D12_RESOURCE_DESC BufferDesc(UINT64 size) {
    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Alignment = 0;
    desc.Width = size;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    desc.Flags = D3D12_RESOURCE_FLAG_NONE;
    return desc;
}

inline D3D12_RESOURCE_DESC Tex2DDesc(DXGI_FORMAT format, UINT width, UINT height, 
                                    D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE) {
    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Alignment = 0;
    desc.Width = width;
    desc.Height = height;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags = flags;
    return desc;
}

inline D3D12_RASTERIZER_DESC DefaultRasterizerDesc() {
    D3D12_RASTERIZER_DESC desc = {};
    desc.FillMode = D3D12_FILL_MODE_SOLID;
    desc.CullMode = D3D12_CULL_MODE_BACK;
    desc.FrontCounterClockwise = FALSE;
    desc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
    desc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    desc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    desc.DepthClipEnable = TRUE;
    desc.MultisampleEnable = FALSE;
    desc.AntialiasedLineEnable = FALSE;
    desc.ForcedSampleCount = 0;
    desc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
    return desc;
}

inline D3D12_BLEND_DESC DefaultBlendDesc() {
    D3D12_BLEND_DESC desc = {};
    desc.AlphaToCoverageEnable = FALSE;
    desc.IndependentBlendEnable = FALSE;
    const D3D12_RENDER_TARGET_BLEND_DESC defaultRenderTargetBlendDesc = {
        FALSE, FALSE,
        D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
        D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
        D3D12_LOGIC_OP_NOOP,
        D3D12_COLOR_WRITE_ENABLE_ALL,
    };
    for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
        desc.RenderTarget[i] = defaultRenderTargetBlendDesc;
    return desc;
}

inline D3D12_DEPTH_STENCIL_DESC DefaultDepthStencilDesc() {
    D3D12_DEPTH_STENCIL_DESC desc = {};
    desc.DepthEnable = TRUE;
    desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    desc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    desc.StencilEnable = FALSE;
    desc.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
    desc.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
    const D3D12_DEPTH_STENCILOP_DESC defaultStencilOp = {
        D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS
    };
    desc.FrontFace = defaultStencilOp;
    desc.BackFace = defaultStencilOp;
    return desc;
}

inline D3D12_SHADER_BYTECODE ShaderBytecode(ID3DBlob* blob) {
    D3D12_SHADER_BYTECODE sbc = {};
    sbc.pShaderBytecode = blob->GetBufferPointer();
    sbc.BytecodeLength = blob->GetBufferSize();
    return sbc;
}

inline D3D12_CPU_DESCRIPTOR_HANDLE CPUDescriptorHandle(
    D3D12_CPU_DESCRIPTOR_HANDLE base,
    INT offsetInDescriptors,
    UINT descriptorIncrementSize)
{
    D3D12_CPU_DESCRIPTOR_HANDLE handle;
    handle.ptr = base.ptr + (SIZE_T)offsetInDescriptors * descriptorIncrementSize;
    return handle;
}

// ==================== D3D12 Mesh ====================
struct D3D12Mesh {
    ComPtr<ID3D12Resource> vertexBuffer;
    ComPtr<ID3D12Resource> indexBuffer;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
    D3D12_INDEX_BUFFER_VIEW indexBufferView;
    uint32_t indexCount;
};

// ==================== Constant Buffer Structure ====================
struct CBData {
    XMMATRIX mvp;     // transposed
    XMMATRIX world;   // transposed
    XMFLOAT3 lightDir;
    float pad0;
};

// ==================== D3D12 Shader ====================
struct D3D12Shader {
    ComPtr<ID3D12RootSignature> rootSignature;
    ComPtr<ID3D12PipelineState> pipelineState;
    CBData cbData; // Store constant buffer data for batch update
};

// ==================== D3D12 Renderer ====================
class D3D12Renderer : public IRenderer {
private:
    HWND m_hwnd;
    
    // Core D3D12 objects
    ComPtr<ID3D12Device> m_device;
    ComPtr<ID3D12CommandQueue> m_commandQueue;
    ComPtr<IDXGISwapChain3> m_swapChain;
    ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
    ComPtr<ID3D12DescriptorHeap> m_cbvHeap;
    
    // Frame resources
    ComPtr<ID3D12Resource> m_renderTargets[FRAME_COUNT];
    ComPtr<ID3D12Resource> m_depthStencil;
    ComPtr<ID3D12CommandAllocator> m_commandAllocators[FRAME_COUNT];
    ComPtr<ID3D12GraphicsCommandList> m_commandList;
    
    // Synchronization
    ComPtr<ID3D12Fence> m_fence;
    UINT64 m_fenceValues[FRAME_COUNT];
    HANDLE m_fenceEvent;
    UINT m_frameIndex;
    
    // Constant buffer
    ComPtr<ID3D12Resource> m_constantBuffer;
    UINT8* m_cbvDataBegin;
    
    UINT m_rtvDescriptorSize;
    UINT m_dsvDescriptorSize;
    UINT m_cbvDescriptorSize;
    
    std::unordered_map<uint32_t, D3D12Mesh> m_meshes;
    std::unordered_map<uint32_t, D3D12Shader> m_shaders;
    uint32_t m_nextMeshHandle;
    uint32_t m_nextShaderHandle;
    uint32_t m_currentShader;
    
    int m_width;
    int m_height;
    
    float m_clearColor[4];
    bool m_depthTestEnabled;
    bool m_cullingEnabled;

    HWND getHWND(GLFWwindow* window) {
        return glfwGetWin32Window(window);
    }

    void waitForGpu() {
        // Don't wait if resources are already released
        if (!m_commandQueue || !m_fence || !m_fenceEvent) {
            return;
        }

        const UINT64 fence = m_fenceValues[m_frameIndex];
        HRESULT hr = m_commandQueue->Signal(m_fence.Get(), fence);
        if (FAILED(hr)) {
            return; // Already shutting down
        }
        
        m_fence->SetEventOnCompletion(fence, m_fenceEvent);
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }

    void moveToNextFrame() {
        const UINT64 currentFenceValue = m_fenceValues[m_frameIndex];
        m_commandQueue->Signal(m_fence.Get(), currentFenceValue);

        m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

        if (m_fence->GetCompletedValue() < m_fenceValues[m_frameIndex]) {
            m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent);
            WaitForSingleObject(m_fenceEvent, INFINITE);
        }

        m_fenceValues[m_frameIndex] = currentFenceValue + 1;
    }

    bool compileShader(const char* src, const char* entry, const char* target, 
                      ComPtr<ID3DBlob>& outBlob) {
        UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
        flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
        ComPtr<ID3DBlob> errorBlob;
        HRESULT hr = D3DCompile(src, std::strlen(src), nullptr, nullptr, nullptr,
                               entry, target, flags, 0,
                               outBlob.GetAddressOf(), errorBlob.GetAddressOf());
        if (FAILED(hr)) {
            if (errorBlob) {
                std::fprintf(stderr, "Shader compile error: %s\n", 
                           (const char*)errorBlob->GetBufferPointer());
            }
            return false;
        }
        return true;
    }

    std::string glslToHLSL(const char* glslVS, const char* glslFS) {
        return R"(
cbuffer CB : register(b0)
{
    float4x4 uMVP;
    float4x4 uWorld;
    float3   uLightDir;
    float    pad0;
};

struct VSIn {
    float3 aPos : POSITION;
    float3 aNrm : NORMAL;
    float4 aCol : COLOR;
};

struct VSOut {
    float4 pos : SV_Position;
    float3 nrmW : TEXCOORD0;
    float4 col : COLOR;
};

VSOut VSMain(VSIn v)
{
    VSOut o;
    o.pos = mul(float4(v.aPos, 1.0), uMVP);
    
    float3 n = mul(float4(v.aNrm, 0.0), uWorld).xyz;
    o.nrmW = normalize(n);
    
    o.col = v.aCol;
    return o;
}

float4 PSMain(VSOut i) : SV_Target
{
    float3 L = normalize(-uLightDir);
    float ndl = saturate(dot(normalize(i.nrmW), L));
    float ambient = 0.18;
    float diff = ambient + ndl * 0.82;
    
    return float4(i.col.rgb * diff, 1.0);
}
)";
    }

    void createRenderTargets() {
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();

        for (UINT i = 0; i < FRAME_COUNT; i++) {
            m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i]));
            m_device->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, rtvHandle);
            rtvHandle = CPUDescriptorHandle(rtvHandle, 1, m_rtvDescriptorSize);
        }
    }

    void createDepthStencil() {
        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

        D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
        depthOptimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
        depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
        depthOptimizedClearValue.DepthStencil.Stencil = 0;

        D3D12_HEAP_PROPERTIES heapProps = DefaultHeapProps();
        D3D12_RESOURCE_DESC resourceDesc = Tex2DDesc(
            DXGI_FORMAT_D32_FLOAT, m_width, m_height,
            D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

        m_device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            &depthOptimizedClearValue,
            IID_PPV_ARGS(&m_depthStencil));

        m_device->CreateDepthStencilView(m_depthStencil.Get(), &dsvDesc,
            m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
    }

public:
    D3D12Renderer()
        : m_hwnd(nullptr)
        , m_frameIndex(0)
        , m_rtvDescriptorSize(0)
        , m_dsvDescriptorSize(0)
        , m_cbvDescriptorSize(0)
        , m_fenceEvent(nullptr)
        , m_cbvDataBegin(nullptr)
        , m_nextMeshHandle(1)
        , m_nextShaderHandle(1)
        , m_currentShader(0)
        , m_width(1280)
        , m_height(720)
        , m_depthTestEnabled(true)
        , m_cullingEnabled(false)
    {
        m_clearColor[0] = 0.0f;
        m_clearColor[1] = 0.0f;
        m_clearColor[2] = 0.0f;
        m_clearColor[3] = 1.0f;
        
        for (UINT i = 0; i < FRAME_COUNT; i++) {
            m_fenceValues[i] = 0;
        }
    }

    virtual ~D3D12Renderer() {
        shutdown();
    }

    bool initialize(GLFWwindow* window) override {
        m_hwnd = getHWND(window);
        if (!m_hwnd) {
            std::fprintf(stderr, "Failed to get HWND from GLFW window\n");
            return false;
        }

        glfwGetFramebufferSize(window, &m_width, &m_height);

        UINT dxgiFactoryFlags = 0;

#ifdef _DEBUG
        // Enable debug layer
        ComPtr<ID3D12Debug> debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
            debugController->EnableDebugLayer();
            dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
        }
#endif

        // Create factory
        ComPtr<IDXGIFactory4> factory;
        CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory));

        // Create device
        HRESULT hr = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device));
        if (FAILED(hr)) {
            std::fprintf(stderr, "Failed to create D3D12 device\n");
            return false;
        }

        // Create command queue
        D3D12_COMMAND_QUEUE_DESC queueDesc = {};
        queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

        hr = m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue));
        if (FAILED(hr)) {
            std::fprintf(stderr, "Failed to create command queue\n");
            return false;
        }

        // Create swap chain
        DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
        swapChainDesc.BufferCount = FRAME_COUNT;
        swapChainDesc.Width = m_width;
        swapChainDesc.Height = m_height;
        swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swapChainDesc.SampleDesc.Count = 1;

        ComPtr<IDXGISwapChain1> swapChain;
        hr = factory->CreateSwapChainForHwnd(
            m_commandQueue.Get(),
            m_hwnd,
            &swapChainDesc,
            nullptr,
            nullptr,
            &swapChain);

        if (FAILED(hr)) {
            std::fprintf(stderr, "Failed to create swap chain\n");
            return false;
        }

        factory->MakeWindowAssociation(m_hwnd, DXGI_MWA_NO_ALT_ENTER);
        swapChain.As(&m_swapChain);
        m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

        // Create descriptor heaps
        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
        rtvHeapDesc.NumDescriptors = FRAME_COUNT;
        rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap));

        D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
        dsvHeapDesc.NumDescriptors = 1;
        dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        m_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_dsvHeap));

        D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc = {};
        cbvHeapDesc.NumDescriptors = 1;
        cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        m_device->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&m_cbvHeap));

        m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        m_dsvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
        m_cbvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        createRenderTargets();
        createDepthStencil();

        // Create command allocators
        for (UINT i = 0; i < FRAME_COUNT; i++) {
            m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, 
                IID_PPV_ARGS(&m_commandAllocators[i]));
        }

        // Create command list
        m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
            m_commandAllocators[m_frameIndex].Get(), nullptr, IID_PPV_ARGS(&m_commandList));
        m_commandList->Close();

        // Create constant buffer
        const UINT constantBufferSize = CalcConstantBufferByteSize(sizeof(CBData));
        D3D12_HEAP_PROPERTIES heapProps = UploadHeapProps();
        D3D12_RESOURCE_DESC bufferDesc = BufferDesc(constantBufferSize);
        
        m_device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &bufferDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_constantBuffer));

        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
        cbvDesc.BufferLocation = m_constantBuffer->GetGPUVirtualAddress();
        cbvDesc.SizeInBytes = constantBufferSize;
        m_device->CreateConstantBufferView(&cbvDesc, m_cbvHeap->GetCPUDescriptorHandleForHeapStart());

        D3D12_RANGE readRange = { 0, 0 };
        m_constantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_cbvDataBegin));

        // Create fence
        m_device->CreateFence(m_fenceValues[m_frameIndex], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));
        m_fenceValues[m_frameIndex]++;

        m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (m_fenceEvent == nullptr) {
            return false;
        }

        std::printf("Direct3D 12 Renderer initialized\n");
        return true;
    }

    void shutdown() override {
        // Wait for GPU to finish all work before destroying resources
        // Do this BEFORE clearing any resources
        if (m_commandQueue && m_fence && m_fenceEvent) {
            waitForGpu();
        }

        // Unmap constant buffer
        if (m_constantBuffer) {
            m_constantBuffer->Unmap(0, nullptr);
            m_cbvDataBegin = nullptr;
        }

        // Clear application resources (this releases COM objects)
        // These need to be cleared while fence/queue are still valid
        m_meshes.clear();
        m_shaders.clear();

        // Release constant buffer
        m_constantBuffer.Reset();

        // Release command list (must be before command allocators)
        m_commandList.Reset();

        // Release frame resources
        for (UINT i = 0; i < FRAME_COUNT; i++) {
            m_renderTargets[i].Reset();
            m_commandAllocators[i].Reset();
        }

        // Release depth stencil
        m_depthStencil.Reset();

        // Release descriptor heaps
        m_cbvHeap.Reset();
        m_dsvHeap.Reset();
        m_rtvHeap.Reset();

        // Release swap chain
        m_swapChain.Reset();

        // Release fence BEFORE closing the event
        m_fence.Reset();

        // Close fence event handle
        if (m_fenceEvent) {
            CloseHandle(m_fenceEvent);
            m_fenceEvent = nullptr;
        }

        // Release command queue
        m_commandQueue.Reset();

        // Release device last
        m_device.Reset();
    }

    void beginFrame() override {
        m_commandAllocators[m_frameIndex]->Reset();
        m_commandList->Reset(m_commandAllocators[m_frameIndex].Get(), nullptr);

        D3D12_RESOURCE_BARRIER barrier = TransitionBarrier(
            m_renderTargets[m_frameIndex].Get(),
            D3D12_RESOURCE_STATE_PRESENT,
            D3D12_RESOURCE_STATE_RENDER_TARGET);
        m_commandList->ResourceBarrier(1, &barrier);

        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = CPUDescriptorHandle(
            m_rtvHeap->GetCPUDescriptorHandleForHeapStart(),
            m_frameIndex, m_rtvDescriptorSize);
        D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();

        m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);
        m_commandList->ClearRenderTargetView(rtvHandle, m_clearColor, 0, nullptr);
        m_commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

        D3D12_VIEWPORT viewport = { 0.0f, 0.0f, (float)m_width, (float)m_height, 0.0f, 1.0f };
        D3D12_RECT scissorRect = { 0, 0, (LONG)m_width, (LONG)m_height };
        m_commandList->RSSetViewports(1, &viewport);
        m_commandList->RSSetScissorRects(1, &scissorRect);
    }

    void endFrame() override {
        D3D12_RESOURCE_BARRIER barrier = TransitionBarrier(
            m_renderTargets[m_frameIndex].Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PRESENT);
        m_commandList->ResourceBarrier(1, &barrier);

        m_commandList->Close();

        ID3D12CommandList* cmdLists[] = { m_commandList.Get() };
        m_commandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

        m_swapChain->Present(1, 0);
        moveToNextFrame();
    }

    void setClearColor(float r, float g, float b, float a) override {
        m_clearColor[0] = r;
        m_clearColor[1] = g;
        m_clearColor[2] = b;
        m_clearColor[3] = a;
    }

    void setViewport(int width, int height) override {
        waitForGpu();

        for (UINT i = 0; i < FRAME_COUNT; i++) {
            m_renderTargets[i].Reset();
            m_fenceValues[i] = m_fenceValues[m_frameIndex];
        }

        m_width = width;
        m_height = height;

        DXGI_SWAP_CHAIN_DESC desc = {};
        m_swapChain->GetDesc(&desc);
        m_swapChain->ResizeBuffers(FRAME_COUNT, width, height, desc.BufferDesc.Format, desc.Flags);

        m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

        createRenderTargets();
        
        m_depthStencil.Reset();
        createDepthStencil();
    }

    uint32_t createMesh(const Vertex* vertices, uint32_t vertexCount,
                       const uint16_t* indices, uint32_t indexCount) override {
        D3D12Mesh mesh;
        mesh.indexCount = indexCount;

        const UINT vertexBufferSize = vertexCount * sizeof(Vertex);
        const UINT indexBufferSize = indexCount * sizeof(uint16_t);

        // Create vertex buffer
        D3D12_HEAP_PROPERTIES uploadHeapProps = UploadHeapProps();
        D3D12_RESOURCE_DESC vbDesc = BufferDesc(vertexBufferSize);
        
        m_device->CreateCommittedResource(
            &uploadHeapProps,
            D3D12_HEAP_FLAG_NONE,
            &vbDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&mesh.vertexBuffer));

        UINT8* pVertexDataBegin;
        D3D12_RANGE readRange = { 0, 0 };
        mesh.vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin));
        memcpy(pVertexDataBegin, vertices, vertexBufferSize);
        mesh.vertexBuffer->Unmap(0, nullptr);

        mesh.vertexBufferView.BufferLocation = mesh.vertexBuffer->GetGPUVirtualAddress();
        mesh.vertexBufferView.StrideInBytes = sizeof(Vertex);
        mesh.vertexBufferView.SizeInBytes = vertexBufferSize;

        // Create index buffer
        D3D12_RESOURCE_DESC ibDesc = BufferDesc(indexBufferSize);
        
        m_device->CreateCommittedResource(
            &uploadHeapProps,
            D3D12_HEAP_FLAG_NONE,
            &ibDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&mesh.indexBuffer));

        UINT8* pIndexDataBegin;
        mesh.indexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pIndexDataBegin));
        memcpy(pIndexDataBegin, indices, indexBufferSize);
        mesh.indexBuffer->Unmap(0, nullptr);

        mesh.indexBufferView.BufferLocation = mesh.indexBuffer->GetGPUVirtualAddress();
        mesh.indexBufferView.Format = DXGI_FORMAT_R16_UINT;
        mesh.indexBufferView.SizeInBytes = indexBufferSize;

        uint32_t handle = m_nextMeshHandle++;
        m_meshes[handle] = mesh;
        return handle;
    }

    void destroyMesh(uint32_t meshHandle) override {
        auto it = m_meshes.find(meshHandle);
        if (it != m_meshes.end()) {
            // Wait for GPU to finish using this mesh before destroying it
            waitForGpu();
            m_meshes.erase(it);
        }
    }

    uint32_t createShader(const char* vertexSource, const char* fragmentSource) override {
        std::string hlsl = glslToHLSL(vertexSource, fragmentSource);

        D3D12Shader shader;

        // Compile shaders
        ComPtr<ID3DBlob> vsBlob, psBlob;
        if (!compileShader(hlsl.c_str(), "VSMain", "vs_5_1", vsBlob) ||
            !compileShader(hlsl.c_str(), "PSMain", "ps_5_1", psBlob)) {
            return 0;
        }

        // Create root signature
        D3D12_ROOT_PARAMETER rootParameters[1] = {};
        rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        rootParameters[0].Descriptor.ShaderRegister = 0;
        rootParameters[0].Descriptor.RegisterSpace = 0;
        rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
        rootSignatureDesc.NumParameters = _countof(rootParameters);
        rootSignatureDesc.pParameters = rootParameters;
        rootSignatureDesc.NumStaticSamplers = 0;
        rootSignatureDesc.pStaticSamplers = nullptr;
        rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        ComPtr<ID3DBlob> signature, error;
        D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
        m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
            IID_PPV_ARGS(&shader.rootSignature));

        // Input layout
        D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };

        // PSO
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.InputLayout.pInputElementDescs = inputLayout;
        psoDesc.InputLayout.NumElements = _countof(inputLayout);
        psoDesc.pRootSignature = shader.rootSignature.Get();
        psoDesc.VS = ShaderBytecode(vsBlob.Get());
        psoDesc.PS = ShaderBytecode(psBlob.Get());
        
        psoDesc.RasterizerState = DefaultRasterizerDesc();
        psoDesc.RasterizerState.CullMode = m_cullingEnabled ? D3D12_CULL_MODE_BACK : D3D12_CULL_MODE_NONE;
        psoDesc.RasterizerState.FrontCounterClockwise = TRUE;
        
        psoDesc.BlendState = DefaultBlendDesc();
        
        psoDesc.DepthStencilState = DefaultDepthStencilDesc();
        psoDesc.DepthStencilState.DepthEnable = m_depthTestEnabled;
        
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        psoDesc.SampleDesc.Count = 1;

        HRESULT hr = m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&shader.pipelineState));
        if (FAILED(hr)) {
            std::fprintf(stderr, "Failed to create pipeline state\n");
            return 0;
        }

        // Initialize cbData
        shader.cbData.mvp = XMMatrixIdentity();
        shader.cbData.world = XMMatrixIdentity();
        shader.cbData.lightDir = XMFLOAT3(0.0f, -1.0f, 0.0f);
        shader.cbData.pad0 = 0.0f;

        uint32_t handle = m_nextShaderHandle++;
        m_shaders[handle] = shader;
        return handle;
    }

    void destroyShader(uint32_t shaderHandle) override {
        auto it = m_shaders.find(shaderHandle);
        if (it != m_shaders.end()) {
            // Wait for GPU to finish using this shader before destroying it
            waitForGpu();
            m_shaders.erase(it);
        }
    }

    void useShader(uint32_t shaderHandle) override {
        auto it = m_shaders.find(shaderHandle);
        if (it != m_shaders.end()) {
            m_currentShader = shaderHandle;
            D3D12Shader& shader = it->second;
            m_commandList->SetPipelineState(shader.pipelineState.Get());
            m_commandList->SetGraphicsRootSignature(shader.rootSignature.Get());
        }
    }

    void setUniformMat4(uint32_t shaderHandle, const char* name, const Mat4& matrix) override {
        auto it = m_shaders.find(shaderHandle);
        if (it == m_shaders.end()) return;

        D3D12Shader& shader = it->second;

        XMMATRIX xm = XMLoadFloat4x4((const XMFLOAT4X4*)matrix.m);
        xm = XMMatrixTranspose(xm);

        if (strcmp(name, "uMVP") == 0) {
            shader.cbData.mvp = xm;
        } else if (strcmp(name, "uWorld") == 0) {
            shader.cbData.world = xm;
        }
    }

    void setUniformVec3(uint32_t shaderHandle, const char* name, const Vec3& vec) override {
        auto it = m_shaders.find(shaderHandle);
        if (it == m_shaders.end()) return;

        D3D12Shader& shader = it->second;

        if (strcmp(name, "uLightDir") == 0) {
            shader.cbData.lightDir = XMFLOAT3(vec.x, vec.y, vec.z);
        }
    }

    uint32_t createTexture(const char* filepath) override {
        // TODO: D3D12 texture loading requires descriptor heap setup
        // For now, just log and return 0
        std::fprintf(stderr, "D3D12 texture loading TODO: %s\n", filepath);
        return 0;
    }

    void destroyTexture(uint32_t textureHandle) override {
        (void)textureHandle;
    }

    void setUniformInt(uint32_t shaderHandle, const char* name, int value) override {
        (void)shaderHandle;
        (void)name;
        (void)value;
    }

    void drawMesh(uint32_t meshHandle, uint32_t textureHandle = 0) override {
        (void)textureHandle;
        auto meshIt = m_meshes.find(meshHandle);
        if (meshIt == m_meshes.end()) return;

        // Update constant buffer
        auto shaderIt = m_shaders.find(m_currentShader);
        if (shaderIt != m_shaders.end()) {
            D3D12Shader& shader = shaderIt->second;
            memcpy(m_cbvDataBegin, &shader.cbData, sizeof(CBData));
            m_commandList->SetGraphicsRootConstantBufferView(0, m_constantBuffer->GetGPUVirtualAddress());
        }

        D3D12Mesh& mesh = meshIt->second;

        m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_commandList->IASetVertexBuffers(0, 1, &mesh.vertexBufferView);
        m_commandList->IASetIndexBuffer(&mesh.indexBufferView);
        m_commandList->DrawIndexedInstanced(mesh.indexCount, 1, 0, 0, 0);
    }

    void setDepthTest(bool enable) override {
        m_depthTestEnabled = enable;
        // Note: This will take effect on next shader creation
    }

    void setCulling(bool enable) override {
        m_cullingEnabled = enable;
        // Note: This will take effect on next shader creation
    }
};

// ==================== Factory Function ====================
#ifdef _WIN32
IRenderer* createD3D12Renderer() {
    return new D3D12Renderer();
}
#endif
