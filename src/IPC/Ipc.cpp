#include "Ipc.h"
#include <debugapi.h>
#include <handleapi.h>
#include <minwindef.h>
#include <utility>
#include <winnt.h>
#include <winuser.h>
#include "Globals.h"
#include "FanyDefines.h"
#include "MetasequoiaIME.h"
#include <fmt/xchar.h>

static HANDLE hMapFile = nullptr;
static void *pBuf;
static FanyImeSharedMemoryData *sharedData;
static bool canUseSharedMemory = false;

static HANDLE hPipe = nullptr;
static HANDLE hFromServerPipe = nullptr;

static FanyImeNamedpipeData namedpipeData = {};
FanyImeNamedpipeDataToTsf namedpipeDataFromServer = {};

/* Data size transfered from Server process */
static const int ServerDtPipeDataSize = 512;

namespace
{
inline bool IsValidPipeHandle(HANDLE hPipeHandle)
{
    return hPipeHandle != nullptr && hPipeHandle != INVALID_HANDLE_VALUE;
}

void ClosePipeHandleIfValid(HANDLE &hPipeHandle)
{
    if (IsValidPipeHandle(hPipeHandle))
    {
        CloseHandle(hPipeHandle);
    }
    hPipeHandle = nullptr;
}

bool TryOpenClientPipe(HANDLE &hPipeHandle, const wchar_t *pipeName)
{
    if (IsValidPipeHandle(hPipeHandle))
    {
        return true;
    }

    HANDLE openedPipe = CreateFile(   //
        pipeName,                     //
        GENERIC_READ | GENERIC_WRITE, //
        0,                            //
        nullptr,                      //
        OPEN_EXISTING,                //
        0,                            //
        nullptr                       //
    );

    if (openedPipe == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    hPipeHandle = openedPipe;
    return true;
}
} // namespace

int InitIpc()
{
    //
    // Shared memory, open here
    //
    hMapFile = OpenFileMappingW( //
        FILE_MAP_ALL_ACCESS,     //
        FALSE,                   //
        FANY_IME_SHARED_MEMORY   //
    );

    //
    // Shared memory is not available, try to use namedpipe
    //
    InitNamedpipe();

    if (!hMapFile)
    {
        // Error handling
        canUseSharedMemory = false;

        // TODO: Log error

        return 0;
    }

    bool alreadyExists = (GetLastError() == ERROR_ALREADY_EXISTS);

    pBuf = MapViewOfFile(    //
        hMapFile,            //
        FILE_MAP_ALL_ACCESS, //
        0,                   //
        0,                   //
        BUFFER_SIZE          //
    );                       //

    if (!pBuf)
    {
        // TODO:  Error handling
    }

    sharedData = static_cast<FanyImeSharedMemoryData *>(pBuf);
    // Only initialize the shared memory when first created
    if (!alreadyExists)
    {
        // Initialize
        *sharedData = {};
        sharedData->point[0] = 100;
        sharedData->point[1] = 100;
    }

    return 0;
}

int InitNamedpipe()
{
    return ConnectToAllNamedpipe();
}

int ConnectToAllNamedpipe()
{
    bool mainPipeReady = TryOpenClientPipe(hPipe, FANY_IME_NAMED_PIPE);
    bool toTsfPipeReady = TryOpenClientPipe(hFromServerPipe, FANY_IME_TO_TSF_NAMED_PIPE);
    bool toTsfWorkerPipeReady =
        TryOpenClientPipe(Global::hToTsfWorkerThreadPipe, FANY_IME_TO_TSF_WORKER_THREAD_NAMED_PIPE);

    return (mainPipeReady && toTsfPipeReady && toTsfWorkerPipeReady) ? 1 : 0;
}

int ConnectToTsfNamedpipe()
{
    return TryOpenClientPipe(hFromServerPipe, FANY_IME_TO_TSF_NAMED_PIPE) ? 1 : 0;
}

int CloseIpc()
{
    //
    // Namedpipe
    //
    CloseNamedpipe();

    if (!canUseSharedMemory)
    {
        return -1;
    }

    //
    // Shared memory
    //
    if (pBuf)
    {
        UnmapViewOfFile(pBuf);
        pBuf = nullptr;
    }

    if (hMapFile)
    {
        CloseHandle(hMapFile);
        hMapFile = nullptr;
    }

    //
    // Events
    //
    for (const auto &eventName : FANY_IME_EVENT_ARRAY)
    {
        HANDLE hEvent = OpenEventW( //
            EVENT_ALL_ACCESS,       //
            FALSE,                  //
            eventName.c_str()       //
        );                          //
        if (hEvent)
        {
            CloseHandle(hEvent);
        }
    }

    return 0;
}

int CloseNamedpipe()
{
    ClosePipeHandleIfValid(hPipe);
    ClosePipeHandleIfValid(hFromServerPipe);
    ClosePipeHandleIfValid(Global::hToTsfWorkerThreadPipe);
    return 0;
}

int WriteDataToSharedMemory(           //
    UINT keycode,                      //
    WCHAR wch,                         //
    UINT modifiers_down,               //
    const int point[2],                //
    int pinyin_length,                 //
    const std::wstring &pinyin_string, //
    UINT write_flag                    //
)
{
    if (!canUseSharedMemory)
    {
        return WriteDataToNamedPipe(keycode, wch, modifiers_down, point, pinyin_length, pinyin_string, write_flag);
    }

    if (write_flag >> 0 & 1u)
    {
        sharedData->keycode = keycode;
    }

    if (write_flag >> 1 & 1u)
    {
        sharedData->wch = wch;
    }

    if (write_flag >> 2 & 1u)
    {
        sharedData->modifiers_down = modifiers_down;
    }

    if (write_flag >> 3 & 1u)
    {
        sharedData->point[0] = point[0];
        sharedData->point[1] = point[1];
    }

    if (write_flag >> 4 & 1u)
    {
        sharedData->pinyin_length = pinyin_length;
    }

    if (write_flag >> 5 & 1u)
    {
        wcscpy_s(sharedData->pinyin_string, pinyin_string.c_str());
        sharedData->pinyin_string[pinyin_length] = L'\0';
    }

    return 0;
}

/**
 * @brief
 *
 * @param keycode
 * @param modifiers_down
 * @param point
 * @param pinyin_length
 * @param pinyin_string
 * @param write_flag
 * @return int
 */
int WriteDataToNamedPipe(              //
    UINT keycode,                      //
    WCHAR wch,                         //
    UINT modifiers_down,               //
    const int point[2],                //
    int pinyin_length,                 //
    const std::wstring &pinyin_string, //
    UINT write_flag                    //
)
{
    if (write_flag >> 0 & 1u)
    {
        namedpipeData.keycode = keycode;
    }

    if (write_flag >> 1 & 1u)
    {
        namedpipeData.wch = wch;
    }

    if (write_flag >> 2 & 1u)
    {
        namedpipeData.modifiers_down = modifiers_down;
    }

    if (write_flag >> 3 & 1u)
    {
        namedpipeData.point[0] = point[0];
        namedpipeData.point[1] = point[1];
    }

    if (write_flag >> 4 & 1u)
    {
        namedpipeData.pinyin_length = pinyin_length;
    }

    if (write_flag >> 5 & 1u)
    {
        wcscpy_s(namedpipeData.pinyin_string, pinyin_string.c_str());
        namedpipeData.pinyin_string[pinyin_length] = L'\0';
    }

    return 0;
}

int SendKeyEventToUIProcess()
{
    if (!canUseSharedMemory)
    {
        return SendKeyEventToUIProcessViaNamedPipe();
    }

    HANDLE hEvent = OpenEventW(         //
        EVENT_MODIFY_STATE,             //
        FALSE,                          //
        FANY_IME_EVENT_ARRAY[0].c_str() //
    );                                  //

    if (!hEvent)
    {
        // TODO: Error handling
    }

    if (!SetEvent(hEvent))
    {
        // TODO: Error handling
    }

    CloseHandle(hEvent);
    return 0;
}

int SendHideCandidateWndEventToUIProcess()
{
    if (!canUseSharedMemory)
    {
        return SendHideCandidateWndEventToUIProcessViaNamedPipe();
    }

    HANDLE hEvent = OpenEventW(         //
        EVENT_MODIFY_STATE,             //
        FALSE,                          //
        FANY_IME_EVENT_ARRAY[1].c_str() // FanyHideCandidateWndEvent
    );                                  //

    if (!hEvent)
    {
        // TODO: Error handling
    }

    if (!SetEvent(hEvent))
    {
        // TODO: Error handling
    }

    CloseHandle(hEvent);
    return 0;
}

int SendShowCandidateWndEventToUIProcess()
{
    if (!canUseSharedMemory)
    {
        return SendShowCandidateWndEventToUIProcessViaNamedPipe();
    }

    HANDLE hEvent = OpenEventW(         //
        EVENT_MODIFY_STATE,             //
        FALSE,                          //
        FANY_IME_EVENT_ARRAY[2].c_str() // FanyShowCandidateWndEvent
    );                                  //

    if (!hEvent)
    {
        // TODO: Error handling
    }

    if (!SetEvent(hEvent))
    {
        // TODO: Error handling
    }

    CloseHandle(hEvent);
    return 0;
}

int SendMoveCandidateWndEventToUIProcess()
{
    if (!canUseSharedMemory)
    {
        return SendMoveCandidateWndEventToUIProcessViaNamedPipe();
    }

    HANDLE hEvent = OpenEventW(         //
        EVENT_MODIFY_STATE,             //
        FALSE,                          //
        FANY_IME_EVENT_ARRAY[3].c_str() // FanyMoveCandidateWndEvent
    );                                  //

    if (!hEvent)
    {
        // TODO: Error handling
    }

    if (!SetEvent(hEvent))
    {
        // TODO: Error handling
    }

    CloseHandle(hEvent);
    return 0;
}

int SendLangbarRightClickEventToUIProcess(const RECT *prcArea)
{
    if (!canUseSharedMemory)
    {
        return SendLangbarRightClickEventToUIProcessViaNamedPipe(prcArea);
    }
    return 0;
}

//
// Named pipe
//

void SendToNamedpipe()
{
    if (!hPipe || hPipe == INVALID_HANDLE_VALUE) // Try to reconnect
    {
        hPipe = CreateFile(               //
            FANY_IME_NAMED_PIPE,          //
            GENERIC_READ | GENERIC_WRITE, //
            0,                            //
            nullptr,                      //
            OPEN_EXISTING,                //
            0,                            //
            nullptr                       //
        );

        if (hPipe == INVALID_HANDLE_VALUE)
        {
            // TODO: Log
            return;
        }
        else
        {
            // TODO: Log
        }
    }

    DWORD bytesWritten = 0;
    BOOL ret = WriteFile(      //
        hPipe,                 //
        &namedpipeData,        //
        sizeof(namedpipeData), //
        &bytesWritten,         //
        NULL                   //
    );
    if (!ret || bytesWritten != sizeof(namedpipeData))
    {
        /* Error handling: 必须将 hPipe 置为无效，否则将留下脏句柄，导致有些情况下无法再次连接 */
        DWORD err = GetLastError();
        // if (err == ERROR_BROKEN_PIPE || err == ERROR_NO_DATA)
        // {
        /* 将两个方向的管道同时置为无效 */
        CloseHandle(hPipe);
        CloseHandle(hFromServerPipe);
        // CloseHandle(Global::hToTsfWorkerThreadPipe);
        hPipe = INVALID_HANDLE_VALUE;
        hFromServerPipe = INVALID_HANDLE_VALUE;
        // Global::hToTsfWorkerThreadPipe = INVALID_HANDLE_VALUE;
        /* 向 Server 端发送 kill 同时重置两个管道的 connect */
        SendToAuxNamedpipe(L"kill");
        OutputDebugString(fmt::format(L"[msime]: SendToNamedpipe failed eventually01.").c_str());
        // }

        //
        // 等待 Server 准备好，再重连几次
        //

        /* toTsfNamedPipe 放到消息窗口的线程中去处理 */
        PostMessage(Global::msgWndHandle, WM_ConnectToTsfNamedpipe, 0, 0);
        for (int i = 0; i < 10; ++i)
        {
            Sleep(10);
            hPipe = CreateFile(               //
                FANY_IME_NAMED_PIPE,          //
                GENERIC_READ | GENERIC_WRITE, //
                0,                            //
                nullptr,                      //
                OPEN_EXISTING,                //
                0,                            //
                nullptr                       //
            );
            if (hPipe != INVALID_HANDLE_VALUE)
                break;
        }

        if (hPipe == INVALID_HANDLE_VALUE)
        {
            // TODO: Log
            OutputDebugString(fmt::format(L"[msime]: SendToNamedpipe failed eventually02.").c_str());
            return;
        }
        else
        {
            // TODO: Log
        }

        DWORD bytesWritten = 0;
        BOOL ret = WriteFile(      //
            hPipe,                 //
            &namedpipeData,        //
            sizeof(namedpipeData), //
            &bytesWritten,         //
            NULL                   //
        );

        if (!ret || bytesWritten != sizeof(namedpipeData))
        {
            // TODO: Error handling
            OutputDebugString(fmt::format(L"[msime]: SendToNamedpipe failed eventually03.").c_str());
        }

        return;
    }
}

/**
 * @brief Clear namedpipe data if exists, cause sometimes there may be some useless data sent by last key event from
 * server
 *
 */
void ClearNamedpipeDataIfExists()
{
    bool isSpaceOrNumber = Global::Keycode == VK_SPACE ||                      //
                           (Global::Keycode > '0' && Global::Keycode < '9') || //
                           (Global::CommitWithFirstCandPunc.count(Global::wch) > 0);
    /* Only clear namedpipe data if keycode is space or number, for better performance */
    if (!isSpaceOrNumber)
    {
        return;
    }

    if (!hFromServerPipe || hFromServerPipe == INVALID_HANDLE_VALUE) // Try to reconnect
    {
        hFromServerPipe = CreateFile(     //
            FANY_IME_TO_TSF_NAMED_PIPE,   //
            GENERIC_READ | GENERIC_WRITE, //
            0,                            //
            nullptr,                      //
            OPEN_EXISTING,                //
            0,                            //
            nullptr                       //
        );
        if (hFromServerPipe == INVALID_HANDLE_VALUE)
        {
            // TODO: Log
            return;
        }
        else
        {
            // TODO: Log
        }
    }

    DWORD bytesAvailable = 0;
    DWORD bytesRead = 0;
    while (true)
    {
        BOOL peekOk = PeekNamedPipe( //
            hFromServerPipe,         //
            nullptr,                 //
            0,                       //
            nullptr,                 //
            &bytesAvailable,         //
            nullptr                  //
        );

        if (!peekOk || bytesAvailable == 0)
        {
            break; // no more data or pipe error
        }

        BOOL readOk = ReadFile(              //
            hFromServerPipe,                 //
            &namedpipeDataFromServer,        //
            sizeof(namedpipeDataFromServer), //
            &bytesRead,                      //
            NULL                             //
        );

        if (!readOk || bytesRead == 0)
        {
            break;
        }
    }
}

/**
 * @brief Try to read selected candiate string data from server pipe with timeout
 *
 * @return struct FanyImeNamedpipeDataToTsf*
 */
struct FanyImeNamedpipeDataToTsf *TryReadDataFromServerPipeWithTimeout()
{
    std::pair<UINT, std::wstring> ret = {0, L""};
    int timeoutMs = 10; // Default timeout 10ms
    int intervalMs = 1; // Default interval 1ms

    if (!hFromServerPipe || hFromServerPipe == INVALID_HANDLE_VALUE) // Try to reconnect
    {
        hFromServerPipe = CreateFile(     //
            FANY_IME_TO_TSF_NAMED_PIPE,   //
            GENERIC_READ | GENERIC_WRITE, //
            0,                            //
            nullptr,                      //
            OPEN_EXISTING,                //
            0,                            //
            nullptr                       //
        );
        if (hFromServerPipe == INVALID_HANDLE_VALUE)
        {
            // TODO: Log
            namedpipeDataFromServer.msg_type = Global::DataFromServerMsgType::Normal;
            // wcscpy_s(namedpipeDataFromServer.candidate_string, L"PipeOpenError");
            wcscpy_s(namedpipeDataFromServer.candidate_string, L"X");
            return &namedpipeDataFromServer;
        }
        else
        {
            // TODO: Log
        }
    }

    DWORD bytesAvailable = 0;
    int waited = 0;
    while (waited < timeoutMs)
    {
// TODO: Do not log
#ifdef FANY_DEBUG
        OutputDebugString(fmt::format(L"[msime]: current waited: {}", waited).c_str());
#endif
        if (PeekNamedPipe(hFromServerPipe, nullptr, 0, nullptr, &bytesAvailable, nullptr) && bytesAvailable > 0)
        {
            auto ret = ReadDataFromServerViaNamedPipe();
#ifdef FANY_DEBUG
            OutputDebugString(fmt::format(L"[msime]: PeekNamedPipe: {}", waited).c_str());
#endif
            return ret;
        }
        Sleep(intervalMs); // TODO: Maybe could use less time
        waited += intervalMs;
    }

    /* Error handling: 必须将 hFromServerPipe 置为无效，否则将留下脏句柄，导致有些情况下无法再次连接 */
    CloseHandle(hFromServerPipe);
    hFromServerPipe = INVALID_HANDLE_VALUE;

    namedpipeDataFromServer.msg_type = Global::DataFromServerMsgType::Normal;
    // Pipe timeout error
    wcscpy_s(namedpipeDataFromServer.candidate_string, L"T");
    return &namedpipeDataFromServer;
}

/**
 * @brief Read data sent by server
 *
 * TODO: Cancel when time exceed, we should set a timeout
 *
 * @return struct FanyImeNamedpipeDataToTsf*
 */
struct FanyImeNamedpipeDataToTsf *ReadDataFromServerViaNamedPipe()
{
    if (!hFromServerPipe || hFromServerPipe == INVALID_HANDLE_VALUE) // Try to reconnect
    {
        hFromServerPipe = CreateFile(     //
            FANY_IME_TO_TSF_NAMED_PIPE,   //
            GENERIC_READ | GENERIC_WRITE, //
            0,                            //
            nullptr,                      //
            OPEN_EXISTING,                //
            0,                            //
            nullptr                       //
        );
        if (hFromServerPipe == INVALID_HANDLE_VALUE)
        {
            // TODO: Log
            namedpipeDataFromServer.msg_type = 0;
            // wcscpy_s(namedpipeDataFromServer.candidate_string, L"PipeOpenError");
            wcscpy_s(namedpipeDataFromServer.candidate_string, L"X");
            return &namedpipeDataFromServer;
        }
        else
        {
            // TODO: Log
        }
    }

    DWORD bytesRead = 0;
    BOOL readResult = ReadFile(          //
        hFromServerPipe,                 //
        &namedpipeDataFromServer,        //
        sizeof(namedpipeDataFromServer), //
        &bytesRead,                      //
        NULL                             //
    );
    if (!readResult || bytesRead == 0) // Disconnected or error
    {
        // TODO: Log
    }
    else
    {
#ifdef FANY_DEBUG
        OutputDebugString(fmt::format(L"[msime]: ReadDataFromServerViaNamedPipe: {}", //
                                      namedpipeDataFromServer.candidate_string)
                              .c_str());
#endif
        return &namedpipeDataFromServer;
    }

    namedpipeDataFromServer.msg_type = 0;
    wcscpy_s(namedpipeDataFromServer.candidate_string, L"ReadDataError");
    return &namedpipeDataFromServer;
}

/**
 * @brief 即用即断，不要求一直连接，但是，要让 Server 端的管道一直在 connect 状态阻塞住等待连接
 *
 * @param pipeData
 */
void SendToAuxNamedpipe(std::wstring pipeData)
{
    HANDLE hAuxPipe = INVALID_HANDLE_VALUE;

    // 重试几次，等待 Server 准备好
    for (int retry = 0; retry < 5; ++retry)
    {
        hAuxPipe = CreateFileW(           //
            FANY_IME_AUX_NAMED_PIPE,      //
            GENERIC_READ | GENERIC_WRITE, //
            0,                            //
            nullptr,                      //
            OPEN_EXISTING,                //
            0,                            //
            nullptr                       //
        );
        if (hAuxPipe && hAuxPipe != INVALID_HANDLE_VALUE)
        {
            break;
        }
        /* 为了解决 Windows Media Player 会在启动时调用两次 kill 并且第二次失败的问题
         * TODO: 在 msg wnd proc 中去处理打开程序会调用两次 kill 的问题，将其优化成一次
         */
        Sleep(20); // 等待 20ms 再重试
    }
    if (!hAuxPipe || hAuxPipe == INVALID_HANDLE_VALUE)
    {
        OutputDebugString(fmt::format(L"[msime]: SendToAuxNamedpipe: PipeOpenError: {}", pipeData).c_str());
        return;
    }
    DWORD bytesWritten = 0;
    BOOL ret = WriteFile(                    //
        hAuxPipe,                            //
        pipeData.c_str(),                    //
        pipeData.length() * sizeof(wchar_t), //
        &bytesWritten,                       //
        NULL                                 //
    );
    if (!ret || bytesWritten != pipeData.length() * sizeof(wchar_t))
    {
        // TODO: Error handling
    }
    CloseHandle(hAuxPipe);
}

/**
 * event_type
 *   0: FanyImeKeyEvent
 *   1: FanyHideCandidateWndEvent
 *   2: FanyShowCandidateWndEvent
 *   3: FanyMoveCandidateWndEvent
 */
int SendKeyEventToUIProcessViaNamedPipe()
{
    namedpipeData.event_type = 0;
    SendToNamedpipe();

    return 0;
}

int SendHideCandidateWndEventToUIProcessViaNamedPipe()
{
    namedpipeData.event_type = 1;
    SendToNamedpipe();

    return 0;
}

int SendShowCandidateWndEventToUIProcessViaNamedPipe()
{
    namedpipeData.event_type = 2;
    SendToNamedpipe();

    return 0;
}

int SendMoveCandidateWndEventToUIProcessViaNamedPipe()
{
    namedpipeData.event_type = 3;
    SendToNamedpipe();

    return 0;
}

int SendLangbarRightClickEventToUIProcessViaNamedPipe(const RECT *prcArea)
{
    namedpipeData.event_type = 4;
    /* 利用其他的字段，把图标的坐标传递过去 */
    namedpipeData.point[0] = prcArea->left;
    namedpipeData.point[1] = prcArea->top;
    namedpipeData.keycode = prcArea->right;
    namedpipeData.modifiers_down = prcArea->bottom;
    SendToNamedpipe();

    return 0;
}

int SendIMEActivationEventToUIProcessViaNamedPipe()
{
    SendToAuxNamedpipe(L"IMEActivation");

    return 0;
}

/**
 * @brief Send IME status to UI process via named pipe
 *
 * @param kbdIsOpen
 * @param fullwidthIsOpen
 * @param puncIsOpen
 * @return int
 */
int SendIMEStatusEventToUIProcessViaNamedPipe(bool kbdIsOpen, bool fullwidthIsOpen, bool puncIsOpen)
{
    std::wstring status = L"ftbStatus";
    if (kbdIsOpen)
    {
        status += L"1";
    }
    else
    {
        status += L"0";
    }
    if (fullwidthIsOpen)
    {
        status += L"1";
    }
    else
    {
        status += L"0";
    }
    if (puncIsOpen)
    {
        status += L"1";
    }
    else
    {
        status += L"0";
    }
    SendToAuxNamedpipe(status);
    return 0;
}

int SendIMEDeactivationEventToUIProcessViaNamedPipe()
{
    SendToAuxNamedpipe(L"IMEDeactivation");

    return 0;
}

int SendIMESwitchEventToUIProcessViaNamedPipe(UINT uImeStatus)
{
    namedpipeData.event_type = 7;
    /* 利用其他的字段，把 IME 的中英状态传递过去 */
    namedpipeData.keycode = uImeStatus;
    SendToNamedpipe();

    return 0;
}

int SendPuncSwitchEventToUIProcessViaNamedPipe(BOOL isPunc)
{
    namedpipeData.event_type = 8;
    /* 利用其他的字段，把标点符号的中英状态传递过去 */
    namedpipeData.keycode = isPunc;
    SendToNamedpipe();

    return 0;
}

int SendDoubleSingleByteSwitchEventToUIProcessViaNamedPipe(BOOL isDoubleSingleByte)
{
    namedpipeData.event_type = 9;
    /* 利用其他的字段，把全角/半角的状态传递过去 */
    namedpipeData.keycode = isDoubleSingleByte;
    SendToNamedpipe();

    return 0;
}
