/*
    MediaWin32
    Original: PE64, MSVC + C++/WinRT 2.0.200609.3
    ImageBase: 0x180000000

    Exports:
        RegisterCallbacks   0x180002350
        StartMediaMonitor   0x180002F00
        StopMediaMonitor    0x180002FE0
        MonitorThread       sub_180002370

    GameMaker integration:
        obj_fmod / Other_70.gml expects an async ds_map:
            {
                "type": "win_media_playback",
                "win_media_playback_status": 0.0 / 1.0
            }

        Event index 0x46 (70) = Async Social
*/

#include "pch.h"

#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include <process.h>
#include <objbase.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.Control.h>
#include <winrt/Windows.Media.h>

using namespace std;
using namespace winrt;
using namespace Windows::Media::Control;
using namespace Windows::Media;

#define GMEXPORT extern "C" __declspec(dllexport)

/*
    GameMaker Runner Callbacks
    Registered through: RegisterCallbacks(string, string, string, string)->real

    Original globals:
    qword_18000B268 / qword_18000B270 / qword_18000B278 / qword_18000B280

    Verified against disassembly:
    IDADISM.txt:2048:2055, 2206:2217, 2261:2297
*/ 
typedef int  (__cdecl* FN_CreateDsMap)(int num);
typedef bool (__cdecl* FN_DsMapAddDouble)(int map, const char* key, double value);
typedef bool (__cdecl* FN_DsMapAddString)(int map, const char* key, const char* value);
typedef void (__cdecl* FN_CreateAsyncEvent)(int map, int eventIndex);

static FN_CreateAsyncEvent g_CreateAsyncEvent = nullptr; // a1 | 0x18000B268
static FN_CreateDsMap g_CreateDsMap = nullptr;           // a2 | 0x18000B270
static FN_DsMapAddDouble g_DsMapAddDouble = nullptr;     // a3 | 0x18000B278
static FN_DsMapAddString g_DsMapAddString = nullptr;     // a4 | 0x18000B280

// original flag: byte_18000B220
static std::atomic<bool> g_bMonitoring{ false };

static void SendPlaybackStatus(double isPlaying)
{
    if (!g_CreateDsMap || !g_CreateAsyncEvent)
        return;

    /*
        CreateDsMap(int _num, ...) | original called it with rcx == 0
        (IDADISM.txt:2502 test rcx,rcx / jnz, then call). Passing garbage here corrupts the runner heap -> APPCRASH c0000005 in SuperBoNoise.exe.
        Must pass 0 explicitly.
    */
    int map = -1;
    try
    {
        map = g_CreateDsMap(0);
    }
    catch (...)
    {
        return;
    }

    if (map < 0)
        return;
    try
    {
        if (g_DsMapAddString)
            g_DsMapAddString(map, "type", "win_media_playback");
        if (g_DsMapAddDouble)
            g_DsMapAddDouble(map, "win_media_playback_status", isPlaying);
        
        // 0x46 = 70 = Other_70.gml Async Social
        g_CreateAsyncEvent(map, 70);
    }
    catch (...) { }
}

// original: sub_180002370, spawned via _beginthreadex + _Thrd_detach in StartMediaMonitor
static unsigned __stdcall MonitorThread(void*)
{
    cout << "mediaWin32 :: Initializing apartment..." << endl;

    /*
        the original dll did CoInitializeEx(NULL, 0).For C++ / WinRT.get() to be
        stable we must init the WinRT apartment, not just COM.
        This was the intermittent crash: raw CoInitializeEx + blocking
        .get() races RoInitialize on first SMTC call.
    */
    try
    {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
    }
    catch (...)
    {
        cout << "mediaWin32 :: Unknown error occurred." << endl;
        return 1;
    }

    // poll like the original: QueryPerformanceFrequency + _Thrd_sleep(500ms)
    constexpr auto kPollInterval = std::chrono::milliseconds(500);
    int lastSent = -1; // -1 = none yet, 0 / 1 = last status
    while (g_bMonitoring.load())
    {
        try
        {
            // original: sub_1800021F0 + sub_180002D30 = GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get()
            auto managerOp = GlobalSystemMediaTransportControlsSessionManager::RequestAsync();
            GlobalSystemMediaTransportControlsSessionManager manager = managerOp.get();

            auto session = manager.GetCurrentSession();
            int cur = 0;
            if (!session)
                cur = 0; // no session -> not playing (matches loc_1800024F7 path that still fires event)
            else
            {
                // original: [rax+48h] = GetPlaybackInfo, [rax+38h] = get_PlaybackStatus
                // IDADISM.txt:2432-2456, cmp var_80, 4
                auto playbackInfo = session.GetPlaybackInfo();

                // MediaPlaybackStatus::Playing == 4
                /*
                    Closed = 0
                    Opening = 1
                    Changing = 2
                    Stopped = 3
                    Playing = 4
                    Paused = 5
                */
                if (playbackInfo)
                    cur = (playbackInfo.PlaybackStatus() == GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing) ? 1 : 0;
            }

            // only fire on change, the original spammed every 500ms which can overflow the runner async queue to c0000005 at SuperBoNoise+0x161fc6.
            if (cur != lastSent && g_bMonitoring.load())
            {
                SendPlaybackStatus(cur ? 1.0 : 0.0);
                lastSent = cur;
            }
        }
        catch (const hresult_error&)
        {
            cout << "mediaWin32 :: Exception: " << endl;

            // original still fires 0.0 on exception paths, then continues loop
            if (g_bMonitoring.load() && lastSent != 0)
            {
                SendPlaybackStatus(0.0);
                lastSent = 0;
            }
        }
        catch (...)
        {
            cout << "mediaWin32 :: Exception: " << endl;
            if (g_bMonitoring.load() && lastSent != 0)
            {
                SendPlaybackStatus(0.0);
                lastSent = 0;
            }
        }

        // sleep in small slices so StopMediaMonitor reacts fast (the original did one 500ms _Thrd_sleep, we split into 10x50ms)
        for (int i = 0; i < 10 && g_bMonitoring.load(); ++i)
            std::this_thread::sleep_for(kPollInterval / 10);
    }

    winrt::uninit_apartment();

    // the original never called CoUninitialize (leaks MTA on purpose for gm)
    return 0;
}

GMEXPORT double RegisterCallbacks(char* a1, char* a2, char* a3, char* a4)
{
    // gm passes function pointers as args (declared as string in .yy)
    g_CreateAsyncEvent = reinterpret_cast<FN_CreateAsyncEvent>(a1);
    g_CreateDsMap = reinterpret_cast<FN_CreateDsMap>(a2);
    g_DsMapAddDouble = reinterpret_cast<FN_DsMapAddDouble>(a3);
    g_DsMapAddString = reinterpret_cast<FN_DsMapAddString>(a4);
    return 0.0;
}

GMEXPORT double StartMediaMonitor()
{
    cout << "mediaWin32 :: Starting Monitoring." << endl;

    bool expected = false;

    /*
        original: xchg al, byte_18000B220, only spawn once
        we use atomic compare_exchange to match
    */
    bool was = g_bMonitoring.exchange(true);
    if (!was)
    {
        // Detached like original (_beginthreadex + _Thrd_detach)
        uintptr_t h = _beginthreadex(nullptr, 0, MonitorThread, nullptr, 0, nullptr);
        if (h)
            CloseHandle(reinterpret_cast<HANDLE>(h));
    }
    return 1.0;
}

GMEXPORT double StopMediaMonitor()
{
    cout << "mediaWin32 :: Stopping Monitoring." << endl;
    bool was = g_bMonitoring.exchange(false);
    return was ? 1.0 : 0.0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
        DisableThreadLibraryCalls(hModule);
    return TRUE;
}
