// SPDX-License-Identifier: MIT
/* Copyright (c) 2022-2023 Chilledheart  */

// Main code

#include <vector>
#include <windows.h>

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <d3d11.h>
#include <sstream>

#include "account.hpp"
#include "helper.hpp"

using path = std::filesystem::path;

static const wchar_t kGenshinImpactDir[] = L"GenshinImpactLoader";
static const wchar_t kGenshinImpactImGuiIniFileName[] = L"GenshinImpactLoader\\imgui.ini";
static const wchar_t kGenshinImpactLevelDbFileName[] = L"GenshinImpactLoader\\GenshinImpactLoader.db";

#include "resource.hpp"

// below definition comes from WinUser.h
// https://docs.microsoft.com/en-us/windows/win32/hidpi/wm-dpichanged
#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0
#endif

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

#define SCALED_SIZE(X) (scale_factor * float(X))
#define SCALED_INT_SIZE(X) int(scale_factor * float(X))

static INT g_font_size;
static std::string g_font_name;

static leveldb::DB* g_db = nullptr;
static std::vector<Account> g_loadedAccounts[2];

#if defined(_MSC_VER)
#pragma comment(lib, "d3d11")
#pragma comment(lib, "d3dcompiler")
#pragma comment(lib, "dwmapi")
#endif

// Data
static ID3D11Device*            g_pd3dDevice = nullptr;
static ID3D11DeviceContext*     g_pd3dDeviceContext = nullptr;
static IDXGISwapChain*          g_pSwapChain = nullptr;
static ID3D11RenderTargetView*  g_mainRenderTargetView = nullptr;

// Forward declarations of helper functions
static bool CreateDeviceD3D(HWND hWnd);
static void CleanupDeviceD3D();
static void CreateRenderTarget();
static void CleanupRenderTarget();
static void OnChangedViewport(HWND hwnd, float scale_factor, const RECT* l);
static void LoadAccountsFromDisk(HWND hwnd);
static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static constexpr wchar_t kMsyhTTCFontName[] = L"msyh.ttc";
static constexpr wchar_t kMsyhTTFFontName[] = L"msyh.ttf";
static constexpr INT kMsyhFontSize = 14;
static constexpr wchar_t kSimSunFontName[] = L"simsun.ttc";
static constexpr INT kSimSunFontSize = 12;
static constexpr INT kDefaultFontSize = 13;

static void SetupWindowSize(HWND hwnd, float scale_factor, RECT *rect) {
    LONG x = rect->top;
    LONG y = rect->left;
    LONG width = (rect->right - rect->left);
    LONG height = (rect->bottom - rect->top);

    ::SetWindowPos(hwnd, nullptr, x, y, width, height,
                   SWP_NOZORDER | SWP_NOACTIVATE);
}

static void SetupFonts(float scale_factor) {
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    auto fonts_path = GetWindowsFontsPath();

    ImFont* font = nullptr;
    auto p = fonts_path / kMsyhTTCFontName;
    if (font == nullptr && FileExists(p)) {
        g_font_name = p.u8string();
        g_font_size = kMsyhFontSize;
        font = io.Fonts->AddFontFromFileTTF(g_font_name.c_str(), (float)SCALED_SIZE(g_font_size),
                                            nullptr, io.Fonts->GetGlyphRangesChineseFull());
        IM_ASSERT(font != nullptr);
    }
    p = fonts_path / kMsyhTTFFontName;
    if (font == nullptr && FileExists(p)) {
        g_font_name = p.u8string();
        g_font_name = (fonts_path / kMsyhTTFFontName).u8string();
        g_font_size = kMsyhFontSize;
        font = io.Fonts->AddFontFromFileTTF(g_font_name.c_str(), (float)SCALED_SIZE(g_font_size),
                                            nullptr, io.Fonts->GetGlyphRangesChineseFull());
        IM_ASSERT(font != nullptr);
    }
    p = fonts_path / kSimSunFontName;
    if (font == nullptr && FileExists(p)) {
        g_font_name = p.u8string();
        g_font_size = kSimSunFontSize;
        font = io.Fonts->AddFontFromFileTTF(g_font_name.c_str(), (float)SCALED_SIZE(g_font_size),
                                            nullptr, io.Fonts->GetGlyphRangesChineseFull());
        IM_ASSERT(font != nullptr);
    }
    if (font == nullptr) {
        g_font_name = std::string();
        g_font_size = kDefaultFontSize;

        ImFontConfig cfg;
        cfg.OversampleH = cfg.OversampleV = 1, cfg.PixelSnapH = true;
        cfg.SizePixels = SCALED_SIZE(g_font_size);
        font = io.Fonts->AddFontDefault(&cfg);
        IM_ASSERT(font != nullptr);
    }
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
      hInstance, LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APPICON)), nullptr, nullptr, nullptr,
      L"Genshin Impact Loader Class", nullptr };
    ::RegisterClassExW(&wc);
    INT x = 100, y = 100;
    INT width = 460, height = 400;
    RECT l {x, y, x+width, y+height};
    HWND hwnd = ::CreateWindowW(wc.lpszClassName,
                                L"Genshin Impact Multi Account Switch",
                                WS_OVERLAPPEDWINDOW, x, y, width, height,
                                nullptr, nullptr, wc.hInstance, nullptr);

    // Resize window up to dpi scale before everything
    float scale_factor = ImGui_ImplWin32_GetDpiScaleForHwnd(hwnd);

    l.left *= scale_factor;
    l.top *= scale_factor;
    l.right *= scale_factor;
    l.bottom *= scale_factor;
    SetupWindowSize(hwnd, scale_factor, &l);

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

    // Setup ImGui Ini File
    auto localapp_path = GetLocalAppPath();
    auto dir_path = localapp_path / kGenshinImpactDir;
    if (EnsureCreatedDirectory(dir_path)) {
        static std::string ini_path = (localapp_path / kGenshinImpactImGuiIniFileName).u8string();
        io.IniFilename = ini_path.c_str();
    } else {
        io.IniFilename = nullptr;
    }

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // Setup fonts
    SetupFonts(scale_factor);

    // Trigger DPI changed events
    OnChangedViewport(hwnd, scale_factor, &l);

    // Main loop
    bool done = false;

    // Our state
    std::vector<const char*> loadedAccountNames[2];
    char savedName[2][kMaxDisplayNameLength] = {};
    int selectedAccount[2] = {};
    const char* alert_message = nullptr;

    while (!done)
    {
        // Poll and handle messages (inputs, window resize, etc.)
        // See the WndProc() function below for our to dispatch events to the Win32 backend.
        MSG msg;
        while (::PeekMessageW(&msg, nullptr, 0U, 0U, PM_REMOVE))
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
                int load = 0;
                int save = 0;
                int gone = 0;
                int gone_selected = 0;

                if (!ImGui::BeginTabItem(isGlobal ? u8"Global Server" : u8"国服"))
                    continue;

                ImGui::Text(isGlobal ? u8"Choose existing account to load or save current account."
                            : u8"请选择加载当前账号或者保存当前账号");

                loadedAccountNames[i].clear();
                for (const auto& accnt : g_loadedAccounts[i]) {
                    loadedAccountNames[i].push_back(accnt.display_name().c_str());
                }

                if (!g_loadedAccounts[i].empty()) {
                    const char** items = &loadedAccountNames[i][0];
                    ImGui::ListBox(isGlobal ? u8"known accounts" : u8"账号列表", &selectedAccount[i], items, static_cast<int>(loadedAccountNames[i].size()), 4);

                    if (ImGui::Button(isGlobal ? u8"Load Selected" : u8"载入选中"))
                        load = 1;

                    ImGui::SameLine();

                    if (ImGui::Button(isGlobal ? u8"Wipe Selected" : u8"抹去选中"))
                        gone_selected = 1;

                    ImGui::SameLine();

                    if (ImGui::Button(isGlobal ? u8"View Selected" : u8"查看选中"))
                        ImGui::OpenPopup("view-popup");
                }

                ImGui::InputTextWithHint(isGlobal ? u8"Save As" : u8"另存为",
                                         isGlobal ? u8"enter account name" : u8"输入账号名称",
                                         savedName[i],
                                         IM_ARRAYSIZE(savedName[i]));

                ImGui::SameLine();

                if (ImGui::Button(isGlobal ? u8"Save" : u8"保存") && strnlen(savedName[i], sizeof(savedName[i])))
                    save = 1;

                ImGui::Text(isGlobal ? u8"Wipe current logged-in account." : u8"抹去当前账号");

                ImGui::SameLine();

                if (ImGui::Button(isGlobal ? u8"Wipe" : u8"抹去"))
                    gone = 1;

                if (load) {
                    if (!g_loadedAccounts[i][selectedAccount[i]].Save()) {
                        alert_message = isGlobal ? u8"Failed to save current account" : u8"无法写入当期帐号信息";
                        ImGui::OpenPopup("alert-popup");
                    }
                }
                if (gone_selected) {
                    if (g_loadedAccounts[i].size() > selectedAccount[i]) {
                        // save changes to disk
                        if (WipeAccountToDb(g_db, g_loadedAccounts[i][selectedAccount[i]])) {
                            auto iter = g_loadedAccounts[i].begin() + selectedAccount[i];
                            g_loadedAccounts[i].erase(iter);
                            auto iter2 = loadedAccountNames[i].begin() + selectedAccount[i];
                            loadedAccountNames[i].erase(iter2);
                        } else {
                            alert_message = isGlobal ? u8"Failed to sync changes to disk" : u8"无法保存当前操作";
                            ImGui::OpenPopup("alert-popup");
                        }
                    }
                }
                if (gone) {
                    std::vector<uint8_t> name(1, 0), data(0);
                    Account account(Rand<uint64_t>(), isGlobal, "Gone", name, data, 0);
                    if (account.Save()) {
                        alert_message = isGlobal ? u8"Current Login information is wiped" : u8"当前登录信息已被移除";
                        ImGui::OpenPopup("alert-popup");
                    }

                }
                if (save) {
                    Account account(isGlobal, savedName[i]);
                    if (!account.Load()) {
                        alert_message = isGlobal ? u8"Failed to load current account" : u8"无法读取当期帐号信息";
                        ImGui::OpenPopup("alert-popup");
                    } else {
                        // save accounts to disk
                        if (SaveAccountToDb(g_db, account)) {
                            g_loadedAccounts[i].push_back(account);
                            loadedAccountNames[i].push_back(account.display_name().c_str());
                            selectedAccount[i] = static_cast<int>(g_loadedAccounts[i].size()) - 1;
                            savedName[i][0] = '\0';
                        } else {
                            alert_message = isGlobal ? u8"Failed to sync changes to disk" : u8"无法保存当前操作";
                            ImGui::OpenPopup("alert-popup");
                        }
                    }
                }

                if (ImGui::BeginPopupModal("alert-popup"))
                {
                    ImGui::Text("%s", alert_message);
                    if (ImGui::Button(u8"Okay")) {
                        ImGui::CloseCurrentPopup();
                        alert_message = nullptr;
                    }
                    ImGui::EndPopup();
                }

                if (ImGui::BeginPopupModal("view-popup"))
                {
                    if (ImGui::Button(u8"Close popup")) {
                        ImGui::CloseCurrentPopup();
                    }
                    const auto& account = g_loadedAccounts[i][selectedAccount[i]];
                    std::time_t time = account.time();
                    ImGui::Text("[%s]: %s", "display name", account.display_name().c_str());
                    ImGui::Text("[%s]: %s", "key", account.name().data());
                    ImGui::Text("[%s]: %s", "time", std::ctime(&time));
                    const nlohmann::json &data_json = account.data_json();
                    for (auto& [key, val] : data_json.items()) {
                        if (val.is_discarded()) {
                            continue;
                        }
                        std::ostringstream os;
                        os << val;
                        ImGui::Text("[%s]: %s", key.c_str(), os.str().c_str());
                    }
                    ImGui::EndPopup();
                }
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
            ImGui::Separator();
        }

        ImGui::End();

        // Rendering
        ImGui::Render();
        ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
        const float clear_color_with_alpha[4] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_pSwapChain->Present(1, 0); // Present with vsync
        //g_pSwapChain->Present(0, 0); // Present without vsync
    }

    CloseDb(g_db);

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
    if (D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext) != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget()
{
   if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

void OnChangedViewport(HWND hwnd, float scale_factor, const RECT* rect) {
    static float previous_scale_factor = 1.0f;

    LONG x = rect->top;
    LONG y = rect->left;
    LONG width = (rect->right - rect->left);
    LONG height = (rect->bottom - rect->top);

    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImFont* font = nullptr; (void)font;

    ::SetWindowPos(hwnd, nullptr, x, y, width, height,
                   SWP_NOZORDER | SWP_NOACTIVATE);

    ImGui::GetStyle().ScaleAllSizes(1.0f / previous_scale_factor);
    ImGui::GetStyle().ScaleAllSizes(scale_factor);

    previous_scale_factor = scale_factor;

    io.Fonts->Clear();
    if (g_pd3dDeviceContext) {
        ImGui_ImplDX11_InvalidateDeviceObjects();
    }
    if (!g_font_name.empty()) {
        font = io.Fonts->AddFontFromFileTTF(g_font_name.c_str(), (float)SCALED_SIZE(g_font_size),
                                            nullptr, io.Fonts->GetGlyphRangesChineseFull());
    } else {
        ImFontConfig cfg;
        cfg.OversampleH = cfg.OversampleV = 1, cfg.PixelSnapH = true;
        cfg.SizePixels = SCALED_SIZE(g_font_size);
        font = io.Fonts->AddFontDefault(&cfg);
    }
    io.Fonts->Build();
    assert(font->IsLoaded());
}

void LoadAccountsFromDisk(HWND hwnd) {
    // Setup DB
    auto localapp_path = GetLocalAppPath();
    auto dir_path = localapp_path / kGenshinImpactDir;
    auto db_path = localapp_path / kGenshinImpactLevelDbFileName;
    if (!EnsureCreatedDirectory(dir_path)) {
        // "Unable to create directory"
        ::MessageBoxW(hwnd, L"Failed to Open LocalApp Directory", L"GenshinImpactLoader", MB_OK);
        ::PostQuitMessage(0);
        return;
    }
    g_db = OpenDb(db_path.u8string());
    if (!g_db) {
        ::MessageBoxW(hwnd, L"Failed to Open DB", L"GenshinImpactLoader", MB_OK);
        ::PostQuitMessage(0);
        return;
    }

    // Load DB to Memory
    // i = 0 -> Global Service
    // i = 1 -> CN Service
    // Load saved data from disk
    {
        std::vector<Account> loadedAccounts[2];
        LoadSavedAccounts_Old(loadedAccounts);
        for (int i = 0; i < 2; ++i) {
            for (const Account& account : loadedAccounts[i]) {
                if (!SaveAccountToDb(g_db, account)) {
                    ::MessageBoxW(hwnd, L"Failed to Load Old Data", L"GenshinImpactLoader", MB_OK);
                }
            }
        }
    }
    LoadSavedAccounts(g_db, g_loadedAccounts);

    // Update time field if missing
    std::time_t ctime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    for (int i = 0; i < 2; ++i) {
        for (Account& account : g_loadedAccounts[i]) {
            if (account.time() == 0) {
                account.update_time(ctime);
                if (!SaveAccountToDb(g_db, account)) {
                    // roll back if failed
                    account.update_time(0);
                }
            }
        }
    }
}

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
    case WM_CREATE:
        LoadAccountsFromDisk(hWnd);
        break;
    case WM_DPICHANGED:
        // https://github.com/microsoft/Windows-classic-samples/blob/main/Samples/DPIAwarenessPerWindow/client/DpiAwarenessContext.cpp
        OnChangedViewport(hWnd, HIWORD(wParam)/96.0f, reinterpret_cast<RECT*>(lParam));
        return 0;
    case WM_SIZE:
        if (g_pd3dDevice != nullptr && wParam != SIZE_MINIMIZED)
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
