#include "Private.h"
#include "Globals.h"
#include "MetasequoiaIME.h"
#include "CandidateListUIPresenter.h"
#include "CompositionProcessorEngine.h"
#include "Compartment.h"
#include "define.h"
#include <debugapi.h>
#include <namedpipeapi.h>
#include <winnt.h>
#include <winuser.h>
#include <Windows.h>
#include <shellscalingapi.h>
#include "FanyLog.h"
#include "Ipc.h"
#include "CommonUtils.h"
#include "Global/FanyDefines.h"
#include "Utils/FanyUtils.h"

#pragma comment(lib, "Shcore.lib")

namespace
{
constexpr UINT_PTR TIMER_CONNECT_ALL_NAMEDPIPE = 1;
constexpr UINT_PTR TIMER_CONNECT_TO_TSF_NAMEDPIPE = 2;
constexpr UINT CONNECT_NAMEDPIPE_RETRY_INTERVAL_MS = 50;
constexpr int CONNECT_ALL_NAMEDPIPE_MAX_RETRY = 3;
constexpr int CONNECT_TO_TSF_NAMEDPIPE_MAX_RETRY = 1;
int g_connectAllNamedpipeRetryCount = 0;
int g_connectToTsfNamedpipeRetryCount = 0;

} // namespace

//+---------------------------------------------------------------------------
//
// CreateInstance
//
//----------------------------------------------------------------------------

/* static */
HRESULT CMetasequoiaIME::CreateInstance(_In_ IUnknown *pUnkOuter, REFIID riid, _Outptr_ void **ppvObj)
{
    CMetasequoiaIME *pMetasequoiaIME = nullptr;
    HRESULT hr = S_OK;

    if (ppvObj == nullptr)
    {
        return E_INVALIDARG;
    }

    *ppvObj = nullptr;

    if (nullptr != pUnkOuter)
    {
        return CLASS_E_NOAGGREGATION;
    }

    pMetasequoiaIME = new (std::nothrow) CMetasequoiaIME();
    if (pMetasequoiaIME == nullptr)
    {
        return E_OUTOFMEMORY;
    }

    hr = pMetasequoiaIME->QueryInterface(riid, ppvObj);

    pMetasequoiaIME->Release();

    return hr;
}

//+---------------------------------------------------------------------------
//
// ctor
//
//----------------------------------------------------------------------------

CMetasequoiaIME::CMetasequoiaIME()
{
    DllAddRef();

    _pThreadMgr = nullptr;

    _threadMgrEventSinkCookie = TF_INVALID_COOKIE;

    _pTextEditSinkContext = nullptr;
    _textEditSinkCookie = TF_INVALID_COOKIE;

    _activeLanguageProfileNotifySinkCookie = TF_INVALID_COOKIE;

    _dwThreadFocusSinkCookie = TF_INVALID_COOKIE;

    _pComposition = nullptr;

    _pCompositionProcessorEngine = nullptr;

    _candidateMode = CANDIDATE_NONE;
    _pCandidateListUIPresenter = nullptr;
    _isCandidateWithWildcard = FALSE;

    _pDocMgrLastFocused = nullptr;

    _pSIPIMEOnOffCompartment = nullptr;
    _dwSIPIMEOnOffCompartmentSinkCookie = 0;
    _msgWndHandle = nullptr;

    _pContext = nullptr;

    _refCount = 1;

    _msgWndHandle = nullptr;
    _pIpcThread = nullptr;
    _shouldStopIpcThread = false;
}

//+---------------------------------------------------------------------------
//
// dtor
//
//----------------------------------------------------------------------------

CMetasequoiaIME::~CMetasequoiaIME()
{
    if (_pCandidateListUIPresenter)
    {
        delete _pCandidateListUIPresenter;
        _pCandidateListUIPresenter = nullptr;
    }
    DllRelease();

    /* 处理线程的清理 */
    if (_pIpcThread)
    {
        if (_pIpcThread->joinable())
        {
            _pIpcThread->join();
        }
        delete _pIpcThread;
        _pIpcThread = nullptr;
    }
}

//+---------------------------------------------------------------------------
//
// QueryInterface
//
//----------------------------------------------------------------------------

STDAPI CMetasequoiaIME::QueryInterface(REFIID riid, _Outptr_ void **ppvObj)
{
    if (ppvObj == nullptr)
    {
        return E_INVALIDARG;
    }

    *ppvObj = nullptr;

    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfTextInputProcessor))
    {
        *ppvObj = (ITfTextInputProcessor *)this;
    }
    else if (IsEqualIID(riid, IID_ITfTextInputProcessorEx))
    {
        *ppvObj = (ITfTextInputProcessorEx *)this;
    }
    else if (IsEqualIID(riid, IID_ITfThreadMgrEventSink))
    {
        *ppvObj = (ITfThreadMgrEventSink *)this;
    }
    else if (IsEqualIID(riid, IID_ITfTextEditSink))
    {
        *ppvObj = (ITfTextEditSink *)this;
    }
    else if (IsEqualIID(riid, IID_ITfKeyEventSink))
    {
        *ppvObj = (ITfKeyEventSink *)this;
    }
    else if (IsEqualIID(riid, IID_ITfActiveLanguageProfileNotifySink))
    {
        *ppvObj = (ITfActiveLanguageProfileNotifySink *)this;
    }
    else if (IsEqualIID(riid, IID_ITfCompositionSink))
    {
        *ppvObj = (ITfCompositionSink *)this;
    }
    else if (IsEqualIID(riid, IID_ITfDisplayAttributeProvider))
    {
        *ppvObj = (ITfDisplayAttributeProvider *)this;
    }
    else if (IsEqualIID(riid, IID_ITfThreadFocusSink))
    {
        *ppvObj = (ITfThreadFocusSink *)this;
    }
    else if (IsEqualIID(riid, IID_ITfFunctionProvider))
    {
        *ppvObj = (ITfFunctionProvider *)this;
    }
    else if (IsEqualIID(riid, IID_ITfFunction))
    {
        *ppvObj = (ITfFunction *)this;
    }
    else if (IsEqualIID(riid, IID_ITfFnGetPreferredTouchKeyboardLayout))
    {
        *ppvObj = (ITfFnGetPreferredTouchKeyboardLayout *)this;
    }

    if (*ppvObj)
    {
        AddRef();
        return S_OK;
    }

    return E_NOINTERFACE;
}

//+---------------------------------------------------------------------------
//
// AddRef
//
//----------------------------------------------------------------------------

STDAPI_(ULONG) CMetasequoiaIME::AddRef()
{
    return ++_refCount;
}

//+---------------------------------------------------------------------------
//
// Release
//
//----------------------------------------------------------------------------

STDAPI_(ULONG) CMetasequoiaIME::Release()
{
    LONG cr = --_refCount;

    assert(_refCount >= 0);

    if (_refCount == 0)
    {
        delete this;
    }

    return cr;
}

//+---------------------------------------------------------------------------
//
// ITfTextInputProcessorEx::ActivateEx
//
//----------------------------------------------------------------------------

STDAPI CMetasequoiaIME::ActivateEx(ITfThreadMgr *pThreadMgr, TfClientId tfClientId, DWORD dwFlags)
{
    _pThreadMgr = pThreadMgr;
    _pThreadMgr->AddRef();

    _tfClientId = tfClientId;
    _dwActivateFlags = dwFlags;

#ifdef FANY_DEBUG
    OutputDebugString(L"[msime]: CMetasequoiaIME::ActivateEx");
#endif
    /*
    std::wstring processName = FanyUtils::GetCurrentProcessName();
    if (Global::VSCodeSeries.find(processName) != Global::VSCodeSeries.end())
    {
        Global::IsVSCodeLike = true;
    }
    */
    // Set up IPC(named pipe)
    // InitIpc();
    // TODO: 去掉共享内存，只保留命名管道
    // InitNamedpipe();

    Global::current_process_name = GetCurrentProcessName();

    if (!_InitThreadMgrEventSink())
    {
        goto ExitError;
    }

    // Register generic window class for message-only window
    {
        WNDCLASSEX wcex = {};
        wcex.cbSize = sizeof(WNDCLASSEX);
        wcex.lpfnWndProc = CMetasequoiaIME_WindowProc;
        wcex.hInstance = Global::dllInstanceHandle;
        wcex.lpszClassName = L"MetasequoiaIMEWorkerWnd";
        wcex.cbWndExtra = sizeof(LONG_PTR);
        RegisterClassEx(&wcex);
    }

    _msgWndHandle = CreateWindowEx( //
        0,                          //
        L"MetasequoiaIMEWorkerWnd", //
        L"MetasequoiaIMEWorkerWnd", //
        0, 0, 0, 0, 0,              //
        HWND_MESSAGE,               //
        nullptr,                    //
        Global::dllInstanceHandle,  //
        this);
    SetWindowLongPtr(_msgWndHandle, GWLP_USERDATA, (LONG_PTR)this);
    Global::msgWndHandle = _msgWndHandle;

    /* 连接命名管道 */
    Global::g_connected = true; // 激活时也需要设置一下
    PostMessage(_msgWndHandle, WM_ConnectNamedpipe, 0, 0);

    /* 创建 IPC 线程 */
    _shouldStopIpcThread = false;
    _pIpcThread = new std::thread(IpcWorkerThread, this);

    ITfDocumentMgr *pDocMgrFocus = nullptr;
    if (SUCCEEDED(_pThreadMgr->GetFocus(&pDocMgrFocus)) && (pDocMgrFocus != nullptr))
    {
        _InitTextEditSink(pDocMgrFocus);
        pDocMgrFocus->Release();
    }

    if (!_InitKeyEventSink())
    {
        goto ExitError;
    }

    if (!_InitActiveLanguageProfileNotifySink())
    {
        goto ExitError;
    }

    if (!_InitThreadFocusSink())
    {
        goto ExitError;
    }

    if (!_InitDisplayAttributeGuidAtom())
    {
        goto ExitError;
    }

    if (!_InitFunctionProviderSink())
    {
        goto ExitError;
    }

    if (!_AddTextProcessorEngine())
    {
        goto ExitError;
    }

    // Reset to Chinese mode whenever switch back to this IME
    _pCompositionProcessorEngine->InitializeMetasequoiaIMECompartment(pThreadMgr, tfClientId);

    // 激活此输入法时，向 server 端发送一个激活的消息
    PostMessage(_msgWndHandle, WM_IMEActivation, 0, 0);

    {
        HWND hwndTarget = GetFocus();
        DPI_AWARENESS awareness = GetAwarenessFromDpiAwarenessContext(GetWindowDpiAwarenessContext(hwndTarget));

        if (awareness == DPI_AWARENESS_UNAWARE)
        {
#ifdef FANY_DEBUG
            OutputDebugString(fmt::format(L"[msime]: awareness == DPI_AWARENESS_UNAWARE").c_str());
#endif
            /* 宿主是非感知程序，需要反向缩放 */
            DPI_AWARENESS_CONTEXT oldCtx = SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE);
            HMONITOR hMon = MonitorFromWindow(hwndTarget, MONITOR_DEFAULTTONEAREST);
            UINT dpiX = 96, dpiY = 96;
            GetDpiForMonitor(hMon, MDT_EFFECTIVE_DPI, &dpiX, &dpiY);
            SetThreadDpiAwarenessContext(oldCtx);
            Global::DpiScale = dpiX / 96.0;
#ifdef FANY_DEBUG
            OutputDebugString(fmt::format(L"[msime]: Global::DpiScale: {}", Global::DpiScale).c_str());
#endif
        }
    }

#ifdef FANY_DEBUG
    OutputDebugString(L"[msime]: CMetasequoiaIME::ActivateEx");
#endif

    return S_OK;

ExitError:
    Deactivate();
    return E_FAIL;
}

//+---------------------------------------------------------------------------
//
// ITfTextInputProcessorEx::Deactivate
//
//----------------------------------------------------------------------------

STDAPI CMetasequoiaIME::Deactivate()
{
    // 注销此输入法时，向 server 端发送一个注销的消息
    // 注销不要给消息窗口发送消息，而是直接在这里处理
    // SendIMEDeactivationEventToUIProcessViaNamedPipe();

    ITfInputProcessorProfileMgr *pProfileMgr = nullptr;
    TF_INPUTPROCESSORPROFILE profile = {};
    HRESULT hrProfile = CoCreateInstance( //
        CLSID_TF_InputProcessorProfiles,  //
        nullptr,                          //
        CLSCTX_INPROC_SERVER,             //
        IID_ITfInputProcessorProfileMgr,  //
        (void **)&pProfileMgr);

    if (SUCCEEDED(hrProfile) && pProfileMgr != nullptr)
    {
        hrProfile = pProfileMgr->GetActiveProfile(GUID_TFCAT_TIP_KEYBOARD, &profile);
        if (SUCCEEDED(hrProfile) && !IsEqualCLSID(profile.clsid, Global::MetasequoiaIMECLSID))
        {
            // 向 server 端发送一个输入法真的切换到了别的输入法的消息
            // 只有接收到这个消息，才可以隐藏 ftb
            OutputDebugString(fmt::format(L"[msime]: Truely deactivate IME and switch to another.").c_str());
            SendIMEDeactivationEventToUIProcessViaNamedPipe();
        }

        pProfileMgr->Release();
    }

    /* 清理 IPC 线程 */
    _shouldStopIpcThread = true;
    // CancelIoEx(Global::hToTsfWorkerThreadPipe, nullptr);
    // CloseHandle(Global::hToTsfWorkerThreadPipe);
    // Global::hToTsfWorkerThreadPipe = nullptr;
    // Clean IPC
    /* 必须先 CloseIpc，再 join 线程 */
    CloseIpc();
    if (_pIpcThread && _pIpcThread->joinable())
    {
        _pIpcThread->join();
        delete _pIpcThread;
        _pIpcThread = nullptr;
    }
    // TODO: 去掉共享内存，只保留命名管道
    // CloseNamedpipe();

#ifdef FANY_DEBUG
    OutputDebugString(L"[msime]: CMetasequoiaIME::Deactivate");
#endif

    if (_pCompositionProcessorEngine)
    {
        delete _pCompositionProcessorEngine;
        _pCompositionProcessorEngine = nullptr;
    }

    ITfContext *pContext = _pContext;
    if (_pContext)
    {
        pContext->AddRef();
        _EndComposition(_pContext);
    }

    if (_pCandidateListUIPresenter)
    {
        delete _pCandidateListUIPresenter;
        _pCandidateListUIPresenter = nullptr;

        if (pContext)
        {
            pContext->Release();
        }

        _candidateMode = CANDIDATE_NONE;
        _isCandidateWithWildcard = FALSE;
    }

    _UninitFunctionProviderSink();

    _UninitThreadFocusSink();

    _UninitActiveLanguageProfileNotifySink();

    _UninitKeyEventSink();

    _UninitThreadMgrEventSink();

    CCompartment CompartmentKeyboardOpen(_pThreadMgr, _tfClientId, GUID_COMPARTMENT_KEYBOARD_OPENCLOSE);
    CompartmentKeyboardOpen._ClearCompartment();

    CCompartment CompartmentDoubleSingleByte(_pThreadMgr, _tfClientId,
                                             Global::MetasequoiaIMEGuidCompartmentDoubleSingleByte);
    CompartmentDoubleSingleByte._ClearCompartment();

    CCompartment CompartmentPunctuation(_pThreadMgr, _tfClientId, Global::MetasequoiaIMEGuidCompartmentPunctuation);
    CompartmentDoubleSingleByte._ClearCompartment();

    if (_pThreadMgr != nullptr)
    {
        _pThreadMgr->Release();
    }

    _tfClientId = TF_CLIENTID_NULL;

    if (_pDocMgrLastFocused)
    {
        _pDocMgrLastFocused->Release();
        _pDocMgrLastFocused = nullptr;
    }

    /* 清理消息窗口 */
    if (_msgWndHandle)
    {
        KillTimer(_msgWndHandle, TIMER_CONNECT_ALL_NAMEDPIPE);
        KillTimer(_msgWndHandle, TIMER_CONNECT_TO_TSF_NAMEDPIPE);
        g_connectAllNamedpipeRetryCount = 0;
        g_connectToTsfNamedpipeRetryCount = 0;
        DestroyWindow(_msgWndHandle);
        _msgWndHandle = nullptr;
    }
    UnregisterClass(L"MetasequoiaIMEWorkerWnd", Global::dllInstanceHandle);

    return S_OK;
}

//+---------------------------------------------------------------------------
//
// IpcWorkerThread
// 从 Server 端接收消息
//
//----------------------------------------------------------------------------
void CMetasequoiaIME::IpcWorkerThread(CMetasequoiaIME *pIME)
{
    while (!pIME->_shouldStopIpcThread)
    {
        /* 阻塞读 */
        while (!pIME->_shouldStopIpcThread)
        {
            DWORD bytesRead = 0;
            FanyImeNamedpipeDataToTsfWorkerThread buf;
            BOOL readResult = ReadFile(         //
                Global::hToTsfWorkerThreadPipe, //
                &buf,                           //
                sizeof(buf),                    //
                &bytesRead,                     //
                nullptr                         //
            );

            if (!readResult || bytesRead == 0)
            {
                break;
            }

            if (buf.msg_type == Global::DataToTsfWorkerThreadMsgType::CommitCurCandidate)
            {
                PostMessage(pIME->_msgWndHandle, WM_CommitCandidate, 0, 0);
            }
            else if (buf.msg_type == Global::DataToTsfWorkerThreadMsgType::SwitchToEnglish)
            {
                PostMessage(                                               //
                    pIME->_msgWndHandle,                                   //
                    WM_CheckGlobalCompartment,                             //
                    Global::DataToTsfWorkerThreadMsgType::SwitchToEnglish, //
                    0);
                OutputDebugString(fmt::format(L"[msime]: Switch to English").c_str());
            }
            else if (buf.msg_type == Global::DataToTsfWorkerThreadMsgType::SwitchToChinese)
            {
                PostMessage(                                               //
                    pIME->_msgWndHandle,                                   //
                    WM_CheckGlobalCompartment,                             //
                    Global::DataToTsfWorkerThreadMsgType::SwitchToChinese, //
                    0);
                OutputDebugString(fmt::format(L"[msime]: Switch to Chinese").c_str());
            }
            else if (buf.msg_type == Global::DataToTsfWorkerThreadMsgType::SwitchToPuncEn)
            {
                PostMessage(                                              //
                    pIME->_msgWndHandle,                                  //
                    WM_CheckGlobalCompartment,                            //
                    Global::DataToTsfWorkerThreadMsgType::SwitchToPuncEn, //
                    0);
                OutputDebugString(fmt::format(L"[msime]: Switch to Punc En").c_str());
            }
            else if (buf.msg_type == Global::DataToTsfWorkerThreadMsgType::SwitchToPuncCn)
            {
                PostMessage(                                              //
                    pIME->_msgWndHandle,                                  //
                    WM_CheckGlobalCompartment,                            //
                    Global::DataToTsfWorkerThreadMsgType::SwitchToPuncCn, //
                    0);
                OutputDebugString(fmt::format(L"[msime]: Switch to Punc Cn").c_str());
            }
            else if (buf.msg_type == Global::DataToTsfWorkerThreadMsgType::SwitchToFullwidth)
            {
                PostMessage(                                                 //
                    pIME->_msgWndHandle,                                     //
                    WM_CheckGlobalCompartment,                               //
                    Global::DataToTsfWorkerThreadMsgType::SwitchToFullwidth, //
                    0);
                OutputDebugString(fmt::format(L"[msime]: Switch to Fullwidth").c_str());
            }
            else if (buf.msg_type == Global::DataToTsfWorkerThreadMsgType::SwitchToHalfwidth)
            {
                PostMessage(                                                 //
                    pIME->_msgWndHandle,                                     //
                    WM_CheckGlobalCompartment,                               //
                    Global::DataToTsfWorkerThreadMsgType::SwitchToHalfwidth, //
                    0);
                OutputDebugString(fmt::format(L"[msime]: Switch to Halfwidth").c_str());
            }
        }
        Sleep(100);
    }
}

//+---------------------------------------------------------------------------
//
// CMetasequoiaIME_WindowProc
//
//----------------------------------------------------------------------------
LRESULT CALLBACK CMetasequoiaIME_WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    CMetasequoiaIME *pIME = (CMetasequoiaIME *)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    if (!pIME)
    {
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    switch (message)
    {
    case WM_CheckGlobalCompartment: {
        if (wParam == Global::DataToTsfWorkerThreadMsgType::SwitchToEnglish)
        {
            OutputDebugString(fmt::format(L"[msime]: Wnd Switch to English").c_str());
            pIME->GetCompositionProcessorEngine()->SetIMEMode(pIME->_GetThreadMgr(), pIME->_GetClientId(), FALSE);
        }
        else if (wParam == Global::DataToTsfWorkerThreadMsgType::SwitchToChinese)
        {
            OutputDebugString(fmt::format(L"[msime]: Wnd Switch to Chinese").c_str());
            pIME->GetCompositionProcessorEngine()->SetIMEMode(pIME->_GetThreadMgr(), pIME->_GetClientId(), TRUE);
            OutputDebugString(fmt::format(L"[msime]: Switch to Chinese").c_str());
        }
        else if (wParam == Global::DataToTsfWorkerThreadMsgType::SwitchToPuncEn)
        {
            OutputDebugString(fmt::format(L"[msime]: Wnd Switch to Punc En").c_str());
            pIME->GetCompositionProcessorEngine()->SetPunctuationMode(pIME->_GetThreadMgr(), pIME->_GetClientId(),
                                                                      FALSE);
        }
        else if (wParam == Global::DataToTsfWorkerThreadMsgType::SwitchToPuncCn)
        {
            OutputDebugString(fmt::format(L"[msime]: Wnd Switch to Punc Cn").c_str());
            pIME->GetCompositionProcessorEngine()->SetPunctuationMode(pIME->_GetThreadMgr(), pIME->_GetClientId(),
                                                                      TRUE);
        }
        else if (wParam == Global::DataToTsfWorkerThreadMsgType::SwitchToFullwidth)
        {
            OutputDebugString(fmt::format(L"[msime]: Wnd Switch to Fullwidth").c_str());
            pIME->GetCompositionProcessorEngine()->SetDoubleSingleByteMode(pIME->_GetThreadMgr(), pIME->_GetClientId(),
                                                                           TRUE);
        }
        else if (wParam == Global::DataToTsfWorkerThreadMsgType::SwitchToHalfwidth)
        {
            OutputDebugString(fmt::format(L"[msime]: Wnd Switch to Halfwidth").c_str());
            pIME->GetCompositionProcessorEngine()->SetDoubleSingleByteMode(pIME->_GetThreadMgr(), pIME->_GetClientId(),
                                                                           FALSE);
        }
        break;
    }
    case WM_ConnectNamedpipe: {
        OutputDebugString(fmt::format(L"[msime]: Try to connect named pipe via WM_ConnectNamedpipe").c_str());
        KillTimer(hWnd, TIMER_CONNECT_ALL_NAMEDPIPE);
        g_connectAllNamedpipeRetryCount = 0;
        SendToAuxNamedpipe(L"kill");
        // 即使是第一次连接，也要等一会儿再试，要给 server 端留时间来处理 kill
        SetTimer(hWnd, TIMER_CONNECT_ALL_NAMEDPIPE, CONNECT_NAMEDPIPE_RETRY_INTERVAL_MS, nullptr);
        break;
    }
    case WM_DisconnectNamedpipe: {
        OutputDebugString(fmt::format(L"[msime]: WM_DisconnectNamedpipe.").c_str());
        KillTimer(hWnd, TIMER_CONNECT_ALL_NAMEDPIPE);
        KillTimer(hWnd, TIMER_CONNECT_TO_TSF_NAMEDPIPE);
        g_connectAllNamedpipeRetryCount = 0;
        g_connectToTsfNamedpipeRetryCount = 0;
        CloseNamedpipe();
        break;
    }
    case WM_ConnectToTsfNamedpipe: {
        OutputDebugString(fmt::format(L"[msime]: Try to Connect to TSF named pipe").c_str());
        KillTimer(hWnd, TIMER_CONNECT_TO_TSF_NAMEDPIPE);
        g_connectToTsfNamedpipeRetryCount = 0;
        // Keep old behavior: wait once before first connect try, but do it asynchronously.
        SetTimer(hWnd, TIMER_CONNECT_TO_TSF_NAMEDPIPE, CONNECT_NAMEDPIPE_RETRY_INTERVAL_MS, nullptr);
        break;
    }
    case WM_TIMER: {
        if (wParam == TIMER_CONNECT_ALL_NAMEDPIPE)
        {
            // 如果用户已经切换走了，就不用继续重试
            if (!Global::g_connected)
            {
                KillTimer(hWnd, TIMER_CONNECT_ALL_NAMEDPIPE);
                g_connectAllNamedpipeRetryCount = 0;
                break;
            }

            OutputDebugString(
                fmt::format(L"[msime]: Retry connect all named pipes: {}", g_connectAllNamedpipeRetryCount).c_str());
            if (ConnectToAllNamedpipe())
            {
                KillTimer(hWnd, TIMER_CONNECT_ALL_NAMEDPIPE);
                g_connectAllNamedpipeRetryCount = 0;
                OutputDebugString(fmt::format(L"[msime]: Yes Connected to named pipe :)").c_str());
                break;
            }

            g_connectAllNamedpipeRetryCount++;
            if (g_connectAllNamedpipeRetryCount >= CONNECT_ALL_NAMEDPIPE_MAX_RETRY)
            {
                KillTimer(hWnd, TIMER_CONNECT_ALL_NAMEDPIPE);
                g_connectAllNamedpipeRetryCount = 0;
            }
            break;
        }

        if (wParam == TIMER_CONNECT_TO_TSF_NAMEDPIPE)
        {
            OutputDebugString(
                fmt::format(L"[msime]: Retry connect to tsf named pipe: {}", g_connectToTsfNamedpipeRetryCount)
                    .c_str());
            if (ConnectToTsfNamedpipe())
            {
                KillTimer(hWnd, TIMER_CONNECT_TO_TSF_NAMEDPIPE);
                g_connectToTsfNamedpipeRetryCount = 0;
                break;
            }

            g_connectToTsfNamedpipeRetryCount++;
            if (g_connectToTsfNamedpipeRetryCount >= CONNECT_TO_TSF_NAMEDPIPE_MAX_RETRY)
            {
                KillTimer(hWnd, TIMER_CONNECT_TO_TSF_NAMEDPIPE);
                g_connectToTsfNamedpipeRetryCount = 0;
            }
            break;
        }
        break;
    }
    case WM_IMEActivation: {
        SendIMEActivationEventToUIProcessViaNamedPipe();
        break;
    }
    case WM_ThreadFocus: {
        /*  */
        /* 通知 server 端更新一下 ftb 中英、全半角、标点的状态 */
        SendIMEStatusEventToUIProcessViaNamedPipe(                                                          //
            pIME->GetCompositionProcessorEngine()->GetIMEMode(pIME->_GetThreadMgr(), pIME->_GetClientId()), //
            pIME->GetCompositionProcessorEngine()->GetDoubleSingleByteMode(pIME->_GetThreadMgr(),
                                                                           pIME->_GetClientId()),                    //
            pIME->GetCompositionProcessorEngine()->GetPunctuationMode(pIME->_GetThreadMgr(), pIME->_GetClientId())); //
        break;
    }
    case WM_UpdateIMEStatus: {
        SendIMESwitchEventToUIProcessViaNamedPipe((UINT)wParam);
        break;
    }
    case WM_UpdateDoubleSingleByte: {
        SendDoubleSingleByteSwitchEventToUIProcessViaNamedPipe((BOOL)wParam);
        break;
    }
    case WM_UpdatePuncMode: {
        SendPuncSwitchEventToUIProcessViaNamedPipe((BOOL)wParam);
        break;
    }
    case WM_CommitCandidate: {
        ITfDocumentMgr *pDocMgrFocus = nullptr;
        ITfContext *pContext = nullptr;

        if (SUCCEEDED(pIME->_GetThreadMgr()->GetFocus(&pDocMgrFocus)) && pDocMgrFocus)
        {
            if (SUCCEEDED(pDocMgrFocus->GetTop(&pContext)) && pContext)
            {
                _KEYSTROKE_STATE KeystrokeState;
                KeystrokeState.Category = CATEGORY_CANDIDATE;
                KeystrokeState.Function = FUNCTION_FINALIZE_CANDIDATELIST;
                pIME->_InvokeKeyHandler(pContext, 0, 0, 0, KeystrokeState);
                pContext->Release();
            }
            pDocMgrFocus->Release();
        }
        break;
    }
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

//+---------------------------------------------------------------------------
//
// ITfFunctionProvider::GetType
//
//----------------------------------------------------------------------------
HRESULT CMetasequoiaIME::GetType(__RPC__out GUID *pguid)
{
    HRESULT hr = E_INVALIDARG;
    if (pguid)
    {
        *pguid = Global::MetasequoiaIMECLSID;
        hr = S_OK;
    }
    return hr;
}

//+---------------------------------------------------------------------------
//
// ITfFunctionProvider::::GetDescription
//
//----------------------------------------------------------------------------
HRESULT CMetasequoiaIME::GetDescription(__RPC__deref_out_opt BSTR *pbstrDesc)
{
    HRESULT hr = E_INVALIDARG;
    if (pbstrDesc != nullptr)
    {
        *pbstrDesc = nullptr;
        hr = E_NOTIMPL;
    }
    return hr;
}

//+---------------------------------------------------------------------------
//
// ITfFunctionProvider::::GetFunction
//
//----------------------------------------------------------------------------
HRESULT CMetasequoiaIME::GetFunction(__RPC__in REFGUID rguid, __RPC__in REFIID riid,
                                     __RPC__deref_out_opt IUnknown **ppunk)
{
    HRESULT hr = E_NOINTERFACE;

    if ((IsEqualGUID(rguid, GUID_NULL)) && (IsEqualGUID(riid, __uuidof(ITfFnSearchCandidateProvider))))
    {
        hr = _pITfFnSearchCandidateProvider->QueryInterface(riid, (void **)ppunk);
    }
    else if (IsEqualGUID(rguid, GUID_NULL))
    {
        hr = QueryInterface(riid, (void **)ppunk);
    }

    return hr;
}

//+---------------------------------------------------------------------------
//
// ITfFunction::GetDisplayName
//
//----------------------------------------------------------------------------
HRESULT CMetasequoiaIME::GetDisplayName(_Out_ BSTR *pbstrDisplayName)
{
    HRESULT hr = E_INVALIDARG;
    if (pbstrDisplayName != nullptr)
    {
        *pbstrDisplayName = nullptr;
        hr = E_NOTIMPL;
    }
    return hr;
}

//+---------------------------------------------------------------------------
//
// ITfFnGetPreferredTouchKeyboardLayout::GetLayout
// The tkblayout will be Optimized layout.
//----------------------------------------------------------------------------
HRESULT CMetasequoiaIME::GetLayout(_Out_ TKBLayoutType *ptkblayoutType, _Out_ WORD *pwPreferredLayoutId)
{
    HRESULT hr = E_INVALIDARG;
    if ((ptkblayoutType != nullptr) && (pwPreferredLayoutId != nullptr))
    {
        *ptkblayoutType = TKBLT_OPTIMIZED;
        *pwPreferredLayoutId = TKBL_OPT_SIMPLIFIED_CHINESE_PINYIN;
        hr = S_OK;
    }
    return hr;
}
