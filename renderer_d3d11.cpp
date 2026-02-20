// renderer_d3d11.cpp - Direct3D 11 implementation
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// Include GLFW before defining GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#ifndef GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#endif
#include <GLFW/glfw3native.h>

#include "renderer.h"

#include <d3d11.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <wrl/client.h>

#include <cstdio>
#include <string>
#include <unordered_map>
#include <cstring>

#include "stb_image.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

using Microsoft::WRL::ComPtr;
using namespace DirectX;

// ==================== D3D11 Texture ====================
struct D3D11Texture {
    ComPtr<ID3D11Texture2D> texture;
    ComPtr<ID3D11ShaderResourceView> srv;
    int width;
    int height;
};

// ==================== D3D11 Mesh ====================
struct D3D11Mesh {
    ComPtr<ID3D11Buffer> vertexBuffer;
    ComPtr<ID3D11Buffer> indexBuffer;
    uint32_t indexCount;
};

// ==================== Constant Buffer Structure ====================
struct CBData {
    XMMATRIX mvp;       // transposed  — 64 bytes
    XMMATRIX world;     // transposed  — 64 bytes
    XMFLOAT3 lightDir;                // 12 bytes
    float    useTexture;              //  4 bytes  (1.0 = textured, 0.0 = vertex colour)
    float    useNormalMap;            //  4 bytes  (1.0 = use normal map, 0.0 = vertex normal)
    XMFLOAT3 padding;                 // 12 bytes (alignment)
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

// ==================== D3D11 Shader ====================
struct D3D11Shader {
    ComPtr<ID3D11VertexShader> vertexShader;
    ComPtr<ID3D11PixelShader> pixelShader;
    ComPtr<ID3D11InputLayout> inputLayout;
    ComPtr<ID3D11Buffer> constantBuffer;
    std::unordered_map<std::string, int> uniformOffsets; // Not used in simple impl
    CBData cbData; // Store constant buffer data for batch update
};

// ==================== D3D11 Renderer ====================
class D3D11Renderer : public IRenderer {
private:
    HWND m_hwnd;
    ComPtr<ID3D11Device> m_device;
    ComPtr<ID3D11DeviceContext> m_context;
    ComPtr<IDXGISwapChain> m_swapChain;
    ComPtr<ID3D11RenderTargetView> m_rtv;
    ComPtr<ID3D11Texture2D> m_depthTexture;
    ComPtr<ID3D11DepthStencilView> m_dsv;
    ComPtr<ID3D11RasterizerState> m_rasterizerState;
    ComPtr<ID3D11DepthStencilState> m_depthStencilState;
    ComPtr<ID3D11SamplerState> m_samplerState;
    
    std::unordered_map<uint32_t, D3D11Mesh> m_meshes;
    std::unordered_map<uint32_t, D3D11Shader> m_shaders;
    std::unordered_map<uint32_t, D3D11Texture> m_textures;
    uint32_t m_nextMeshHandle;
    uint32_t m_nextShaderHandle;
    uint32_t m_nextTextureHandle;
    uint32_t m_currentShader;
    
    int m_width;
    int m_height;
    
    float m_clearColor[4];
    
    // Store view-projection for instanced rendering
    XMMATRIX m_viewProj;
    bool m_hasViewProj;
    bool m_inInstancedDraw;
    
    // Store normal map binding for texture slot 1
    uint32_t m_boundNormalMap;

    // Helper to get HWND from GLFWwindow
    HWND getHWND(GLFWwindow* window) {
        return glfwGetWin32Window(window);
    }

    void createRTVAndDSV(UINT w, UINT h) {
        // Release old views
        m_rtv.Reset();
        m_dsv.Reset();
        m_depthTexture.Reset();

        // Get back buffer
        ComPtr<ID3D11Texture2D> backBuffer;
        HRESULT hr = m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D),
                                           reinterpret_cast<void**>(backBuffer.GetAddressOf()));
        if (FAILED(hr)) {
            std::fprintf(stderr, "Failed to get back buffer\n");
            return;
        }

        // Create RTV
        hr = m_device->CreateRenderTargetView(backBuffer.Get(), nullptr, m_rtv.GetAddressOf());
        if (FAILED(hr)) {
            std::fprintf(stderr, "Failed to create RTV\n");
            return;
        }

        // Create depth buffer
        D3D11_TEXTURE2D_DESC depthDesc{};
        depthDesc.Width = w;
        depthDesc.Height = h;
        depthDesc.MipLevels = 1;
        depthDesc.ArraySize = 1;
        depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        depthDesc.SampleDesc.Count = 1;
        depthDesc.Usage = D3D11_USAGE_DEFAULT;
        depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

        hr = m_device->CreateTexture2D(&depthDesc, nullptr, m_depthTexture.GetAddressOf());
        if (FAILED(hr)) {
            std::fprintf(stderr, "Failed to create depth texture\n");
            return;
        }

        hr = m_device->CreateDepthStencilView(m_depthTexture.Get(), nullptr, m_dsv.GetAddressOf());
        if (FAILED(hr)) {
            std::fprintf(stderr, "Failed to create DSV\n");
            return;
        }

        // Bind render targets
        ID3D11RenderTargetView* rtvs[] = { m_rtv.Get() };
        m_context->OMSetRenderTargets(1, rtvs, m_dsv.Get());

        // Set viewport
        D3D11_VIEWPORT vp{};
        vp.Width = (float)w;
        vp.Height = (float)h;
        vp.MinDepth = 0.0f;
        vp.MaxDepth = 1.0f;
        m_context->RSSetViewports(1, &vp);
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

    // Convert OpenGL GLSL to HLSL (simple conversion)
    std::string glslToHLSL(const char* glslVS, const char* glslFS) {
        (void)glslVS; (void)glslFS; // We use our own HLSL; GLSL source is ignored
        return R"(
cbuffer CB : register(b0)
{
    float4x4 uMVP;
    float4x4 uWorld;
    float3   uLightDir;
    float    uUseTexture;
    float    uUseNormalMap;  // Flag for normal mapping
    float3   padding;        // Padding for 16-byte alignment
};

Texture2D    gTex     : register(t0);
Texture2D    gNormalMap : register(t1);  // Normal map texture
SamplerState gSampler : register(s0);

struct VSIn {
    float3 aPos      : POSITION;
    float3 aNrm      : NORMAL;
    float4 aCol      : COLOR;
    float2 aTexCoord : TEXCOORD0;
    float3 aTangent  : TANGENT;
    float3 aBitangent: BITANGENT;
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
    
    // Transform tangent space vectors to world space
    float3 T = normalize(mul(float4(v.aTangent, 0.0), uWorld).xyz);
    float3 B = normalize(mul(float4(v.aBitangent, 0.0), uWorld).xyz);
    float3 N = normalize(mul(float4(v.aNrm, 0.0), uWorld).xyz);
    
    // Re-orthogonalize using Gram-Schmidt
    T = normalize(T - dot(T, N) * N);
    B = normalize(cross(N, T));
    
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
        
        // Transform to world space with orthogonalized TBN
        N = normalize(mul(tangentNormal, i.TBN));
    }
    
    // Lighting
    float3 L = normalize(-uLightDir);
    float ndl = saturate(dot(N, L));
    float diff = 0.40 + ndl * 0.60;  // High ambient for bright scene

    // Base color
    float4 baseColor = (uUseTexture > 0.5)
                       ? gTex.Sample(gSampler, i.texCoord)
                       : i.col;

    return float4(baseColor.rgb * diff, baseColor.a);
}
)";
    }

public:
    D3D11Renderer()
        : m_hwnd(nullptr)
        , m_nextMeshHandle(1)
        , m_nextShaderHandle(1)
        , m_nextTextureHandle(1)
        , m_currentShader(0)
        , m_width(1280)
        , m_height(720)
        , m_hasViewProj(false)
        , m_inInstancedDraw(false)
        , m_boundNormalMap(0)
    {
        m_clearColor[0] = 0.0f;
        m_clearColor[1] = 0.0f;
        m_clearColor[2] = 0.0f;
        m_clearColor[3] = 1.0f;
    }

    virtual ~D3D11Renderer() {
        shutdown();
    }

    bool initialize(GLFWwindow* window) override {
        m_hwnd = getHWND(window);
        if (!m_hwnd) {
            std::fprintf(stderr, "Failed to get HWND from GLFW window\n");
            return false;
        }

        // Get window size
        glfwGetFramebufferSize(window, &m_width, &m_height);

        // Create swap chain description
        DXGI_SWAP_CHAIN_DESC sd{};
        sd.BufferCount = 2;
        sd.BufferDesc.Width = m_width;
        sd.BufferDesc.Height = m_height;
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow = m_hwnd;
        sd.SampleDesc.Count = 1;
        sd.Windowed = TRUE;
        sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        UINT flags = 0;
#ifdef _DEBUG
        flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

        D3D_FEATURE_LEVEL featureLevel;
        const D3D_FEATURE_LEVEL levels[] = { 
            D3D_FEATURE_LEVEL_11_0, 
            D3D_FEATURE_LEVEL_10_0 
        };

        HRESULT hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
            flags,
            levels, (UINT)std::size(levels),
            D3D11_SDK_VERSION,
            &sd,
            m_swapChain.GetAddressOf(),
            m_device.GetAddressOf(),
            &featureLevel,
            m_context.GetAddressOf());

        if (FAILED(hr)) {
            std::fprintf(stderr, "Failed to create D3D11 device and swap chain\n");
            return false;
        }

        // Create render targets
        createRTVAndDSV(m_width, m_height);

        // Create default depth stencil state (depth testing enabled)
        D3D11_DEPTH_STENCIL_DESC dsDesc{};
        dsDesc.DepthEnable = TRUE;
        dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        dsDesc.DepthFunc = D3D11_COMPARISON_LESS;
        
        hr = m_device->CreateDepthStencilState(&dsDesc, m_depthStencilState.GetAddressOf());
        if (FAILED(hr)) {
            std::fprintf(stderr, "Failed to create depth stencil state\n");
            return false;
        }
        
        m_context->OMSetDepthStencilState(m_depthStencilState.Get(), 0);

        // Create rasterizer state (no culling by default)
        D3D11_RASTERIZER_DESC rsDesc{};
        rsDesc.FillMode = D3D11_FILL_SOLID;
        rsDesc.CullMode = D3D11_CULL_NONE;
        rsDesc.FrontCounterClockwise = TRUE;
        rsDesc.DepthClipEnable = TRUE;

        hr = m_device->CreateRasterizerState(&rsDesc, m_rasterizerState.GetAddressOf());
        if (FAILED(hr)) {
            std::fprintf(stderr, "Failed to create rasterizer state\n");
            return false;
        }

        m_context->RSSetState(m_rasterizerState.Get());

        // Create sampler state and bind it permanently to PS slot 0
        D3D11_SAMPLER_DESC sampDesc{};
        sampDesc.Filter         = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sampDesc.AddressU       = D3D11_TEXTURE_ADDRESS_WRAP;
        sampDesc.AddressV       = D3D11_TEXTURE_ADDRESS_WRAP;
        sampDesc.AddressW       = D3D11_TEXTURE_ADDRESS_WRAP;
        sampDesc.MaxAnisotropy  = 1;
        sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        sampDesc.MaxLOD         = D3D11_FLOAT32_MAX;
        hr = m_device->CreateSamplerState(&sampDesc, m_samplerState.GetAddressOf());
        if (FAILED(hr)) {
            std::fprintf(stderr, "Failed to create sampler state\n");
            return false;
        }
        m_context->PSSetSamplers(0, 1, m_samplerState.GetAddressOf());
        std::printf("Feature Level: %x\n", featureLevel);

        return true;
    }

    void shutdown() override {
        // Clean up meshes
        for (auto& pair : m_meshes) {
            // ComPtr handles cleanup automatically
        }
        m_meshes.clear();

        // Clean up shaders
        for (auto& pair : m_shaders) {
            // ComPtr handles cleanup automatically
        }
        m_shaders.clear();

        m_context.Reset();
        m_device.Reset();
        m_swapChain.Reset();
    }

    void beginFrame() override {
        m_context->ClearRenderTargetView(m_rtv.Get(), m_clearColor);
        m_context->ClearDepthStencilView(m_dsv.Get(), 
                                        D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 
                                        1.0f, 0);
    }

    void endFrame() override {
        m_swapChain->Present(1, 0);
    }

    void setClearColor(float r, float g, float b, float a) override {
        m_clearColor[0] = r;
        m_clearColor[1] = g;
        m_clearColor[2] = b;
        m_clearColor[3] = a;
    }

    void setViewport(int width, int height) override {
        m_width = width;
        m_height = height;

        // Unbind render targets
        m_context->OMSetRenderTargets(0, nullptr, nullptr);

        // Resize swap chain
        m_swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);

        // Recreate render targets
        createRTVAndDSV(width, height);
    }

    uint32_t createMesh(const Vertex* vertices, uint32_t vertexCount,
                       const uint16_t* indices, uint32_t indexCount) override {
        D3D11Mesh mesh;
        mesh.indexCount = indexCount;

        // Create vertex buffer
        D3D11_BUFFER_DESC vbDesc{};
        vbDesc.Usage = D3D11_USAGE_IMMUTABLE;
        vbDesc.ByteWidth = vertexCount * sizeof(Vertex);
        vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

        D3D11_SUBRESOURCE_DATA vbData{};
        vbData.pSysMem = vertices;

        HRESULT hr = m_device->CreateBuffer(&vbDesc, &vbData, mesh.vertexBuffer.GetAddressOf());
        if (FAILED(hr)) {
            std::fprintf(stderr, "Failed to create vertex buffer\n");
            return 0;
        }

        // Create index buffer
        D3D11_BUFFER_DESC ibDesc{};
        ibDesc.Usage = D3D11_USAGE_IMMUTABLE;
        ibDesc.ByteWidth = indexCount * sizeof(uint16_t);
        ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

        D3D11_SUBRESOURCE_DATA ibData{};
        ibData.pSysMem = indices;

        hr = m_device->CreateBuffer(&ibDesc, &ibData, mesh.indexBuffer.GetAddressOf());
        if (FAILED(hr)) {
            std::fprintf(stderr, "Failed to create index buffer\n");
            return 0;
        }

        uint32_t handle = m_nextMeshHandle++;
        m_meshes[handle] = mesh;
        return handle;
    }

    void destroyMesh(uint32_t meshHandle) override {
        auto it = m_meshes.find(meshHandle);
        if (it != m_meshes.end()) {
            m_meshes.erase(it);
        }
    }

    uint32_t createShader(const char* vertexSource, const char* fragmentSource) override {
        // Convert GLSL to HLSL (simplified - just use our known HLSL)
        std::string hlsl = glslToHLSL(vertexSource, fragmentSource);

        D3D11Shader shader;

        // Compile vertex shader
        ComPtr<ID3DBlob> vsBlob;
        if (!compileShader(hlsl.c_str(), "VSMain", "vs_5_0", vsBlob)) {
            std::fprintf(stderr, "Failed to compile vertex shader\n");
            return 0;
        }

        HRESULT hr = m_device->CreateVertexShader(
            vsBlob->GetBufferPointer(), 
            vsBlob->GetBufferSize(), 
            nullptr, 
            shader.vertexShader.GetAddressOf());
        if (FAILED(hr)) {
            std::fprintf(stderr, "Failed to create vertex shader\n");
            return 0;
        }

        // Compile pixel shader
        ComPtr<ID3DBlob> psBlob;
        if (!compileShader(hlsl.c_str(), "PSMain", "ps_5_0", psBlob)) {
            std::fprintf(stderr, "Failed to compile pixel shader\n");
            return 0;
        }

        hr = m_device->CreatePixelShader(
            psBlob->GetBufferPointer(), 
            psBlob->GetBufferSize(), 
            nullptr, 
            shader.pixelShader.GetAddressOf());
        if (FAILED(hr)) {
            std::fprintf(stderr, "Failed to create pixel shader\n");
            return 0;
        }

        // Create input layout — must match Vertex struct AND VSIn semantics
        D3D11_INPUT_ELEMENT_DESC layout[] = {
            { "POSITION",  0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL",    0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR",     0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD",  0, DXGI_FORMAT_R32G32_FLOAT,       0, 40, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TANGENT",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 48, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "BITANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 60, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };

        hr = m_device->CreateInputLayout(
            layout, 
            (UINT)std::size(layout),
            vsBlob->GetBufferPointer(), 
            vsBlob->GetBufferSize(),
            shader.inputLayout.GetAddressOf());
        if (FAILED(hr)) {
            std::fprintf(stderr, "Failed to create input layout\n");
            return 0;
        }

        // Create constant buffer
        D3D11_BUFFER_DESC cbDesc{};
        cbDesc.Usage = D3D11_USAGE_DYNAMIC;
        cbDesc.ByteWidth = sizeof(CBData);
        cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        hr = m_device->CreateBuffer(&cbDesc, nullptr, shader.constantBuffer.GetAddressOf());
        if (FAILED(hr)) {
            std::fprintf(stderr, "Failed to create constant buffer\n");
            return 0;
        }

        uint32_t handle = m_nextShaderHandle++;
        
        // Initialize constant buffer data
        shader.cbData.mvp        = XMMatrixIdentity();
        shader.cbData.world      = XMMatrixIdentity();
        shader.cbData.lightDir   = XMFLOAT3(0.0f, -1.0f, 0.0f);
        shader.cbData.useTexture = 0.0f;
        
        m_shaders[handle] = shader;
        return handle;
    }

    void destroyShader(uint32_t shaderHandle) override {
        auto it = m_shaders.find(shaderHandle);
        if (it != m_shaders.end()) {
            m_shaders.erase(it);
        }
    }

    void useShader(uint32_t shaderHandle) override {
        auto it = m_shaders.find(shaderHandle);
        if (it != m_shaders.end()) {
            m_currentShader = shaderHandle;
            
            D3D11Shader& shader = it->second;
            m_context->IASetInputLayout(shader.inputLayout.Get());
            m_context->VSSetShader(shader.vertexShader.Get(), nullptr, 0);
            m_context->PSSetShader(shader.pixelShader.Get(), nullptr, 0);
        }
    }

    void setUniformMat4(uint32_t shaderHandle, const char* name, const Mat4& matrix) override {
        static bool debugOnce = true;
        if (debugOnce) {
            printf("D3D11 setUniformMat4: shader=%u, name=%s\n", shaderHandle, name);
            debugOnce = false;
        }
        
        auto it = m_shaders.find(shaderHandle);
        if (it == m_shaders.end()) {
            printf("D3D11 setUniformMat4: Shader %u NOT FOUND!\n", shaderHandle);
            return;
        }

        D3D11Shader& shader = it->second;

        // Store the matrix for later batch update
        // Convert column-major Mat4 to XMMATRIX and transpose
        XMMATRIX xm = XMLoadFloat4x4((const XMFLOAT4X4*)matrix.m);
        xm = XMMatrixTranspose(xm);

        if (strcmp(name, "uMVP") == 0) {
            shader.cbData.mvp = xm;
            
            // Store view-projection for instanced rendering
            // Only store when NOT in instanced draw to prevent overwriting
            if (!m_inInstancedDraw) {
                m_viewProj = xm;
                m_hasViewProj = true;
                
                static bool once = true;
                if (once) {
                    printf("D3D11: Stored viewProj, inInstancedDraw=%d, hasViewProj=%d\n", 
                           m_inInstancedDraw, m_hasViewProj);
                    once = false;
                }
            }
        } else if (strcmp(name, "uWorld") == 0) {
            shader.cbData.world = xm;
        }
    }

    void setUniformVec3(uint32_t shaderHandle, const char* name, const Vec3& vec) override {
        auto it = m_shaders.find(shaderHandle);
        if (it == m_shaders.end()) return;

        D3D11Shader& shader = it->second;

        // Store the vector for later batch update
        if (strcmp(name, "uLightDir") == 0) {
            shader.cbData.lightDir = XMFLOAT3(vec.x, vec.y, vec.z);
        }
    }

    uint32_t createTexture(const char* filepath) override {
        int width, height, channels;
        stbi_set_flip_vertically_on_load(false);
        unsigned char* data = stbi_load(filepath, &width, &height, &channels, 4);
        
        if (!data) {
            std::fprintf(stderr, "Failed to load texture: %s - %s\n", filepath, stbi_failure_reason());
            return 0;
        }

        std::printf("Loaded texture: %s (%dx%d)\n", filepath, width, height);

        D3D11Texture texture;
        texture.width = width;
        texture.height = height;

        D3D11_TEXTURE2D_DESC texDesc = {};
        texDesc.Width = width;
        texDesc.Height = height;
        texDesc.MipLevels = 0;
        texDesc.ArraySize = 1;
        texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        texDesc.SampleDesc.Count = 1;
        texDesc.Usage = D3D11_USAGE_DEFAULT;
        texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        texDesc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

        HRESULT hr = m_device->CreateTexture2D(&texDesc, nullptr, &texture.texture);
        if (FAILED(hr)) {
            stbi_image_free(data);
            return 0;
        }

        m_context->UpdateSubresource(texture.texture.Get(), 0, nullptr, data, width * 4, 0);

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = texDesc.Format;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = -1;

        hr = m_device->CreateShaderResourceView(texture.texture.Get(), &srvDesc, &texture.srv);
        if (FAILED(hr)) {
            stbi_image_free(data);
            return 0;
        }

        m_context->GenerateMips(texture.srv.Get());
        stbi_image_free(data);

        uint32_t handle = m_nextTextureHandle++;
        m_textures[handle] = texture;
        return handle;
    }

    void destroyTexture(uint32_t textureHandle) override {
        m_textures.erase(textureHandle);
    }
    
    uint32_t createTextureFromData(const uint8_t* data, int width, int height, int channels) override {
        if (!data) {
            std::fprintf(stderr, "D3D11: createTextureFromData - null data\n");
            return 0;
        }
        
        std::printf("D3D11: Creating texture from data (%dx%d, %d channels)\n", width, height, channels);
        
        D3D11Texture texture;
        texture.width = width;
        texture.height = height;
        
        // Determine format
        DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
        if (channels == 1) format = DXGI_FORMAT_R8_UNORM;
        else if (channels == 3) format = DXGI_FORMAT_R8G8B8A8_UNORM; // D3D11 doesn't have RGB8, use RGBA8
        else if (channels == 4) format = DXGI_FORMAT_R8G8B8A8_UNORM;
        
        // If 3 channels, need to convert to 4 channels
        std::vector<uint8_t> rgba_data;
        const uint8_t* upload_data = data;
        if (channels == 3) {
            rgba_data.resize(width * height * 4);
            for (int i = 0; i < width * height; i++) {
                rgba_data[i * 4 + 0] = data[i * 3 + 0];
                rgba_data[i * 4 + 1] = data[i * 3 + 1];
                rgba_data[i * 4 + 2] = data[i * 3 + 2];
                rgba_data[i * 4 + 3] = 255;
            }
            upload_data = rgba_data.data();
        }
        
        // Create texture
        D3D11_TEXTURE2D_DESC texDesc = {};
        texDesc.Width = width;
        texDesc.Height = height;
        texDesc.MipLevels = 1;
        texDesc.ArraySize = 1;
        texDesc.Format = format;
        texDesc.SampleDesc.Count = 1;
        texDesc.Usage = D3D11_USAGE_IMMUTABLE;
        texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        
        D3D11_SUBRESOURCE_DATA initData = {};
        initData.pSysMem = upload_data;
        initData.SysMemPitch = width * (channels == 3 ? 4 : channels);
        
        HRESULT hr = m_device->CreateTexture2D(&texDesc, &initData, texture.texture.GetAddressOf());
        if (FAILED(hr)) {
            std::fprintf(stderr, "D3D11: Failed to create texture from data\n");
            return 0;
        }
        
        // Create SRV
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = format;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        
        hr = m_device->CreateShaderResourceView(texture.texture.Get(), &srvDesc, texture.srv.GetAddressOf());
        if (FAILED(hr)) {
            std::fprintf(stderr, "D3D11: Failed to create SRV for texture\n");
            return 0;
        }
        
        uint32_t handle = m_nextTextureHandle++;
        m_textures[handle] = texture;
        std::printf("D3D11: Texture created successfully, handle=%u\n", handle);
        return handle;
    }
    
    void bindTextureToUnit(uint32_t textureHandle, int unit) override {
        // D3D11: Store for binding in draw calls
        if (unit == 1) {
            m_boundNormalMap = textureHandle;
        }
        // Diffuse texture (unit 0) is bound per-draw-call in drawMesh
    }

    void setUniformInt(uint32_t shaderHandle, const char* name, int value) override {
        auto it = m_shaders.find(shaderHandle);
        if (it == m_shaders.end()) return;
        if (strcmp(name, "uUseTexture") == 0)
            it->second.cbData.useTexture = (float)value;
        else if (strcmp(name, "uUseNormalMap") == 0)
            it->second.cbData.useNormalMap = (float)value;
    }

    void drawMesh(uint32_t meshHandle, uint32_t textureHandle = 0) override {
        auto meshIt = m_meshes.find(meshHandle);
        if (meshIt == m_meshes.end()) return;

        // Update constant buffer
        auto shaderIt = m_shaders.find(m_currentShader);
        if (shaderIt != m_shaders.end()) {
            D3D11Shader& shader = shaderIt->second;

            // Propagate texture flag before uploading to GPU
            shader.cbData.useTexture = (textureHandle > 0) ? 1.0f : 0.0f;
            
            D3D11_MAPPED_SUBRESOURCE ms{};
            HRESULT hr = m_context->Map(shader.constantBuffer.Get(), 0, 
                                        D3D11_MAP_WRITE_DISCARD, 0, &ms);
            if (SUCCEEDED(hr)) {
                CBData* cb = (CBData*)ms.pData;
                *cb = shader.cbData;
                m_context->Unmap(shader.constantBuffer.Get(), 0);
            }

            m_context->VSSetConstantBuffers(0, 1, shader.constantBuffer.GetAddressOf());
            m_context->PSSetConstantBuffers(0, 1, shader.constantBuffer.GetAddressOf());
        }

        // Bind or unbind diffuse texture (slot 0)
        if (textureHandle > 0) {
            auto texIt = m_textures.find(textureHandle);
            if (texIt != m_textures.end()) {
                ID3D11ShaderResourceView* srv = texIt->second.srv.Get();
                m_context->PSSetShaderResources(0, 1, &srv);
            }
        } else {
            ID3D11ShaderResourceView* nullSRV = nullptr;
            m_context->PSSetShaderResources(0, 1, &nullSRV);
        }
        
        // Bind normal map to slot 1
        if (m_boundNormalMap > 0) {
            auto normalTexIt = m_textures.find(m_boundNormalMap);
            if (normalTexIt != m_textures.end()) {
                ID3D11ShaderResourceView* normalSrv = normalTexIt->second.srv.Get();
                m_context->PSSetShaderResources(1, 1, &normalSrv);
            }
        } else {
            ID3D11ShaderResourceView* nullSRV = nullptr;
            m_context->PSSetShaderResources(1, 1, &nullSRV);
        }

        D3D11Mesh& mesh = meshIt->second;
        UINT stride = sizeof(Vertex);
        UINT offset = 0;
        m_context->IASetVertexBuffers(0, 1, mesh.vertexBuffer.GetAddressOf(), &stride, &offset);
        m_context->IASetIndexBuffer(mesh.indexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
        m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_context->DrawIndexed(mesh.indexCount, 0, 0);
    }
    
    // Instanced drawing
    void drawMeshInstanced(uint32_t meshHandle, uint32_t textureHandle,
                          const InstanceData* instances, uint32_t instanceCount) override {
        if (instanceCount == 0 || !instances) return;
        if (!m_hasViewProj) {
            printf("D3D11: drawMeshInstanced called but no viewProj stored!\n");
            return;
        }
        
        auto shaderIt = m_shaders.find(m_currentShader);
        if (shaderIt == m_shaders.end()) return;
        
        static int debugCount = 0;
        if (debugCount < 3) {
            printf("D3D11 drawMeshInstanced: mesh=%u, tex=%u, count=%u\n", 
                   meshHandle, textureHandle, instanceCount);
            printf("  ViewProj stored: %d\n", m_hasViewProj ? 1 : 0);
            debugCount++;
        }
        
        // Set flag to prevent viewProj from being overwritten
        m_inInstancedDraw = true;
        
        static bool printedTransform = false;
        if (!printedTransform && instanceCount > 0) {
            printf("D3D11: First instance transform:\n");
            printf("  [%.2f %.2f %.2f %.2f]\n", 
                   instances[0].worldMatrix.m[0], instances[0].worldMatrix.m[1],
                   instances[0].worldMatrix.m[2], instances[0].worldMatrix.m[3]);
            printf("  [%.2f %.2f %.2f %.2f]\n", 
                   instances[0].worldMatrix.m[4], instances[0].worldMatrix.m[5],
                   instances[0].worldMatrix.m[6], instances[0].worldMatrix.m[7]);
            printf("  [%.2f %.2f %.2f %.2f]\n", 
                   instances[0].worldMatrix.m[8], instances[0].worldMatrix.m[9],
                   instances[0].worldMatrix.m[10], instances[0].worldMatrix.m[11]);
            printf("  [%.2f %.2f %.2f %.2f]\n", 
                   instances[0].worldMatrix.m[12], instances[0].worldMatrix.m[13],
                   instances[0].worldMatrix.m[14], instances[0].worldMatrix.m[15]);
            printedTransform = true;
        }
        
        // Fallback implementation: draw one at a time using stored viewProj
        for (uint32_t i = 0; i < instanceCount; i++) {
            // World matrix from instance data needs to be transposed to match viewProj
            XMMATRIX world = Mat4ToXM(instances[i].worldMatrix);
            world = XMMatrixTranspose(world);
            
            // Both world and viewProj are transposed
            // XMMatrixMultiply(A, B) computes A * B
            // We have: world^T and viewProj^T
            // We want: MVP = viewProj * world
            // Since (AB)^T = B^T * A^T, we need: viewProj^T * world^T to get (world * viewProj)^T
            // But we want (viewProj * world)^T, so we compute: world^T * viewProj^T
            // Wait, that gives us (viewProj * world)^T which is what D3D expects!
            XMMATRIX mvp = XMMatrixMultiply(m_viewProj, world);  // viewProj * world (both transposed)
            
            shaderIt->second.cbData.world = world;
            shaderIt->second.cbData.mvp = mvp;
            
            drawMesh(meshHandle, textureHandle);
        }
        
        m_inInstancedDraw = false;
    }

    void setDepthTest(bool enable) override {
        D3D11_DEPTH_STENCIL_DESC dsDesc{};
        dsDesc.DepthEnable = enable ? TRUE : FALSE;
        dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        dsDesc.DepthFunc = D3D11_COMPARISON_LESS;

        ComPtr<ID3D11DepthStencilState> dsState;
        HRESULT hr = m_device->CreateDepthStencilState(&dsDesc, dsState.GetAddressOf());
        if (SUCCEEDED(hr)) {
            m_context->OMSetDepthStencilState(dsState.Get(), 0);
        }
    }

    void setCulling(bool enable) override {
        D3D11_RASTERIZER_DESC rsDesc{};
        rsDesc.FillMode = D3D11_FILL_SOLID;
        rsDesc.CullMode = enable ? D3D11_CULL_BACK : D3D11_CULL_NONE;
        rsDesc.FrontCounterClockwise = TRUE;
        rsDesc.DepthClipEnable = TRUE;

        ComPtr<ID3D11RasterizerState> rsState;
        HRESULT hr = m_device->CreateRasterizerState(&rsDesc, rsState.GetAddressOf());
        if (SUCCEEDED(hr)) {
            m_context->RSSetState(rsState.Get());
        }
    }
};

// ==================== Factory (Windows version) ====================
#ifdef _WIN32

// Forward declarations - defined in other renderer files
IRenderer* createOpenGLRenderer();
IRenderer* createD3D12Renderer();

IRenderer* createRenderer(RendererAPI api) {
    switch (api) {
        case RendererAPI::OpenGL:
            return createOpenGLRenderer();
        case RendererAPI::Direct3D11:
            return new D3D11Renderer();
        case RendererAPI::Direct3D12:
            return createD3D12Renderer();
        default:
            return nullptr;
    }
}
#endif
