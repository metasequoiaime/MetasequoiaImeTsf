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

#ifdef FANY_IPC_DEBUG
#define FANY_IPC_LOG_RAW(message) OutputDebugString(message)
#define FANY_IPC_LOGW(message) OutputDebugString((message).c_str())
#define FANY_IPC_LOGF(...) OutputDebugString(fmt::format(__VA_ARGS__).c_str())
#else
#define FANY_IPC_LOG_RAW(message) ((void)0)
#define FANY_IPC_LOGW(message) ((void)0)
#define FANY_IPC_LOGF(...) ((void)0)
#endif

static HANDLE hMapFile = nullptr;
static void *pBuf;
static FanyImeSharedMemoryData *sharedData;
static bool canUseSharedMemory = false;

static thread_local HANDLE hPipe = nullptr;
static thread_local HANDLE hFromServerPipe = nullptr;
static thread_local HANDLE hToTsfWorkerThreadPipe = nullptr;

static thread_local FanyImeNamedpipeData namedpipeData = {};
static thread_local FanyImeNamedpipeDataToTsf namedpipeDataFromServer = {};

/* Data size transfered from Server process */
static const int ServerDtPipeDataSize = 512;

namespace
{
inline bool IsValidPipeHandle(HANDLE hPipeHandle)
{
    return hPipeHandle != nullptr && hPipeHandle != INVALID_HANDLE_VALUE;
}

double GetElapsedMilliseconds(const LARGE_INTEGER &startCounter, const LARGE_INTEGER &endCounter,
                              const LARGE_INTEGER &frequency)
{
    return static_cast<double>(endCounter.QuadPart - startCounter.QuadPart) * 1000.0 /
           static_cast<double>(frequency.QuadPart);
}

uint64_t GetPipeClientId()
{
    thread_local const uint64_t clientId =
        (static_cast<uint64_t>(GetCurrentProcessId()) << 32) | static_cast<uint64_t>(GetCurrentThreadId());
    return clientId;
}

void LogCreateFileFailure(const wchar_t *pipeName)
{
    FANY_IPC_LOGF(L"[msime]: [ipc] CreateFile failed: pipe={}, gle={}", pipeName, GetLastError());
}

void LogCreateFileSuccess(const wchar_t *pipeName)
{
    FANY_IPC_LOGF(L"[msime]: [ipc] CreateFile connected: pipe={}", pipeName);
}

void LogWriteFailure(const wchar_t *pipeName, DWORD bytesWritten, DWORD expectedBytes)
{
    FANY_IPC_LOGF(L"[msime]: [ipc] WriteFile failed: pipe={}, gle={}, bytes_written={}, expected_bytes={}", pipeName,
                  GetLastError(), bytesWritten, expectedBytes);
}

void LogReadFailure(const wchar_t *pipeName, DWORD bytesRead)
{
    FANY_IPC_LOGF(L"[msime]: [ipc] ReadFile failed or empty: pipe={}, gle={}, bytes_read={}", pipeName,
                  GetLastError(), bytesRead);
}

void LogPeekFailure(const wchar_t *pipeName)
{
    FANY_IPC_LOGF(L"[msime]: [ipc] PeekNamedPipe failed: pipe={}, gle={}", pipeName, GetLastError());
}

void LogAuxMessage(const std::wstring &message)
{
    FANY_IPC_LOGF(L"[msime]: [ipc] SendToAuxNamedpipe: {}", message);
}

void ClosePipeHandleIfValid(HANDLE &hPipeHandle)
{
    if (IsValidPipeHandle(hPipeHandle))
    {
        CloseHandle(hPipeHandle);
    }
    hPipeHandle = nullptr;
}

bool WritePipeHello(HANDLE hPipeHandle, UINT pipeRole)
{
    DWORD bytesWritten = 0;
    if (pipeRole == FanyImePipeRole::Main)
    {
        FanyImeNamedpipeData hello = {};
        hello.event_type = FanyImePipeEventType::ClientHello;
        hello.client_id = GetPipeClientId();
        BOOL ret = WriteFile(hPipeHandle, &hello, sizeof(hello), &bytesWritten, NULL);
        return ret && bytesWritten == sizeof(hello);
    }

    FanyImePipeHello hello = {};
    hello.client_id = GetPipeClientId();
    hello.pipe_role = pipeRole;
    BOOL ret = WriteFile(hPipeHandle, &hello, sizeof(hello), &bytesWritten, NULL);
    return ret && bytesWritten == sizeof(hello);
}

bool TryOpenClientPipe(HANDLE &hPipeHandle, const wchar_t *pipeName, UINT pipeRole)
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
        LogCreateFileFailure(pipeName);
        return false;
    }

    hPipeHandle = openedPipe;
    LogCreateFileSuccess(pipeName);
    if (!WritePipeHello(hPipeHandle, pipeRole))
    {
        LogWriteFailure(pipeName, 0, pipeRole == FanyImePipeRole::Main ? sizeof(FanyImeNamedpipeData)
                                                                       : sizeof(FanyImePipeHello));
        ClosePipeHandleIfValid(hPipeHandle);
        return false;
    }
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
    bool mainPipeReady = TryOpenClientPipe(hPipe, FANY_IME_NAMED_PIPE, FanyImePipeRole::Main);
    bool toTsfPipeReady = TryOpenClientPipe(hFromServerPipe, FANY_IME_TO_TSF_NAMED_PIPE, FanyImePipeRole::ToTsf);
    bool toTsfWorkerPipeReady =
        TryOpenClientPipe(hToTsfWorkerThreadPipe, FANY_IME_TO_TSF_WORKER_THREAD_NAMED_PIPE,
                          FanyImePipeRole::ToTsfWorkerThread);

    return (mainPipeReady && toTsfPipeReady && toTsfWorkerPipeReady) ? 1 : 0;
}

int ConnectToTsfNamedpipe()
{
    return TryOpenClientPipe(hFromServerPipe, FANY_IME_TO_TSF_NAMED_PIPE, FanyImePipeRole::ToTsf) ? 1 : 0;
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
    ClosePipeHandleIfValid(hToTsfWorkerThreadPipe);
    return 0;
}

HANDLE GetToTsfWorkerThreadNamedpipe()
{
    return hToTsfWorkerThreadPipe;
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
    namedpipeData.client_id = GetPipeClientId();

    if (!TryOpenClientPipe(hPipe, FANY_IME_NAMED_PIPE, FanyImePipeRole::Main))
    {
        return;
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
        LogWriteFailure(FANY_IME_NAMED_PIPE, bytesWritten, sizeof(namedpipeData));
        ClosePipeHandleIfValid(hPipe);
#ifdef FANY_DEBUG
        OutputDebugString(fmt::format(L"[msime]: SendToNamedpipe failed eventually01.").c_str());
#endif
        for (int i = 0; i < 10; ++i)
        {
            Sleep(10);
            if (TryOpenClientPipe(hPipe, FANY_IME_NAMED_PIPE, FanyImePipeRole::Main))
            {
                break;
            }
        }

        if (hPipe == INVALID_HANDLE_VALUE)
        {
            LogCreateFileFailure(FANY_IME_NAMED_PIPE);
#ifdef FANY_DEBUG
            OutputDebugString(fmt::format(L"[msime]: SendToNamedpipe failed eventually02.").c_str());
#endif
            return;
        }
        else
        {
            LogCreateFileSuccess(FANY_IME_NAMED_PIPE);
        }

        namedpipeData.client_id = GetPipeClientId();
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
            LogWriteFailure(FANY_IME_NAMED_PIPE, bytesWritten, sizeof(namedpipeData));
#ifdef FANY_DEBUG
            OutputDebugString(fmt::format(L"[msime]: SendToNamedpipe failed eventually03.").c_str());
#endif
        }

        return;
    }
}

/**
 * @brief Clear namedpipe data if exists, cause sometimes there may be some useless data sent by last key event from
 * server
 *
 */
void ClearNamedpipeDataIfExists(bool force)
{
    bool isSpaceOrNumber = Global::Keycode == VK_SPACE ||                      //
                           (Global::Keycode > '0' && Global::Keycode < '9') || //
                           (Global::CommitWithFirstCandPunc.count(Global::wch) > 0);
    /* Only clear namedpipe data if keycode is space or number, for better performance */
    if (!force && !isSpaceOrNumber)
    {
        return;
    }

    if (!hFromServerPipe || hFromServerPipe == INVALID_HANDLE_VALUE) // Try to reconnect
    {
        if (!TryOpenClientPipe(hFromServerPipe, FANY_IME_TO_TSF_NAMED_PIPE, FanyImePipeRole::ToTsf))
        {
            return;
        }
    }

    DWORD bytesAvailable = 0;
    DWORD bytesRead = 0;
    int clearedCount = 0;
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
            if (!peekOk)
            {
                LogPeekFailure(FANY_IME_TO_TSF_NAMED_PIPE);
            }
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
            LogReadFailure(FANY_IME_TO_TSF_NAMED_PIPE, bytesRead);
            break;
        }
        clearedCount++;
    }

    if (force)
    {
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
    int timeoutMs = 50; // Default timeout 50ms

    if (!hFromServerPipe || hFromServerPipe == INVALID_HANDLE_VALUE) // Try to reconnect
    {
        if (!TryOpenClientPipe(hFromServerPipe, FANY_IME_TO_TSF_NAMED_PIPE, FanyImePipeRole::ToTsf))
        {
            namedpipeDataFromServer.msg_type = Global::DataFromServerMsgType::Normal;
            // wcscpy_s(namedpipeDataFromServer.candidate_string, L"PipeOpenError");
            wcscpy_s(namedpipeDataFromServer.candidate_string, L"X");
            return &namedpipeDataFromServer;
        }
    }

    DWORD bytesAvailable = 0;
    LARGE_INTEGER frequency = {};
    LARGE_INTEGER startCounter = {};
    LARGE_INTEGER nowCounter = {};
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&startCounter);

    while (true)
    {
// TODO: Do not log
#ifdef FANY_DEBUG
        QueryPerformanceCounter(&nowCounter);
        OutputDebugString(
            fmt::format(L"[msime]: current waited: {:.3f}", GetElapsedMilliseconds(startCounter, nowCounter, frequency))
                .c_str());
#endif
        if (PeekNamedPipe(hFromServerPipe, nullptr, 0, nullptr, &bytesAvailable, nullptr) && bytesAvailable > 0)
        {
            auto ret = ReadDataFromServerViaNamedPipe();
#ifdef FANY_DEBUG
            QueryPerformanceCounter(&nowCounter);
            OutputDebugString(
                fmt::format(L"[msime]: PeekNamedPipe: {:.3f}", GetElapsedMilliseconds(startCounter, nowCounter, frequency))
                    .c_str());
#endif
            return ret;
        }

        QueryPerformanceCounter(&nowCounter);
        if (GetElapsedMilliseconds(startCounter, nowCounter, frequency) >= timeoutMs)
        {
            break;
        }

        // Avoid Sleep(1), which often expands to a full scheduler tick (~15.6ms)
        // in explorer.exe and makes space-commit latency visibly worse.
        if (!SwitchToThread())
        {
            YieldProcessor();
        }
    }

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
        if (!TryOpenClientPipe(hFromServerPipe, FANY_IME_TO_TSF_NAMED_PIPE, FanyImePipeRole::ToTsf))
        {
            namedpipeDataFromServer.msg_type = 0;
            // wcscpy_s(namedpipeDataFromServer.candidate_string, L"PipeOpenError");
            wcscpy_s(namedpipeDataFromServer.candidate_string, L"X");
            return &namedpipeDataFromServer;
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
        LogReadFailure(FANY_IME_TO_TSF_NAMED_PIPE, bytesRead);
        ClosePipeHandleIfValid(hFromServerPipe);
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
#ifdef FANY_DEBUG
        OutputDebugString(fmt::format(L"[msime]: SendToAuxNamedpipe: PipeOpenError: {}", pipeData).c_str());
#endif
        LogCreateFileFailure(FANY_IME_AUX_NAMED_PIPE);
        return;
    }
    LogAuxMessage(pipeData);
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
        LogWriteFailure(FANY_IME_AUX_NAMED_PIPE, bytesWritten, pipeData.length() * sizeof(wchar_t));
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
    namedpipeData.event_type = FanyImePipeEventType::KeyEvent;
    SendToNamedpipe();

    return 0;
}

int SendHideCandidateWndEventToUIProcessViaNamedPipe()
{
    namedpipeData.event_type = FanyImePipeEventType::HideCandidateWnd;
    SendToNamedpipe();

    return 0;
}

int SendShowCandidateWndEventToUIProcessViaNamedPipe()
{
    namedpipeData.event_type = FanyImePipeEventType::ShowCandidateWnd;
    SendToNamedpipe();

    return 0;
}

int SendMoveCandidateWndEventToUIProcessViaNamedPipe()
{
    namedpipeData.event_type = FanyImePipeEventType::MoveCandidateWnd;
    SendToNamedpipe();

    return 0;
}

int SendLangbarRightClickEventToUIProcessViaNamedPipe(const RECT *prcArea)
{
    namedpipeData.event_type = FanyImePipeEventType::LangbarRightClick;
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

int SendClientActivatedEventToServerViaNamedPipe()
{
    namedpipeData = {};
    namedpipeData.event_type = FanyImePipeEventType::ClientActivated;
    SendToNamedpipe();

    return 0;
}

int SendClientDeactivatedEventToServerViaNamedPipe()
{
    namedpipeData = {};
    namedpipeData.event_type = FanyImePipeEventType::ClientDeactivated;
    SendToNamedpipe();

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
    namedpipeData.event_type = FanyImePipeEventType::IMESwitch;
    /* 利用其他的字段，把 IME 的中英状态传递过去 */
    namedpipeData.keycode = uImeStatus;
    SendToNamedpipe();

    return 0;
}

int SendPuncSwitchEventToUIProcessViaNamedPipe(BOOL isPunc)
{
    namedpipeData.event_type = FanyImePipeEventType::PuncSwitch;
    /* 利用其他的字段，把标点符号的中英状态传递过去 */
    namedpipeData.keycode = isPunc;
    SendToNamedpipe();

    return 0;
}

int SendDoubleSingleByteSwitchEventToUIProcessViaNamedPipe(BOOL isDoubleSingleByte)
{
    namedpipeData.event_type = FanyImePipeEventType::DoubleSingleByteSwitch;
    /* 利用其他的字段，把全角/半角的状态传递过去 */
    namedpipeData.keycode = isDoubleSingleByte;
    SendToNamedpipe();

    return 0;
}
