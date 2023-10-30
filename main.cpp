// SPDX-License-Identifier: MIT
/* Copyright (c) 2022-2023 Chilledheart  */

// Main code
#include <vector>
#include <string>
#include <stdio.h>

#include <windows.h>

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <d3d11.h>

#include "resource.h"

#pragma comment(lib, "advapi32")
#pragma comment(lib, "d3d11")
#pragma comment(lib, "d3dcompiler")

// Data
static ID3D11Device*            g_pd3dDevice = NULL;
static ID3D11DeviceContext*     g_pd3dDeviceContext = NULL;
static IDXGISwapChain*          g_pSwapChain = NULL;
static ID3D11RenderTargetView*  g_mainRenderTargetView = NULL;

// Forward declarations of helper functions
static bool CreateDeviceD3D(HWND hWnd);
static void CleanupDeviceD3D();
static void CreateRenderTarget();
static void CleanupRenderTarget();
static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static const wchar_t* genshinImpactCnPathKey = L"Software\\miHoYo\\原神";
static const wchar_t* genshinImpactCnSdkKey = L"MIHOYOSDK_ADL_PROD_CN_h3123967166";
static const wchar_t* genshinImpactGlobalPathKey = L"Software\\miHoYo\\Genshin Impact";
static const wchar_t* genshinImpactGlobalSdkKey = L"MIHOYOSDK_ADL_PROD_OVERSEA_h1158948810";
static const wchar_t* genshinImpactDataKey = L"GENERAL_DATA_h2389025596";

static constexpr size_t kMaxDisplayNameLength = 128U;
static constexpr size_t kRegReadMaximumSize = 1024 * 1024;

static constexpr char kFontName[] = "C:\\Windows\\Fonts\\msyh.ttc";
static constexpr char kFontName2[] = "C:\\Windows\\Fonts\\msyh.ttf";
static constexpr INT kFontSize = 14;
static constexpr char kFontName3[] = "C:\\Windows\\Fonts\\simsun.ttc";
static constexpr INT kFontSize3 = 12;

#define DEFAULT_CONFIG_FILE "GenshinImpactLoader.dat"
#define SCALED_SIZE(X) (scaled_factor * float(X))

bool FileExists(LPCSTR szPath) {
    DWORD dwAttrib = GetFileAttributesA(szPath);

    return (dwAttrib != INVALID_FILE_ATTRIBUTES &&
           !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

static bool OpenKey(HKEY *hkey, bool isWriteOnly, bool isCN) {
    DWORD disposition;
    const wchar_t* subkey = isCN ? genshinImpactCnPathKey : genshinImpactGlobalPathKey;
    REGSAM samDesired =
        KEY_WOW64_64KEY | (isWriteOnly ? KEY_SET_VALUE : KEY_QUERY_VALUE);

    // Creates the specified registry key. If the key already exists, the
    // function opens it. Note that key names are not case sensitive.
    if (::RegCreateKeyExW(
            HKEY_CURRENT_USER /*hKey*/, subkey /* lpSubKey */, 0 /* Reserved */,
            nullptr /*lpClass*/, REG_OPTION_NON_VOLATILE /* dwOptions */,
            samDesired /* samDesired */, nullptr /*lpSecurityAttributes*/,
            hkey /* phkResult */,
            &disposition /* lpdwDisposition*/) == ERROR_SUCCESS) {
        if (disposition == REG_CREATED_NEW_KEY) {
        } else if (disposition == REG_OPENED_EXISTING_KEY) {
        }
        return true;
    }
    return false;
}

static bool ReadKey(HKEY hkey, bool isCN, bool isData, std::vector<BYTE>* output) {
    const wchar_t* valueName = isData ? genshinImpactDataKey : (isCN ? genshinImpactCnSdkKey : genshinImpactGlobalSdkKey);
    DWORD BufferSize;
    DWORD type;

    // If lpData is nullptr, and lpcbData is non-nullptr, the function returns
    // ERROR_SUCCESS and stores the size of the data, in bytes, in the variable
    // pointed to by lpcbData. This enables an application to determine the best
    // way to allocate a buffer for the value's data.
    if (::RegQueryValueExW(hkey /* HKEY */, valueName /* lpValueName */,
                           nullptr /* lpReserved */, &type /* lpType */,
                           nullptr /* lpData */,
                           &BufferSize /* lpcbData */) != ERROR_SUCCESS) {
        return false;
    }

    if (type != REG_BINARY || BufferSize > kRegReadMaximumSize) {
        return false;
    }

    output->resize(BufferSize);
    if (::RegQueryValueExW(hkey /* HKEY */,
                           valueName /* lpValueName */,
                           nullptr /* lpReserved */,
                           &type /* lpType */,
                           output->data() /* lpData */,
                           &BufferSize /* lpcbData */) != ERROR_SUCCESS) {
        return false;
    }
    output->reserve(BufferSize);
    return true;
}

static bool WriteKey(HKEY hkey, bool isCN, bool isData, const std::vector<BYTE>& output) {
    const wchar_t* valueName = isData ? genshinImpactDataKey : (isCN ? genshinImpactCnSdkKey : genshinImpactGlobalSdkKey);
    if (::RegSetValueExW(hkey /* hKey*/,
                         valueName /*lpValueName*/,
                         0 /*Reserved*/,
                         REG_BINARY /*dwType*/,
                         output.data() /*lpData*/,
                         static_cast<DWORD>(output.size()) /*cbData*/) == ERROR_SUCCESS) {
        return true;
    }
    return false;
}

static bool CloseKey(HKEY hkey) {
    return ::RegCloseKey(hkey);
}

class Account {
  public:
    Account(bool is_cn, const std::string& display_name)
      : is_cn_(is_cn), display_name_(display_name) {}

    Account(bool is_cn, const std::string& display_name,
            const std::vector<BYTE> &name,
            const std::vector<BYTE> &data)
      : is_cn_(is_cn), display_name_(display_name),
        name_(name), data_(data) {}

    const std::string& display_name() const {
      return display_name_;
    }

    const bool is_cn() const {
      return is_cn_;
    }

    const std::vector<BYTE>& name() const {
      return name_;
    }

    const std::vector<BYTE>& data() const {
      return data_;
    }

    bool Load() {
        HKEY hkey;
        if (!OpenKey(&hkey, false, is_cn_))
            return false;

        if (!ReadKey(hkey, is_cn_, false, &name_))
            goto failure;

        if (!ReadKey(hkey, is_cn_, true, &data_))
            goto failure;

        CloseKey(hkey);
        return true;

    failure:
        CloseKey(hkey);
        return false;
    }

    bool Save() const {
        HKEY hkey;
        if (!OpenKey(&hkey, true, is_cn_))
            return false;

        if (!WriteKey(hkey, is_cn_, false, name_))
            goto failure;

        if (!data_.empty() && !WriteKey(hkey, is_cn_, true, data_))
            goto failure;

        CloseKey(hkey);
        return true;

    failure:
        CloseKey(hkey);
        return false;
    }

  private:
    const bool is_cn_;
    const std::string display_name_;
    std::vector<BYTE> name_;
    std::vector<BYTE> data_;
};

void LoadSavedAccounts(std::vector<Account> *loadedAccounts) {
    FILE *f = fopen(DEFAULT_CONFIG_FILE, "r");
    struct {
        char name[kMaxDisplayNameLength];
        int isGlobal;
        char account[128];
        char userData[128 * 1024];
    } accnt;

    if (!f)
        return;

    while (fscanf(f, "%s %d %s %s\n", accnt.name, &accnt.isGlobal, accnt.account, accnt.userData) == 4) {
        std::string display_name = accnt.name;
        std::vector<BYTE> name;
        size_t len = strlen(accnt.account);
        name.resize(len + 1, 0);
        memcpy(name.data(), accnt.account, len);

        std::vector<BYTE> data;
        len = strlen(accnt.userData);
        data.resize(len + 1, 0);
        memcpy(data.data(), accnt.userData, len);
        Account account(!accnt.isGlobal, display_name, name, data);

        loadedAccounts[accnt.isGlobal != 0 ? 0 : 1].push_back(account);
    }

    fclose(f);
}

void SaveAccounts(const std::vector<Account> *loadedAccounts) {
    FILE* f = fopen(DEFAULT_CONFIG_FILE, "w");
    if (!f)
        return;
    for (int i = 0; i != 2; ++i)
        for (const auto &account : loadedAccounts[i])
           fprintf(f, "%s %d %s %s\n", account.display_name().c_str(),
                   !account.is_cn(),
                   (const char*)&account.name()[0],
                   (const char*)&account.data()[0]);
    fclose(f);
}

// Main Code
int WINAPI WinMain(HINSTANCE hInstance,
                   HINSTANCE hPrevInstance,
                   LPSTR     lpCmdLine,
                   int       nShowCmd)
{
    // Create application window
    ImGui_ImplWin32_EnableDpiAwareness();

    WNDCLASSEXW wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L,
      hInstance, LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APPICON)), NULL, NULL, NULL,
      L"Genshin Impact Loader Class", NULL };
    ::RegisterClassExW(&wc);
    INT x = 100, y = 100;
    INT width = 460, height = 400;
    HWND hwnd = ::CreateWindowW(wc.lpszClassName,
                                L"Genshin Impact Multi Account Switch",
                                WS_OVERLAPPEDWINDOW, x, y, width, height,
                                NULL, NULL, wc.hInstance, NULL);

    // Initialize Direct3D
    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClass(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    // Show the window
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    //io.Fonts->AddFontDefault();
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/ProggyTiny.ttf", 10.0f);
    // ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
    ImFont* font = NULL;

    float scaled_factor = ImGui_ImplWin32_GetDpiScaleForHwnd(hwnd);

    ::SetWindowPos(hwnd, nullptr, SCALED_SIZE(x), SCALED_SIZE(y),
                   SCALED_SIZE(width), SCALED_SIZE(height),
                   SWP_NOZORDER | SWP_NOACTIVATE);

    if (FileExists(kFontName)) {
      font = io.Fonts->AddFontFromFileTTF(kFontName, (float)SCALED_SIZE(kFontSize),
                                          NULL, io.Fonts->GetGlyphRangesChineseFull());
      IM_ASSERT(font != NULL);
    }
    if (font == NULL && FileExists(kFontName2)) {
      font = io.Fonts->AddFontFromFileTTF(kFontName2, (float)SCALED_SIZE(kFontSize),
                                          NULL, io.Fonts->GetGlyphRangesChineseFull());
      IM_ASSERT(font != NULL);
    }
    if (font == NULL && FileExists(kFontName3)) {
      font = io.Fonts->AddFontFromFileTTF(kFontName3, (float)SCALED_SIZE(kFontSize3),
                                          NULL, io.Fonts->GetGlyphRangesChineseSimplifiedCommon());
      IM_ASSERT(font != NULL);
    }
    font = io.Fonts->AddFontDefault();
    IM_ASSERT(font != NULL);

    // Our state
    std::vector<Account> loadedAccounts[2];
    std::vector<const char*> loadedAccountNames[2];
    static char savedName[2][kMaxDisplayNameLength];

    // i = 0 -> CN Service
    // i = 1 -> Global Service
    // Load saved data from disk
    LoadSavedAccounts(loadedAccounts);

    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

     // Main loop
    bool done = false;
    while (!done)
    {
        // Poll and handle messages (inputs, window resize, etc.)
        // See the WndProc() function below for our to dispatch events to the Win32 backend.
        MSG msg;
        while (::PeekMessageW(&msg, NULL, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessageW(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        // Start the Dear ImGui frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Welcome to Genshin Impact Multi Account Switch");                          // Create a window called "Hello, world!" and append into it.
        ImGui::Text("Choose client type to load or save current logged-in account.");

        // 2. Show a simple window that we create ourselves. We use a Begin/End pair to created a named window.
        ImGuiTabBarFlags tab_bar_flags = ImGuiTabBarFlags_None;
        if (ImGui::BeginTabBar("ServerTabBar", tab_bar_flags)) {
            for (int i = 0, isGlobal = 1; i != 2; ++i, isGlobal = 0) {
                static int load = 0;
                static int save = 0;
                static int gone = 0;

                if (!ImGui::BeginTabItem(isGlobal ? u8"Global Server" : u8"国服"))
                    continue;

                ImGui::Text(isGlobal ? u8"Choose existing account to load or save current account."
                            : u8"请选择加载当前或者保存当前");

                loadedAccountNames[i].clear();
                for (const auto& accnt : loadedAccounts[i]) {
                    loadedAccountNames[i].push_back(accnt.display_name().c_str());
                }

                static int item_current = 0;
                if (!loadedAccounts[i].empty()) {
                    const char** items = &loadedAccountNames[i][0];
                    ImGui::ListBox(u8"accounts", &item_current, items, static_cast<int>(loadedAccountNames[i].size()), 4);
                    ImGui::SameLine();

                    if (ImGui::Button(isGlobal ? u8"Save" : u8"载入"))
                        load = 1;
                }

                ImGui::InputTextWithHint(isGlobal ? "(global/ hint)" : "(cn/ hint)",
                                         isGlobal ? u8"enter account name" : u8"请输入当前名称",
                                         savedName[i],
                                         IM_ARRAYSIZE(savedName[i]));

                ImGui::Text(isGlobal ? u8"Save current account." : u8"保存当前");

                ImGui::SameLine();

                if (ImGui::Button(isGlobal ? u8"Save" : u8"保存") && strlen(savedName[i]))
                    save = 1;

                ImGui::Text(isGlobal ? u8"Wipe current logged-in account." : u8"抹去当前");

                ImGui::SameLine();

                if (ImGui::Button(isGlobal ? u8"Gone" : u8"抹去"))
                    gone = 1;

                if (load) {
                    if (loadedAccounts[i][item_current].Save()) {
                        load = 0;
                    }
                }
                if (gone) {
                    std::vector<BYTE> name(1, 0), data(1, 0);
                    Account account(!isGlobal, "Gone", name, data);
                    if (account.Save()) {
                        gone = 0;
                    }
                }
                if (save) {
                    Account account(!isGlobal, savedName[i]);
                    if (account.Load()) {
                        loadedAccounts[i].push_back(account);
                        loadedAccountNames[i].push_back(account.display_name().c_str());
                        item_current = static_cast<int>(loadedAccounts[i].size()) - 1;
                        // save accounts to disk
                        SaveAccounts(loadedAccounts);
                        save = 0;
                        savedName[i][0] = '\0';
                    }
                }
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
            ImGui::Separator();
        }

        ImGui::End();

        // Rendering
        ImGui::Render();
        const float clear_color_with_alpha[4] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_pSwapChain->Present(1, 0); // Present with vsync
        //g_pSwapChain->Present(0, 0); // Present without vsync
    }

    // Cleanup
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return 0;
}

// Helper functions

bool CreateDeviceD3D(HWND hWnd)
{
    // Setup swap chain
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    //createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    if (D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext) != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = NULL; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = NULL; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = NULL; }
}

void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = NULL; }
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Win32 message handler
// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (g_pd3dDevice != NULL && wParam != SIZE_MINIMIZED)
        {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProc(hWnd, msg, wParam, lParam);
}
