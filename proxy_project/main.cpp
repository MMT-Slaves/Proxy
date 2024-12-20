// Dear ImGui: standalone example application for DirectX 11

// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp
#pragma once

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "Proxy.h"
#include "LogQueue.h"
#include "DomainTrie.h"
#include "CheckUTF.h"
#include <d3d11.h>
#include <tchar.h>
#include <string>
#include <fstream>
#include <time.h>
#include <iostream>

// Data
static ID3D11Device*            g_pd3dDevice = nullptr;
static ID3D11DeviceContext*     g_pd3dDeviceContext = nullptr;
static IDXGISwapChain*          g_pSwapChain = nullptr;
static bool                     g_SwapChainOccluded = false;
static UINT                     g_ResizeWidth = 0, g_ResizeHeight = 0;
static ID3D11RenderTargetView*  g_mainRenderTargetView = nullptr;

// Forward declarations of helper functions
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

#define MAX_SIZE 1000

// Global variables
char blocked_list[MAX_SIZE];       // Array to hold the blocked list
char last_blocked_list[MAX_SIZE];
std::string var;
int saved_blocked = 0;
time_t saved_time = 0;

int playing_port = 1008;
uint64_t selected_log_id = 0; // No log selected initially
uint64_t hovered_log_id = 0; // No log selected initially
const char blocklistFilename[] = "blocklist.txt";

class Application {

public:
    LogQueue* log_queue_global;
    HttpProxy* proxy;

    Application() : log_queue_global(nullptr), proxy(nullptr) {}

    Application(int port, const std::string& blocklist_file) {
        log_queue_global = new LogQueue();
        proxy = new HttpProxy(port, blocklist_file, log_queue_global);
    }

    void update_log() {
        while (true) {
            auto logs = (*log_queue_global).get_logs();
            for (const auto& log : logs) {
                std::cout << log.timestamp << " " << log.method << " "
                    << log.host << ":" << log.port << log.path
                    << " - Client IP: " << log.client_ip
                    << (log.blocked ? " [BLOCKED]" : "") << std::endl;
            }
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
    }

    void connect() {
        // Start the log update thread
        std::thread log_thread(&Application::update_log, this);
        log_thread.detach();

        // Start the proxy in a separate thread
        std::thread proxy_thread([this]() {
            proxy->start();
        });
        proxy_thread.detach();
    }

    ~Application() {
        delete log_queue_global;
        delete proxy;
    }

};
std::vector<char> selected_request_buffer;
std::vector<char> selected_response_buffer;

Application* app = nullptr;

void save_file_content(const std::string& filename, const std::string& content) {
    std::ofstream outfile(filename);
    if (!outfile) {
        std::cerr << "output fail" << "\n";
        return;
    }
    if (outfile.is_open()) {
        outfile << content;
        outfile.close();
    }
}

void RenderLogTable() {
    // Retrieve logs
    auto logs = app->log_queue_global->get_logs();

    // ImGui table
    ImGuiIO MyIO = ImGui::GetIO();
    ImGui::SetNextWindowSize(ImVec2(MyIO.DisplaySize.x - 20, MyIO.DisplaySize.y / 2));
    ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable |
        ImGuiTableFlags_ScrollX;

    if (ImGui::BeginTable("Request Logs", 8, flags))
    {
        // Table headers
        ImGui::TableSetupColumn("ID");
        ImGui::TableSetupColumn("Timestamp");
        ImGui::TableSetupColumn("Method");
        ImGui::TableSetupColumn("Host");
        ImGui::TableSetupColumn("Port");
        ImGui::TableSetupColumn("Path");
        ImGui::TableSetupColumn("Client IP");
        ImGui::TableSetupColumn("Status");
        ImGui::TableHeadersRow();

        // Render rows
        for (const auto& log : logs) {
            // Start a new row
            ImGui::TableNextRow();

            // Set initial column index
            ImGui::TableSetColumnIndex(0);

            // Create a unique identifier for the selectable
            std::string selectable_id = std::to_string(log.id);

            // Determine if this row is selected
            bool is_selected = (log.id == selected_log_id);

            // Create a selectable that spans the entire row
            ImGui::PushID(log.id);
            if (ImGui::Selectable(selectable_id.c_str(), is_selected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap, ImVec2(0, 0))) {
                // Row was clicked
                selected_log_id = log.id;

                // Update selected request buffer
                selected_request_buffer.assign(log.fullRequest.begin(), log.fullRequest.end());
                selected_request_buffer.push_back('\0'); // Null-terminate

                // Update selected response buffer
                selected_response_buffer.assign(log.fullResponse.begin(), log.fullResponse.end());
                selected_response_buffer.push_back('\0'); // Null-terminate
            }
            ImGui::PopID();

            // Check if the row is hovered
            bool is_hovered = ImGui::IsItemHovered();

            // Set background color based on log.blocked, selection, or hover
            if (log.blocked) {
                // Set background color to red if the log is blocked
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(200, 80, 82, 255)); // Black color
            }
            else if (is_selected) {
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32(ImVec4(1.0f, 0.5f, 0.0f, 0.65f))); // Intense highlight
            }
            else if (is_hovered) {
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32(ImVec4(0.8f, 0.8f, 0.8f, 0.65f))); // Light highlight
            }

            // Render cells
            //ImGui::TableSetColumnIndex(0);
            //ImGui::Text("%llu", log.id);

            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%s", log.timestamp.c_str());

            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%s", log.method.c_str());

            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%s", log.host.c_str());

            ImGui::TableSetColumnIndex(4);
            ImGui::Text("%d", log.port);

            ImGui::TableSetColumnIndex(5);
            ImGui::Text("%s", log.path.c_str());

            ImGui::TableSetColumnIndex(6);
            ImGui::Text("%s", log.client_ip.c_str());

            ImGui::TableSetColumnIndex(7);
            ImGui::Text("%s", log.blocked ? "BLOCKED" : "ALLOWED");
        }
        ImGui::EndTable();
    }
}

void RenderRequestResponse() {
    ImGuiIO MyIO = ImGui::GetIO();
    ImGui::Separator(); // Optional separator
    ImGui::Columns(2, "RequestResponseColumns"); // Create two columns

    // Request Text Box
    ImGui::Text("Request");
    ImGui::InputTextMultiline("##Request",
        selected_request_buffer.data(),
        selected_request_buffer.size(),
        ImVec2(-1, MyIO.DisplaySize.y - ImGui::GetCursorPos().y - 10),
        ImGuiInputTextFlags_ReadOnly);

    ImGui::NextColumn();

    // Response Text Box
    ImGui::Text("Response");
    ImGui::InputTextMultiline("##Response",
        selected_response_buffer.data(),
        selected_response_buffer.size(),
        ImVec2(-1, MyIO.DisplaySize.y - ImGui::GetCursorPos().y - 10),
        ImGuiInputTextFlags_ReadOnly);

    ImGui::Columns(1); // End columns

}

// Main code
int main(int, char**)
{
    // Create application window
    //ImGui_ImplWin32_EnableDpiAwareness();
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"PwnHub Proxy", nullptr };
    ::RegisterClassExW(&wc);
    HWND hwnd = CreateWindowW(wc.lpszClassName, L"PwnHub Proxy", WS_OVERLAPPEDWINDOW, 700, 300, 500, 500, nullptr, nullptr, wc.hInstance, nullptr);

    // Initialize Direct3D
    if (!CreateDeviceD3D(hwnd))
    {
        std::cerr << "Create Device Failed\n";
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    // Show the window
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return a nullptr. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use Freetype for higher quality font rendering.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    //io.Fonts->AddFontDefault();
    //io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf", 18.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, nullptr, io.Fonts->GetGlyphRangesJapanese());
    //IM_ASSERT(font != nullptr);

    // Main loop
    bool done = false;
    bool started = false;

    // Initialize buffers with a null terminator
    selected_request_buffer.push_back('\0');
    selected_response_buffer.push_back('\0');

    std::string blocked_input;
    if (loadFileContent(blocklistFilename, blocked_input)) {
        for (int i = 0; i < blocked_input.size(); i++) {
            blocked_list[i] = blocked_input[i];
            last_blocked_list[i] = blocked_input[i];
        }
        save_file_content(blocklistFilename, blocked_input);
    }

    while (!done)
    {
        // Poll and handle messages (inputs, window resize, etc.)
        // See the WndProc() function below for our to dispatch events to the Win32 backend.
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        // Handle window being minimized or screen locked
        if (g_SwapChainOccluded && g_pSwapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED)
        {
            ::Sleep(10);
            continue;
        }
        g_SwapChainOccluded = false;

        // Handle window resize (we don't resize directly in the WM_SIZE handler)
        if (g_ResizeWidth != 0 && g_ResizeHeight != 0)
        {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            g_ResizeWidth = g_ResizeHeight = 0;
            CreateRenderTarget();
        }

        // Start the Dear ImGui frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        {
            ImGuiIO MyIO = ImGui::GetIO();
            ImGui::SetNextWindowSize(ImVec2(MyIO.DisplaySize.x + 10, MyIO.DisplaySize.y + 10));
            ImGui::SetNextWindowPos(ImVec2(0, 0));
            ImGui::Begin("PwnHub Proxy", NULL, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);

            // Start button
            if (!started) {
                // Show input text box for port number in middle on window
                ImGui::SetCursorPosX((MyIO.DisplaySize.x - 300) / 2);
                ImGui::SetCursorPosY((MyIO.DisplaySize.y - 100) / 2);
                ImGui::Text("Enter port number: ");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(100);
                ImGui::InputInt("##Port number", &playing_port);

                ImGui::SameLine();
                if (ImGui::Button("Start")) {
                    started = true;
                    app = new Application(playing_port, blocklistFilename);
                    app->connect();
                }
            }
            else {
                if (ImGui::CollapsingHeader("Blocked List")) {
                    // Multiline text for blocked list domain
                    ImGui::Text("Blocked sites list: ");
                    bool is_modified = strcmp(blocked_list, last_blocked_list) != 0;

                    if (is_modified) {
                        ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(62, 71, 158, 255));
                    }

                    ImGui::SetNextWindowSize(ImVec2(MyIO.DisplaySize.x - 10, MyIO.DisplaySize.y / 6));
                    ImGui::InputTextMultiline("##Blocked sites list", blocked_list, IM_ARRAYSIZE(blocked_list), ImVec2(0, 0));

                    if (is_modified) {
                        ImGui::PopStyleColor();
                    }


                    // Save button
                    if (ImGui::Button("Save")) {
                        var = std::string(blocked_list);              // Save buffer to var
                        if (isUTF7Encoded(var)) {
                            save_file_content(blocklistFilename, var);    // Save var to blocklist.txt
                            app->proxy->load_blocked_sites(blocklistFilename);
                            saved_blocked = 1;
                            saved_time = time(0);
                            for (int i = 0; i < MAX_SIZE; i++) {
                                last_blocked_list[i] = blocked_list[i];
                            }
                        }
                        else {
                            saved_blocked = -1;
                            saved_time = time(0);
                            for (int i = 0; i < MAX_SIZE; i++) {
                                blocked_list[i] = last_blocked_list[i];
                            }
                        }
                    }
                    if (saved_blocked) {
                        if (saved_blocked == 1 && saved_time - time(0) >= -1) {
                            ImGui::SameLine();
                            ImGui::Text("Saved");
                        }
                        else if (saved_blocked == -1 && saved_time - time(0) >= -1) {
                            ImGui::SameLine();
                            ImGui::Text("Invalid UTF-8 characters");
                        }
                        else {
                            saved_blocked = 0;
                        }
                    }

                }
                ImGui::BeginChild("Blocked List and Log Table", ImVec2(0, MyIO.DisplaySize.y / 3), ImGuiChildFlags_ResizeY);
                RenderLogTable();
                ImGui::EndChild();
                RenderRequestResponse();

            }
            
            ImGui::End();
        }

        // Rendering
        ImGui::Render();
        const float clear_color_with_alpha[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        // Present
        HRESULT hr = g_pSwapChain->Present(1, 0);   // Present with vsync
        //HRESULT hr = g_pSwapChain->Present(0, 0); // Present without vsync
        g_SwapChainOccluded = (hr == DXGI_STATUS_OCCLUDED);
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
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED) // Try high-performance WARP software driver if hardware is not available.
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK)
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
        if (wParam == SIZE_MINIMIZED)
            return 0;
        g_ResizeWidth = (UINT)LOWORD(lParam); // Queue resize
        g_ResizeHeight = (UINT)HIWORD(lParam);
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}
