#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

// 전역 변수
HWND g_hWnd = nullptr;
ID3D11Device*           g_pd3dDevice = nullptr;
ID3D11DeviceContext*    g_pImmediateContext = nullptr;
IDXGISwapChain*         g_pSwapChain = nullptr;
ID3D11RenderTargetView* g_pRenderTargetView = nullptr;
ID3D11DepthStencilView* g_pDepthStencilView = nullptr;

// #2 (3) 정점 구조체 및 버퍼 관련 전역 변수 추가
struct Vertex {
    float position[3];
    float color[4];
};
ID3D11Buffer* g_pVertexBuffer = nullptr;
ID3D11InputLayout* g_pInputLayout = nullptr;
ID3D11VertexShader* g_pVertexShader = nullptr;
ID3D11PixelShader* g_pPixelShader = nullptr;

// ImGui 윈도우 프로시저 전달용
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

// 스텐실 상태 선택용 전역 변수
static int g_stencilFunc = 2; // 기본값: D3D11_COMPARISON_ALWAYS
static int g_stencilOp = 0;   // 기본값: D3D11_STENCIL_OP_KEEP
static int g_stencilRef = 1;  // 기본값: 1

bool InitD3D11(HWND hWnd) {
    RECT rect;
    GetClientRect(hWnd, &rect);
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;

    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 1;
    sd.BufferDesc.Width = width;
    sd.BufferDesc.Height = height;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_0,
    };

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags,
        featureLevels, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain,
        &g_pd3dDevice, &featureLevel, &g_pImmediateContext);
    if (FAILED(hr)) return false;

    ID3D11Texture2D* pBackBuffer = nullptr;
    hr = g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
    if (FAILED(hr)) return false;

    hr = g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_pRenderTargetView);
    pBackBuffer->Release();
    if (FAILED(hr)) return false;

    // #2 (1) 깊이-스텐실 버퍼 생성
    D3D11_TEXTURE2D_DESC depthDesc = {};
    depthDesc.Width = width;
    depthDesc.Height = height;
    depthDesc.MipLevels = 1;
    depthDesc.ArraySize = 1;
    depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.SampleDesc.Quality = 0;
    depthDesc.Usage = D3D11_USAGE_DEFAULT;
    depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    depthDesc.CPUAccessFlags = 0;
    depthDesc.MiscFlags = 0;

    ID3D11Texture2D* pDepthStencil = nullptr;
    hr = g_pd3dDevice->CreateTexture2D(&depthDesc, nullptr, &pDepthStencil);
    if (FAILED(hr)) return false;

    hr = g_pd3dDevice->CreateDepthStencilView(pDepthStencil, nullptr, &g_pDepthStencilView);
    pDepthStencil->Release();
    if (FAILED(hr)) return false;

    g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, g_pDepthStencilView);

    D3D11_VIEWPORT vp = {};
    vp.Width = (FLOAT)width;
    vp.Height = (FLOAT)height;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    g_pImmediateContext->RSSetViewports(1, &vp);

    // #2 (3) 삼각형 2개 정점 데이터 (z값 다르게, 겹침 명확화)
    Vertex vertices[] = {
        // 마스크 삼각형 (빨강, 화면 중앙 위쪽)
        { { -0.4f,  0.2f, 0.5f }, { 1, 0, 0, 1 } },
        { {  0.0f,  0.8f, 0.5f }, { 1, 0, 0, 1 } },
        { {  0.4f,  0.2f, 0.5f }, { 1, 0, 0, 1 } },
        // 테스트 삼각형 (초록, 화면 중앙 아래쪽, 일부 겹침)
        { { -0.4f, -0.2f, 0.3f }, { 0, 1, 0, 1 } },
        { {  0.0f, -0.8f, 0.3f }, { 0, 1, 0, 1 } },
        { {  0.4f, -0.2f, 0.3f }, { 0, 1, 0, 1 } },
    };
    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(vertices);
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = 0;
    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = vertices;
    hr = g_pd3dDevice->CreateBuffer(&bd, &initData, &g_pVertexBuffer);
    if (FAILED(hr)) return false;

    // #2 (4) HLSL 셰이더 소스 코드 (간단한 패스스루)
    const char* vsCode = R"(
    struct VS_IN {
        float3 pos : POSITION;
        float4 col : COLOR;
    };
    struct PS_IN {
        float4 pos : SV_POSITION;
        float4 col : COLOR;
    };
    PS_IN main(VS_IN input) {
        PS_IN output;
        output.pos = float4(input.pos, 1.0f);
        output.col = input.col;
        return output;
    })";
    const char* psCode = R"(
    struct PS_IN {
        float4 pos : SV_POSITION;
        float4 col : COLOR;
    };
    float4 main(PS_IN input) : SV_TARGET {
        return input.col;
    })";
    ID3DBlob* vsBlob = nullptr;
    ID3DBlob* psBlob = nullptr;
    ID3DBlob* errorBlob = nullptr;
    hr = D3DCompile(vsCode, strlen(vsCode), nullptr, nullptr, nullptr, "main", "vs_4_0", 0, 0, &vsBlob, &errorBlob);
    if (FAILED(hr)) return false;
    hr = D3DCompile(psCode, strlen(psCode), nullptr, nullptr, nullptr, "main", "ps_4_0", 0, 0, &psBlob, &errorBlob);
    if (FAILED(hr)) { if (vsBlob) vsBlob->Release(); return false; }
    hr = g_pd3dDevice->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &g_pVertexShader);
    if (FAILED(hr)) { vsBlob->Release(); psBlob->Release(); return false; }
    hr = g_pd3dDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &g_pPixelShader);
    if (FAILED(hr)) { vsBlob->Release(); psBlob->Release(); return false; }
    // #2 (4) 입력 레이아웃 생성
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    hr = g_pd3dDevice->CreateInputLayout(layout, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &g_pInputLayout);
    vsBlob->Release(); psBlob->Release();
    if (FAILED(hr)) return false;

    return true;
}

void CleanupD3D11() {
    if (g_pImmediateContext) g_pImmediateContext->ClearState();
    if (g_pDepthStencilView) g_pDepthStencilView->Release();
    if (g_pRenderTargetView) g_pRenderTargetView->Release();
    if (g_pSwapChain) g_pSwapChain->Release();
    if (g_pImmediateContext) g_pImmediateContext->Release();
    if (g_pd3dDevice) g_pd3dDevice->Release();
    if (g_pVertexBuffer) g_pVertexBuffer->Release();
    if (g_pInputLayout) g_pInputLayout->Release();
    if (g_pVertexShader) g_pVertexShader->Release();
    if (g_pPixelShader) g_pPixelShader->Release();
}

void Render() {
    float clearColor[4] = { 0.2f, 0.4f, 0.6f, 1.0f };
    g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, clearColor);
    g_pImmediateContext->ClearDepthStencilView(g_pDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    // ImGui 선택값에 따라 스텐실 상태 동적 생성/적용
    D3D11_DEPTH_STENCIL_DESC dsDesc = {};
    dsDesc.DepthEnable = TRUE;
    dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    dsDesc.DepthFunc = D3D11_COMPARISON_LESS;
    dsDesc.StencilEnable = TRUE;
    dsDesc.StencilReadMask = 0xFF;
    dsDesc.StencilWriteMask = 0xFF;
    dsDesc.FrontFace.StencilFunc = (D3D11_COMPARISON_FUNC)g_stencilFunc;
    dsDesc.FrontFace.StencilPassOp = (D3D11_STENCIL_OP)g_stencilOp;
    dsDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
    dsDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
    dsDesc.BackFace = dsDesc.FrontFace;
    ID3D11DepthStencilState* pDSState = nullptr;
    g_pd3dDevice->CreateDepthStencilState(&dsDesc, &pDSState);

    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    g_pImmediateContext->IASetInputLayout(g_pInputLayout);
    g_pImmediateContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);
    g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    g_pImmediateContext->VSSetShader(g_pVertexShader, nullptr, 0);
    g_pImmediateContext->PSSetShader(g_pPixelShader, nullptr, 0);

    // 첫 번째 삼각형: 스텐실 마스크로만 그림 (스텐실 값 1로 설정)
    D3D11_DEPTH_STENCIL_DESC maskDesc = dsDesc;
    maskDesc.StencilEnable = TRUE;
    maskDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
    maskDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_REPLACE;
    maskDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
    maskDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
    maskDesc.BackFace = maskDesc.FrontFace;
    ID3D11DepthStencilState* pMaskState = nullptr;
    g_pd3dDevice->CreateDepthStencilState(&maskDesc, &pMaskState);
    g_pImmediateContext->OMSetDepthStencilState(pMaskState, 1); // 스텐실 값 1로 설정
    g_pImmediateContext->Draw(3, 0); // 첫 번째 삼각형만
    if (pMaskState) pMaskState->Release();

    // 두 번째 삼각형: 스텐실 조건에 따라 그림
    g_pImmediateContext->OMSetDepthStencilState(pDSState, g_stencilRef);
    g_pImmediateContext->Draw(3, 3); // 두 번째 삼각형만

    if (pDSState) pDSState->Release();
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, "DX11PracticeWindowClass", NULL };
    RegisterClassEx(&wc);
    g_hWnd = CreateWindow(wc.lpszClassName, "DirectX11 Practice Window", WS_OVERLAPPEDWINDOW, 100, 100, 800, 600, NULL, NULL, wc.hInstance, NULL);

    if (!InitD3D11(g_hWnd)) {
        MessageBox(NULL, "DirectX11 초기화 실패", "Error", MB_OK);
        return 0;
    }

    // #2 (9) ImGui 초기화
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(g_hWnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pImmediateContext);

    ShowWindow(g_hWnd, nCmdShow);
    UpdateWindow(g_hWnd);

    MSG msg = {};
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            ImGui_ImplDX11_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();

            ImGui::Begin("Stencil Example");
            ImGui::Text("ImGui + DirectX11 + Stencil Example");
            // #2 (10) 스텐실 비교 함수 선택
            const char* funcs[] = { "NEVER", "LESS", "EQUAL", "LESS_EQUAL", "GREATER", "NOT_EQUAL", "GREATER_EQUAL", "ALWAYS" };
            ImGui::Text("StencilFunc:");
            for (int i = 0; i < 8; ++i) {
                ImGui::RadioButton(funcs[i], &g_stencilFunc, i);
                if (i < 7) ImGui::SameLine();
            }
            // #2 (10) 스텐실 연산 선택
            const char* ops[] = { "KEEP", "ZERO", "REPLACE", "INCR_SAT", "DECR_SAT", "INVERT", "INCR", "DECR" };
            ImGui::Text("\nStencilOp:");
            for (int i = 0; i < 8; ++i) {
                ImGui::RadioButton(ops[i], &g_stencilOp, i);
                if (i < 7) ImGui::SameLine();
            }
            // #2 (10) 스텐실 참조값 선택
            ImGui::Text("\nStencilRef:");
            ImGui::SliderInt("##stencilref", &g_stencilRef, 0, 255);
            ImGui::End();

            ImGui::Render();
            Render();
            // ImGui 렌더 전 D3D11 상태 리셋
            g_pImmediateContext->OMSetDepthStencilState(nullptr, 0);
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
            g_pSwapChain->Present(1, 0);
        }
    }

    // #2 (9) ImGui 종료
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupD3D11();
    UnregisterClass(wc.lpszClassName, wc.hInstance);
    return 0;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // #2 (9) ImGui 윈도우 메시지 전달
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;
    switch (msg) {
    case WM_SIZE:
        if (g_pd3dDevice != nullptr && wParam != SIZE_MINIMIZED) {
            RECT rect;
            GetClientRect(hWnd, &rect);
            int width = rect.right - rect.left;
            int height = rect.bottom - rect.top;
            // D3D11 뷰포트 동기화
            D3D11_VIEWPORT vp = {};
            vp.Width = (FLOAT)width;
            vp.Height = (FLOAT)height;
            vp.MinDepth = 0.0f;
            vp.MaxDepth = 1.0f;
            vp.TopLeftX = 0;
            vp.TopLeftY = 0;
            if (g_pImmediateContext) g_pImmediateContext->RSSetViewports(1, &vp);
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
} 