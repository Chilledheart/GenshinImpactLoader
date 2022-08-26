#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

// [Win32] Our example includes a copy of glfw3.lib pre-compiled with VS2010 to maximize ease of testing and compatibility with old VS compilers.
// To link with VS2010-era libraries, VS2015+ requires linking with legacy_stdio_definitions.lib, which we do using this pragma.
// Your own project should not be affected, as you are likely to link with a newer binary of GLFW that is adequate for your version of Visual Studio.
#if defined(_MSC_VER) && (_MSC_VER >= 1900) && !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")
#endif

#pragma comment(lib, "glfw3")
#pragma comment(lib, "opengl32")

#include <vector>
#include <string>

#include <windows.h>

static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

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

  output->reserve(BufferSize);
  if (::RegQueryValueExW(hkey /* HKEY */, valueName /* lpValueName */,
                         nullptr /* lpReserved */, &type /* lpType */,
                         output->data() /* lpData */,
                         &BufferSize /* lpcbData */) != ERROR_SUCCESS) {
    return false;
  }
  output->resize(BufferSize);
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

#define DEFAULT_CONFIG_FILE "gl.data"

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

    while (fscanf(f, "%s %d %s %s\n", accnt.name, &accnt.isGlobal, (char*)accnt.account, (char*)accnt.userData) != 4) {
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

int WinMain(HINSTANCE hInstance,
            HINSTANCE hPrevInstance,
            LPSTR     lpCmdLine,
            int       nShowCmd)
{
    // Setup window
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

    // Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
    // GL ES 2.0 + GLSL 100
    const char* glsl_version = "#version 100";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(__APPLE__)
    // GL 3.2 + GLSL 150
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
#else
    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    //glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    //glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
#endif

    // Create window with graphics context
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Genshin Impact Loader", NULL, NULL);
    if (window == NULL)
        return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

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
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

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
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
    //IM_ASSERT(font != NULL);

    // Our state
    bool showCnAccounts = false;
    std::vector<AccountInfomation> loadedAccounts[2];
    std::vector<const char*> loadedAccountNames[2];

    // i = 0 -> CN Service
    // i = 1 -> Global Service
    // Load saved data from disk
    LoadSavedAccounts(loadedAccounts);

    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // Main loop
    while (!glfwWindowShouldClose(window))
    {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        glfwPollEvents();

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // 2. Show a simple window that we create ourselves. We use a Begin/End pair to created a named window.
        for (int i = 0, isGlobal = 0; i != 2; ++i, isGlobal = 1) {
            static int load = 0;
            static int save = 0;

            if (!isGlobal) {
                if (showCnAccounts) {
                    continue;
                }
                ImGui::Begin("Welcome to Genshin Impact CN Loader!", &showCnAccounts);
            } else {
                ImGui::Begin("Welcome to Genshin Impact Global Loader!");
            }

            ImGui::Text("Multi Account Switch");

            ImGui::Text("Choose existing account to load or save current account.");               // Display some text (you can use a format strings too)

            loadedAccountNames[i].clear();
            for (const auto& accnt : loadedAccounts[i]) {
              loadedAccountNames[i].push_back(accnt.name.c_str());
            }

            static int item_current = 0;
            if (!loadedAccounts[i].empty()) {
                const char** items = &loadedAccountNames[i][0];
                ImGui::ListBox("listbox", &item_current, items, IM_ARRAYSIZE(items), 4);
                ImGui::SameLine();

                if (ImGui::Button("Load from chosen account"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
                    load = 1;
            }

            static char savedName[128] = "";
            ImGui::InputTextWithHint("(w/ hint)", "enter account name", savedName, IM_ARRAYSIZE(savedName));

            if (ImGui::Button("Save to current Account"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
                save = 1;

            ImGui::End();

            if (load) {
              std::vector<BYTE> blobAccount, blobData;
              if (RecoverAccount(isGlobal, loadedAccounts[i][item_current].blobAccount,
                                 loadedAccounts[i][item_current].blobData)) {
                load = 0;
              }
            }
            if (save) {
              std::vector<BYTE> blobAccount, blobData;
              if (BackupAccount(isGlobal, &blobAccount, &blobData)) {
                AccountInfomation account;
                account.name = savedName;
                account.blobAccount = blobAccount;
                account.blobData = blobData;
                loadedAccounts[i].push_back(account);
                item_current = static_cast<int>(loadedAccounts[i].size()) - 1;
                // save accounts to disk
                SaveAccounts(loadedAccounts);
                save = 0;
              }
            }
        }

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
