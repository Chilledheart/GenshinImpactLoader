#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <d3d11.h>
#include <tchar.h>
#include <stdio.h>

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

// Main code
#include <vector>
#include <string>

#include <windows.h>
#pragma comment(lib, "d3d11")
#pragma comment(lib, "d3dcompiler")

static const wchar_t* genshinImpactCnPathKey = L"Software\\miHoYo\\原神";
static const wchar_t* genshinImpactCnSdkKey = L"MIHOYOSDK_ADL_PROD_CN_h3123967166";
static const wchar_t* genshinImpactGlobalPathKey = L"Software\\miHoYo\\Genshin Impact";
static const wchar_t* genshinImpactGlobalSdkKey = L"MIHOYOSDK_ADL_PROD_OVERSEA_h1158948810";
static const wchar_t* genshinImpactDataKey = L"GENERAL_DATA_h2389025596";

static bool OpenKey(HKEY *hkey, bool isWriteOnly, bool isGlobal) {
    DWORD disposition;
    const wchar_t* subkey = isGlobal ? genshinImpactGlobalPathKey : genshinImpactCnPathKey;
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

static bool ReadKey(HKEY hkey, bool isGlobal, bool isData, std::vector<BYTE>* output) {
    const wchar_t* valueName = isData ? genshinImpactDataKey : (isGlobal ? genshinImpactGlobalSdkKey : genshinImpactCnSdkKey);
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

    if (type != REG_BINARY || BufferSize > 1024 * 1024) {
        return false;
    }

    output->resize(BufferSize);
    if (::RegQueryValueExW(hkey /* HKEY */, valueName /* lpValueName */,
                           nullptr /* lpReserved */, &type /* lpType */,
                           output->data() /* lpData */,
                           &BufferSize /* lpcbData */) != ERROR_SUCCESS) {
        return false;
    }
    output->reserve(BufferSize);
    return true;
}

static bool WriteKey(HKEY hkey, bool isGlobal, bool isData, const std::vector<BYTE>& output) {
    const wchar_t* valueName = isData ? genshinImpactDataKey : (isGlobal ? genshinImpactGlobalSdkKey : genshinImpactCnSdkKey);
    if (::RegSetValueExW(hkey /* hKey*/, valueName /*lpValueName*/, 0 /*Reserved*/,
                         REG_BINARY /*dwType*/,
                         output.data() /*lpData*/, static_cast<DWORD>(output.size()) /*cbData*/) == ERROR_SUCCESS) {
        return true;
    }
    return false;
}

static bool CloseKey(HKEY hkey) {
    return ::RegCloseKey(hkey);
}

static bool BackupAccount(bool isGlobal, std::vector<BYTE> *blobAccount, std::vector<BYTE> *blobData) {
    HKEY hkey;
    if (!OpenKey(&hkey, false, isGlobal))
        return false;

    if (!ReadKey(hkey, isGlobal, false, blobAccount))
        return false;

    if (!ReadKey(hkey, isGlobal, true, blobData))
        return false;

    CloseKey(hkey);
    return true;
}

static bool RecoverAccount(bool isGlobal, const std::vector<BYTE> &blobAccount, const std::vector<BYTE> &blobData) {
    HKEY hkey;
    if (!OpenKey(&hkey, true, isGlobal))
        return false;

    if (!WriteKey(hkey, isGlobal, false, blobAccount))
        return false;

    if (!WriteKey(hkey, isGlobal, true, blobData))
        return false;

    CloseKey(hkey);

    return true;
}

struct AccountInfomation {
    std::string name;
    std::vector<BYTE> blobAccount, blobData;
};

#define DEFAULT_CONFIG_FILE "GenshinImpactLoader.dat"

void LoadSavedAccounts(std::vector<AccountInfomation> *loadedAccounts) {
    FILE *f = fopen(DEFAULT_CONFIG_FILE, "r");
    struct {
        char name[128];
        int isGlobal;
        char account[128];
        char userData[128 * 1024];
    } accnt;

    if (!f)
        return;

    while (fscanf(f, "%s %d %s %s\n", accnt.name, &accnt.isGlobal, (char*)accnt.account, (char*)accnt.userData) == 4) {
        loadedAccounts[accnt.isGlobal != 0 ? 1 : 0].push_back(AccountInfomation());
        AccountInfomation &account = loadedAccounts[accnt.isGlobal != 0 ? 1 : 0].back();
        account.name = accnt.name;
        size_t len = strlen(accnt.account);
        account.blobAccount.resize(len+1);
        memcpy(accnt.account, &account.blobAccount[0], len);
        len = strlen(accnt.userData);
        account.blobData.resize(len+1);
        memcpy(accnt.userData, &account.blobData[0], len);
    }

    fclose(f);
}

void SaveAccounts(const std::vector<AccountInfomation> *loadedAccounts) {
    FILE* f = fopen(DEFAULT_CONFIG_FILE, "w");
    if (!f)
        return;
    for (int i = 0; i != 2; ++i)
        for (const auto &account : loadedAccounts[i])
           fprintf(f, "%s %d %s %s\n", account.name.c_str(), i,
                   (const char*)&account.blobAccount[0], (const char*)&account.blobData[0]);
    fclose(f);
}

// Main Code
int WinMain(HINSTANCE hInstance,
            HINSTANCE hPrevInstance,
            LPSTR     lpCmdLine,
            int       nShowCmd)
{
    // Create application window
    ImGui_ImplWin32_EnableDpiAwareness();
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, _T("ImGui Example"), NULL };
    ::RegisterClassEx(&wc);
    HWND hwnd = ::CreateWindow(wc.lpszClassName, _T("Genshin Impact Multi Account Switch"), WS_OVERLAPPEDWINDOW, 100, 100, 600, 400, NULL, NULL, wc.hInstance, NULL);

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
    ImFont* font;
    font = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\simsun.ttc", 16.0f, NULL, io.Fonts->GetGlyphRangesChineseSimplifiedCommon());
    IM_ASSERT(font != NULL);
    font = io.Fonts->AddFontDefault();
    IM_ASSERT(font != NULL);

    // Our state
    std::vector<AccountInfomation> loadedAccounts[2];
    std::vector<const char*> loadedAccountNames[2];
    static char savedName[2][128];

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
        while (::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
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
        ImGui::Text("Choose existing client type to load or save current account.");

        // 2. Show a simple window that we create ourselves. We use a Begin/End pair to created a named window.
        for (int i = 0, isGlobal = 0; i != 2; ++i, isGlobal = 1) {
            static int load = 0;
            static int save = 0;
            static int gone = 0;

            if (!ImGui::CollapsingHeader(isGlobal ? "Global Service" : "CN Service"))
                continue;

            ImGui::Text("Choose existing account to load or save current account.");               // Display some text (you can use a format strings too)

            loadedAccountNames[i].clear();
            for (const auto& accnt : loadedAccounts[i]) {
                loadedAccountNames[i].push_back(accnt.name.c_str());
            }

            static int item_current = 0;
            if (!loadedAccounts[i].empty()) {
                const char** items = &loadedAccountNames[i][0];
                ImGui::ListBox(isGlobal ? "accounts" : "CN accounts", &item_current, items, static_cast<int>(loadedAccountNames[i].size()), 4);
                ImGui::SameLine();

                if (ImGui::Button("Load"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
                    load = 1;

                if (ImGui::Button("Gone"))
                    gone = 1;
            }

            ImGui::InputTextWithHint(isGlobal ? "(global/ hint)" : "(cn/ hint)", "enter account name", savedName[i], IM_ARRAYSIZE(savedName[i]));

            if (ImGui::Button("Save to current Account") && strlen(savedName[i]))                            // Buttons return true when clicked (most widgets return true when edited/activated)
                save = 1;

            if (load) {
                if (RecoverAccount(isGlobal, loadedAccounts[i][item_current].blobAccount,
                                   loadedAccounts[i][item_current].blobData)) {
                    load = 0;
                }
            }
            if (gone) {
                std::vector<BYTE> blobAccount(1, 0), blobData(1, 0);
                if (RecoverAccount(isGlobal, blobAccount, blobData)) {
                    gone = 0;
                }
            }
            if (save) {
                std::vector<BYTE> blobAccount, blobData;
                if (BackupAccount(isGlobal, &blobAccount, &blobData)) {
                    AccountInfomation account;
                    account.name = savedName[i];
                    account.blobAccount = blobAccount;
                    account.blobData = blobData;
                    loadedAccounts[i].push_back(account);
                    loadedAccountNames[i].push_back(savedName[i]);
                    item_current = static_cast<int>(loadedAccounts[i].size()) - 1;
                    // save accounts to disk
                    SaveAccounts(loadedAccounts);
                    save = 0;
                }
            }
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
    ::UnregisterClass(wc.lpszClassName, wc.hInstance);

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
