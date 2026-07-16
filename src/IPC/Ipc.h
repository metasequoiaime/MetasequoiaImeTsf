#pragma once
#include <Windows.h>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

inline const wchar_t *FANY_IME_SHARED_MEMORY = L"Local\\FanyImeSharedMemory";
inline const int BUFFER_SIZE = 4096;

inline const wchar_t *FANY_IME_NAMED_PIPE = L"\\\\.\\pipe\\FanyImeNamedPipe";
inline const wchar_t *FANY_IME_TO_TSF_NAMED_PIPE = L"\\\\.\\pipe\\FanyImeToTsfNamedPipe";
inline const wchar_t *FANY_IME_TO_TSF_WORKER_THREAD_NAMED_PIPE = L"\\\\.\\pipe\\FanyImeToTsfWorkerThreadNamedPipe";
inline const wchar_t *FANY_IME_AUX_NAMED_PIPE = L"\\\\.\\pipe\\FanyImeAuxNamedPipe";
inline constexpr uint64_t FANY_IME_UNSOLICITED_REQUEST_ID = 0;
inline constexpr uint64_t FANY_IME_NO_REQUEST_ID = UINT64_MAX;

inline const std::vector<std::wstring> FANY_IME_EVENT_ARRAY = {
    L"FanyImeKeyEvent",           // Event sent to UI process to notify time to update UI by new pinyin_string
    L"FanyHideCandidateWndEvent", // Event sent to UI process to notify time to hide candidate window
    L"FanyShowCandidateWndEvent", // Event sent to UI process to notify time to show candidate window
    L"FanyMoveCandidateWndEvent", // Event sent to UI process to notify time to move candidate window
};

//
// modifiers:
//   0: non
//   1: shift
//   2: control
//   3: alt
//   4: win
//   5: to be supplemented
//
struct FanyImeSharedMemoryData
{
    UINT keycode;
    WCHAR wch;
    UINT modifiers_down = 0;
    int point[2] = {100, 100};
    int pinyin_length = 0;
    wchar_t pinyin_string[128];
    wchar_t candidate_string[1024];
    wchar_t selected_candiate_string[128];
};

//
// For uwp/metro apps, here we do not need candidate_string and selected_candiate_string,
// just let server process to handle them
//
// event_type
//   0: FanyImeKeyEvent
//   1: FanyHideCandidateWndEvent
//   2: FanyShowCandidateWndEvent
//   3: FanyMoveCandidateWndEvent
//   4: FanyLangbarRightClickEvent
//
struct FanyImeNamedpipeData
{
    UINT event_type;
    uint64_t client_id = 0;
    uint64_t request_id = 0;
    UINT keycode;
    WCHAR wch;
    UINT modifiers_down = 0;
    int point[2] = {100, 100};
    int pinyin_length = 0;
    wchar_t pinyin_string[128];
};

namespace FanyImePipeEventType
{
constexpr UINT KeyEvent = 0;
constexpr UINT HideCandidateWnd = 1;
constexpr UINT ShowCandidateWnd = 2;
constexpr UINT MoveCandidateWnd = 3;
constexpr UINT LangbarRightClick = 4;
constexpr UINT ClientHello = 10;
constexpr UINT ClientActivated = 11;
constexpr UINT ClientDeactivated = 12; // terminal TIP switch; toolbar hidden
constexpr UINT StatusSnapshot = 13;
constexpr UINT ClientSuspended = 14; // temporary focus route reset; toolbar kept
constexpr UINT IMESwitch = 7;
constexpr UINT PuncSwitch = 8;
constexpr UINT DoubleSingleByteSwitch = 9;
} // namespace FanyImePipeEventType

namespace FanyImePipeRole
{
constexpr UINT Main = 0;
constexpr UINT ToTsf = 1;
constexpr UINT ToTsfWorkerThread = 2;
} // namespace FanyImePipeRole

struct FanyImePipeHello
{
    uint64_t client_id = 0;
    UINT pipe_role = 0;
};

//
// Data received from server end
//
// msg_type
//   0: success
//   1: candidate index out of range error
//
struct FanyImeNamedpipeDataToTsf
{
    UINT msg_type;
    uint64_t request_id = 0;
    wchar_t candidate_string[200]; // 200 chars at most
};

//
// Data sent to tsf worker thread
//
// msg_type
//   0: IME switch to EN
//   1: IME switch to CN
//
// data
//   Not used now.
//
//
struct FanyImeNamedpipeDataToTsfWorkerThread
{
    UINT msg_type;
    wchar_t data[200];
};

static_assert(sizeof(WCHAR) == 2, "The IPC ABI requires 16-bit WCHAR.");
static_assert(offsetof(FanyImeNamedpipeData, client_id) == 8);
static_assert(offsetof(FanyImeNamedpipeData, request_id) == 16);
static_assert(offsetof(FanyImeNamedpipeData, keycode) == 24);
static_assert(offsetof(FanyImeNamedpipeData, pinyin_string) == 48);
static_assert(sizeof(FanyImeNamedpipeData) == 304);
static_assert(offsetof(FanyImeNamedpipeDataToTsf, request_id) == 8);
static_assert(offsetof(FanyImeNamedpipeDataToTsf, candidate_string) == 16);
static_assert(sizeof(FanyImeNamedpipeDataToTsf) == 416);
static_assert(sizeof(FanyImePipeHello) == 16);
static_assert(sizeof(FanyImeNamedpipeDataToTsfWorkerThread) == 404);

enum class KeyEventSendResult
{
    Sent,
    DefinitelyNotSent,
    DeliveryAmbiguous,
};

int InitIpc();
int InitNamedpipe();
int ConnectToAllNamedpipe();
int ConnectToTsfNamedpipe();
int CloseIpc();
int CloseNamedpipe();
void ResetNamedpipeReplyState();
HANDLE GetToTsfWorkerThreadNamedpipe();
void BindNamedpipeFocusState(_In_ const void *owner,
                             _In_opt_ bool *focusResetPending, _In_opt_ bool *activationRequired,
                             _In_opt_ std::atomic<uint64_t> *expectedWorkerFocusToken,
                             _In_opt_ std::atomic<bool> *localSessionResetPending,
                             _In_opt_ std::atomic<UINT> *localSessionResetToken,
                             _In_opt_ std::atomic<bool> *workerCommitReady,
                             _In_opt_ std::atomic<uint64_t> *acknowledgedWorkerFocusToken,
                             _In_opt_ std::atomic<HANDLE> *workerPipeHandle,
                             _In_opt_ std::atomic<UINT> *workerPipeGeneration);
void UnbindNamedpipeFocusState(_In_ const void *owner);
bool IsNamedpipeFocusStateOwner(_In_ const void *owner);
UINT BeginNamedpipeLocalSessionReset();
void InvalidateNamedpipeWorkerGeneration();
void MarkNamedpipeFocusLost();
void RequireNamedpipeFocusActivation();
void MarkNamedpipeSessionDirty();
bool MarkNamedpipeSessionDirtyForOwner(_In_ const void *owner);
bool EnsureNamedpipeFocusSessionActivated();
bool FlushNamedpipeFocusSessionReset();
bool FlushNamedpipeImeDeactivation();

//
// For shared memory
//
int WriteDataToSharedMemory(           //
    UINT keycode,                      // VkCode
    WCHAR wch,                         // Unicode character converted from vkcode
    UINT modifiers_down,               //
    const int point[2],                //
    int pinyin_length,                 //
    const std::wstring &pinyin_string, //
    UINT write_flag                    //
);
KeyEventSendResult SendKeyEventToUIProcess(_Out_opt_ uint64_t *requestId = nullptr);
int SendHideCandidateWndEventToUIProcess();
int SendShowCandidateWndEventToUIProcess();
int SendMoveCandidateWndEventToUIProcess();
int SendLangbarRightClickEventToUIProcess(const RECT *prcArea);
int SendIMEActivationEventToUIProcessViaNamedPipe();
int SendIMEDeactivationEventToUIProcessViaNamedPipe();
int SendClientActivatedEventToServerViaNamedPipe(uint64_t focusToken);
int SendClientDeactivatedEventToServerViaNamedPipe();
int SendClientSuspendedEventToServerViaNamedPipe();
int SendIMEStatusSnapshotToUIProcessViaNamedPipe(bool kbdIsOpen, bool fullwidthIsOpen, bool puncIsOpen);
int SendIMEStatusEventToUIProcessViaNamedPipe(bool kbdIsOpen, bool fullwidthIsOpen, bool puncIsOpen);
int SendIMESwitchEventToUIProcessViaNamedPipe(UINT uImeStatus);
int SendPuncSwitchEventToUIProcessViaNamedPipe(BOOL isPunc);
int SendDoubleSingleByteSwitchEventToUIProcessViaNamedPipe(BOOL isDoubleSingleByte);

void SendToAuxNamedpipe(std::wstring pipeData);

//
// For named pipe
//
int WriteDataToNamedPipe(              //
    UINT keycode,                      //
    WCHAR wch,                         //
    UINT modifiers_down,               //
    const int point[2],                //
    int pinyin_length,                 //
    const std::wstring &pinyin_string, //
    UINT write_flag                    //
);
KeyEventSendResult SendKeyEventToUIProcessViaNamedPipe(_Out_opt_ uint64_t *requestId = nullptr);
int SendHideCandidateWndEventToUIProcessViaNamedPipe();
int SendShowCandidateWndEventToUIProcessViaNamedPipe();
int SendMoveCandidateWndEventToUIProcessViaNamedPipe();
int SendLangbarRightClickEventToUIProcessViaNamedPipe(const RECT *prcArea);
void ClearNamedpipeDataIfExists(bool force = false);
struct FanyImeNamedpipeDataToTsf *TryReadDataFromServerPipeWithTimeout(uint64_t expectedRequestId);
struct FanyImeNamedpipeDataToTsf *ReadDataFromServerViaNamedPipe(uint64_t expectedRequestId);

//
// Modifiers:
//     0b00000001: Shift
//     0b00000010: Control
//     0b00000100: Alt
// TODO: Make it able to denote explicit modifiers, e.g. LShift, RShift, we could use left keys
//
namespace Global
{
inline thread_local UINT Keycode = 0;
inline thread_local WCHAR wch = L'\0';
inline thread_local UINT ModifiersDown = 0;
inline thread_local int Point[2] = {100, 100};
inline thread_local int PinyinLength = 0;
inline thread_local std::wstring PinyinString = L"";

inline thread_local int firefox_like_cnt = 0; // Apps like firefox, e.g. firefox, zen...
inline thread_local std::wstring current_process_name = L"";

inline thread_local wchar_t app_name[512] = {0};

namespace DataFromServerMsgType
{
constexpr UINT Normal = 0;
constexpr UINT OutofRange = 1;
constexpr UINT NeedToCreateWord = 2;
constexpr UINT Preedit = 3;
constexpr UINT NavigationIgnored = 4;
constexpr UINT MoveSelectionPrevious = 5;
constexpr UINT MoveSelectionNext = 6;
constexpr UINT MovePagePrevious = 7;
constexpr UINT MovePageNext = 8;
// Reverse-pipe registration acknowledgement. This frame is consumed during
// the ToTsf pipe handshake and is never exposed as a key reply.
constexpr UINT PipeReady = 9;
// Local-only result. It is never sent over the pipe and must never be
// interpreted as candidate text to commit.
constexpr UINT TransportUnavailable = static_cast<UINT>(-1);
} // namespace DataFromServerMsgType

namespace DataToTsfWorkerThreadMsgType
{
constexpr UINT SwitchToEnglish = 0;
constexpr UINT SwitchToChinese = 1;
constexpr UINT SwitchToPuncEn = 2;
constexpr UINT SwitchToPuncCn = 3;
constexpr UINT SwitchToFullwidth = 4;
constexpr UINT SwitchToHalfwidth = 5;
constexpr UINT CommitCurCandidate = 6;
constexpr UINT PagingCommaPeriodChanged = 7;
constexpr UINT FocusSessionReady = 8;
// Worker-endpoint registration acknowledgement. It is consumed before the
// handle is published to IpcWorkerThread and before Main can be opened.
constexpr UINT PipeReady = 9;
} // namespace DataToTsfWorkerThreadMsgType

inline std::atomic_bool PagingCommaPeriodEnabled{false};
inline thread_local bool g_connected = false;

} // namespace Global
