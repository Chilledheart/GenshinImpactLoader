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
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Dear Genshin Impact Loader", NULL, NULL);
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
    bool show_demo_account = true;
    bool show_another_account = false;
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

        // 1. Show the demo window.
        if (show_demo_account)
        {
            ImGui::Begin("Demo Account", &show_demo_account);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
            ImGui::Text("Hello from demo account!");
            if (ImGui::Button("Close Me"))
                show_demo_account = false;
            ImGui::End();
        }

        // 2. Show a simple window that we create ourselves. We use a Begin/End pair to created a named window.
        {
            static float f = 0.0f;
            static int counter = 0;

            ImGui::Begin("Welcome to Genshin Impact Loader!");                          // Create a window called "Hello, world!" and append into it.

            ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
            ImGui::Checkbox("Demo Account", &show_demo_account);      // Edit bools storing our window open/close state
            ImGui::Checkbox("Another Account", &show_another_account);

            ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
            ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

            if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
                counter++;
            ImGui::SameLine();
            ImGui::Text("counter = %d", counter);

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
            ImGui::End();
        }

        // 3. Show another simple account.
        if (show_another_account)
        {
            ImGui::Begin("Another Account", &show_another_account);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
            ImGui::Text("Hello from another account!");
            if (ImGui::Button("Close Me"))
                show_another_account = false;
            ImGui::End();
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
