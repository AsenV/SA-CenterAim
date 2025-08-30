// CenterAimASI.cpp
#include "plugin.h"
#include "CPlayerPed.h"
#include "CVehicle.h"
#include <windows.h>
#include <string>
#include <cstdio>
#include <cstdlib>

const uintptr_t ADDR_CROSS_X = 0x00B6EC10; // controla vertical (Y) no jogo (conforme seu mapeamento)
const uintptr_t ADDR_CROSS_Y = 0x00B6EC14; // controla horizontal (X)

static HMODULE g_hModule = nullptr;
static bool g_running = true;

// hotkeys
static const int KEY_UNLOAD = VK_END;
static const int KEY_RELOAD_INI = VK_F5;

// valores configuráveis (defaults)
static float g_posX = 0.5f;      // horizontal a pé (ADDR_CROSS_Y)
static float g_posY = 0.5f;      // vertical a pé (ADDR_CROSS_X)
static float g_vehPosX = 0.5f;   // horizontal no veículo (ADDR_CROSS_Y)
static float g_vehPosY = 0.3f;   // vertical no veículo (ADDR_CROSS_X)

// util: obtém pasta do módulo
static std::string GetModuleDir()
{
    char path[MAX_PATH];
    GetModuleFileNameA(g_hModule ? g_hModule : GetModuleHandleA(NULL), path, MAX_PATH);
    std::string s(path);
    size_t pos = s.find_last_of("\\/");
    if (pos != std::string::npos) s = s.substr(0, pos + 1);
    return s;
}

static void CreateDefaultIniIfNotExists(const std::string& fullpath)
{
    DWORD attrs = GetFileAttributesA(fullpath.c_str());
    if (attrs != INVALID_FILE_ATTRIBUTES) return;
    std::FILE* out = std::fopen(fullpath.c_str(), "w");
    if (!out) return;
    std::fprintf(out,
        "[CenterAim]\n"
        "PosX=0.5\n"
        "PosY=0.5\n"
        "VehPosX=0.5\n"
        "VehPosY=0.3\n"
    );
    std::fclose(out);
}

static float clamp01(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

static void LoadConfig()
{
    std::string dir = GetModuleDir();
    std::string full = dir + "CenterAim.ini";
    CreateDefaultIniIfNotExists(full);

    char buf[64];

    // PosX (horizontal a pé) -> ADDR_CROSS_Y
    GetPrivateProfileStringA("CenterAim", "PosX", "0.5", buf, sizeof(buf), full.c_str());
    g_posX = clamp01(static_cast<float>(atof(buf)));

    // PosY (vertical a pé) -> ADDR_CROSS_X
    GetPrivateProfileStringA("CenterAim", "PosY", "0.5", buf, sizeof(buf), full.c_str());
    g_posY = clamp01(static_cast<float>(atof(buf)));

    // VehPosX (horizontal no veículo) -> ADDR_CROSS_Y
    GetPrivateProfileStringA("CenterAim", "VehPosX", "0.5", buf, sizeof(buf), full.c_str());
    g_vehPosX = clamp01(static_cast<float>(atof(buf)));

    // VehPosY (vertical no veículo) -> ADDR_CROSS_X
    GetPrivateProfileStringA("CenterAim", "VehPosY", "0.3", buf, sizeof(buf), full.c_str());
    g_vehPosY = clamp01(static_cast<float>(atof(buf)));
}

// escreve float com proteção de página (retorna true se escrito)
static bool WriteFloatToAddr(uintptr_t addr, float value)
{
    DWORD oldProtect;
    if (!VirtualProtect((LPVOID)addr, sizeof(float), PAGE_EXECUTE_READWRITE, &oldProtect))
        return false;
    *(float*)(addr) = value;
    VirtualProtect((LPVOID)addr, sizeof(float), oldProtect, &oldProtect);
    return true;
}

DWORD WINAPI MainThread(LPVOID)
{
    LoadConfig();

    while (g_running)
    {
        // hotkeys
        if (GetAsyncKeyState(KEY_UNLOAD) & 1) break;
        if (GetAsyncKeyState(KEY_RELOAD_INI) & 1) LoadConfig();

        CPed* player = FindPlayerPed();
        if (!player) {
            Sleep(8);
            continue;
        }

        bool inVehicle = player->bInVehicle != 0;

        // Lembrete do mapeamento: ADDR_CROSS_X controla vertical (Y), ADDR_CROSS_Y controla horizontal (X)
        float desiredX = inVehicle ? g_vehPosX : g_posX; // horizontal (ADDR_CROSS_Y)
        float desiredY = inVehicle ? g_vehPosY : g_posY; // vertical   (ADDR_CROSS_X)

        // escrevemos sempre (garante volta ao centro ao sair do veículo)
        WriteFloatToAddr(ADDR_CROSS_Y, desiredX); // horizontal
        WriteFloatToAddr(ADDR_CROSS_X, desiredY); // vertical

        Sleep(8);
    }

    // finalize thread
    if (g_hModule)
        FreeLibraryAndExitThread(g_hModule, 0);

    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        g_hModule = hModule;
        DisableThreadLibraryCalls(hModule);
        CreateThread(nullptr, 0, MainThread, nullptr, 0, nullptr);
        break;
    case DLL_PROCESS_DETACH:
        g_running = false;
        break;
    }
    return TRUE;
}
