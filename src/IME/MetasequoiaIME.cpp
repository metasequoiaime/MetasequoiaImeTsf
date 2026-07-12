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
#include "../Utils/PerfTimer.h"


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

class CPunctuationCommitEditSession : public CEditSessionBase
{
  public:
    CPunctuationCommitEditSession(CMetasequoiaIME *pTextService, ITfContext *pContext, UINT code, WCHAR wch,
                                  LARGE_INTEGER requestStartQpc)
        : CEditSessionBase(pTextService, pContext), _code(code), _wch(wch), _requestStartQpc(requestStartQpc)
    {
        if (_requestStartQpc.QuadPart == 0)
        {
            QueryPerformanceCounter(&_requestStartQpc);
        }
    }

    STDMETHODIMP DoEditSession(TfEditCookie ec) override
    {
        PerfTimer timer;
        HRESULT hr = _pTextService->_HandleCompositionPunctuation(ec, _pContext, _wch);
        LARGE_INTEGER freq = {};
        LARGE_INTEGER nowQpc = {};
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&nowQpc);
        const double queueElapsedMs =
            static_cast<double>(nowQpc.QuadPart - _requestStartQpc.QuadPart) * 1000.0 / static_cast<double>(freq.QuadPart);
        return hr;
    }

  private:
    UINT _code;
    WCHAR _wch;
    LARGE_INTEGER _requestStartQpc;
};

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
    _pITfFnSearchCandidateProvider = nullptr;

    _msgWndHandle = nullptr;
    _pIpcThread = nullptr;
    _hToTsfWorkerThreadPipe = nullptr;
    _shouldStopIpcThread = false;
    _hasPendingServerCandidate = false;
    _pendingServerCandidateMsgType = Global::DataFromServerMsgType::OutofRange;
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
    _DrainPendingCandidatePresenterCleanup();
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

void CMetasequoiaIME::_QueuePendingCommitCandidate(_In_z_ const WCHAR *pCommitString)
{
    std::lock_guard<std::mutex> lock(_pendingCommitCandidateMutex);
    _pendingCommitCandidate = pCommitString ? pCommitString : L"";
}

std::wstring CMetasequoiaIME::_TakePendingCommitCandidate()
{
    std::lock_guard<std::mutex> lock(_pendingCommitCandidateMutex);
    std::wstring pendingCommitCandidate;
    pendingCommitCandidate.swap(_pendingCommitCandidate);
    return pendingCommitCandidate;
}

void CMetasequoiaIME::_QueuePendingPunctuationCommitText(_In_z_ const WCHAR *pCommitString)
{
    std::lock_guard<std::mutex> lock(_pendingCommitCandidateMutex);
    _pendingPunctuationCommitText = pCommitString ? pCommitString : L"";
}

std::wstring CMetasequoiaIME::_TakePendingPunctuationCommitText()
{
    std::lock_guard<std::mutex> lock(_pendingCommitCandidateMutex);
    std::wstring pendingPunctuationCommitText;
    pendingPunctuationCommitText.swap(_pendingPunctuationCommitText);
    return pendingPunctuationCommitText;
}

HRESULT CMetasequoiaIME::_RequestDirectPunctuationEditSession(_In_ ITfContext *pContext, UINT code, WCHAR wch)
{
    if (pContext == nullptr)
    {
        return E_INVALIDARG;
    }

    LARGE_INTEGER requestStartQpc = {};
    QueryPerformanceCounter(&requestStartQpc);

    CPunctuationCommitEditSession *pEditSession =
        new (std::nothrow) CPunctuationCommitEditSession(this, pContext, code, wch, requestStartQpc);
    if (pEditSession == nullptr)
    {
        return E_OUTOFMEMORY;
    }

    HRESULT editSessionHr = E_FAIL;
    PerfTimer requestTimer;
    HRESULT requestHr =
        pContext->RequestEditSession(_tfClientId, pEditSession, TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, &editSessionHr);
    pEditSession->Release();
    return requestHr;
}

void CMetasequoiaIME::_QueuePendingServerCandidate(UINT msgType, _In_z_ const WCHAR *pCandidateString)
{
    std::lock_guard<std::mutex> lock(_pendingCommitCandidateMutex);
    _hasPendingServerCandidate = true;
    _pendingServerCandidateMsgType = msgType;
    _pendingServerCandidateString = pCandidateString ? pCandidateString : L"";
}

bool CMetasequoiaIME::_TakePendingServerCandidate(_Out_ UINT *pMsgType, _Out_ std::wstring *pCandidateString)
{
    if (pMsgType == nullptr || pCandidateString == nullptr)
    {
        return false;
    }

    std::lock_guard<std::mutex> lock(_pendingCommitCandidateMutex);
    if (!_hasPendingServerCandidate)
    {
        return false;
    }

    *pMsgType = _pendingServerCandidateMsgType;
    pCandidateString->swap(_pendingServerCandidateString);
    _hasPendingServerCandidate = false;
    _pendingServerCandidateMsgType = Global::DataFromServerMsgType::OutofRange;
    return true;
}

void CMetasequoiaIME::_ScheduleCandidatePresenterCleanup(_In_ CCandidateListUIPresenter *pPresenter)
{
    if (pPresenter == nullptr)
    {
        return;
    }

    pPresenter->_PrepareForAsyncCleanup();
    _pendingCandidatePresenterCleanup.push_back(pPresenter);

    if (_msgWndHandle)
    {
        PostMessage(_msgWndHandle, WM_CleanupCandidatePresenter, 0, 0);
    }
}

void CMetasequoiaIME::_DrainPendingCandidatePresenterCleanup()
{
    PerfTimer timer;
    size_t cleanedCount = 0;
    while (!_pendingCandidatePresenterCleanup.empty())
    {
        CCandidateListUIPresenter *pPresenter = _pendingCandidatePresenterCleanup.front();
        _pendingCandidatePresenterCleanup.pop_front();
        if (pPresenter)
        {
            PerfTimer deleteTimer;
            delete pPresenter;
            ++cleanedCount;
        }
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
            /* 宿主是非感知程序，需要反向缩放 */
            DPI_AWARENESS_CONTEXT oldCtx = SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE);
            HMONITOR hMon = MonitorFromWindow(hwndTarget, MONITOR_DEFAULTTONEAREST);
            UINT dpiX = 96, dpiY = 96;
            GetDpiForMonitor(hMon, MDT_EFFECTIVE_DPI, &dpiX, &dpiY);
            SetThreadDpiAwarenessContext(oldCtx);
            Global::DpiScale = dpiX / 96.0;
        }
    }

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
            SendIMEDeactivationEventToUIProcessViaNamedPipe();
        }

        pProfileMgr->Release();
    }

    /* 清理 IPC 线程 */
    _shouldStopIpcThread = true;
    if (_hToTsfWorkerThreadPipe && _hToTsfWorkerThreadPipe != INVALID_HANDLE_VALUE)
    {
        CancelIoEx(_hToTsfWorkerThreadPipe, nullptr);
    }
    // Clean IPC
    /* 必须先 CloseIpc，再 join 线程 */
    CloseIpc();
    _hToTsfWorkerThreadPipe = nullptr;
    if (_pIpcThread && _pIpcThread->joinable())
    {
        _pIpcThread->join();
        delete _pIpcThread;
        _pIpcThread = nullptr;
    }
    // TODO: 去掉共享内存，只保留命名管道
    // CloseNamedpipe();

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

        _candidateMode = CANDIDATE_NONE;
        _isCandidateWithWildcard = FALSE;
    }

    if (pContext)
    {
        pContext->Release();
    }

    _DrainPendingCandidatePresenterCleanup();

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
            if (!pIME->_hToTsfWorkerThreadPipe || pIME->_hToTsfWorkerThreadPipe == INVALID_HANDLE_VALUE)
            {
                Sleep(20);
                continue;
            }

            DWORD bytesRead = 0;
            FanyImeNamedpipeDataToTsfWorkerThread buf;
            BOOL readResult = ReadFile(         //
                pIME->_hToTsfWorkerThreadPipe,  //
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
                pIME->_QueuePendingCommitCandidate(buf.data);
                PostMessage(pIME->_msgWndHandle, WM_CommitCandidate, 0, 0);
            }
            else if (buf.msg_type == Global::DataToTsfWorkerThreadMsgType::SwitchToEnglish)
            {
                PostMessage(                                               //
                    pIME->_msgWndHandle,                                   //
                    WM_CheckGlobalCompartment,                             //
                    Global::DataToTsfWorkerThreadMsgType::SwitchToEnglish, //
                    0);
            }
            else if (buf.msg_type == Global::DataToTsfWorkerThreadMsgType::SwitchToChinese)
            {
                PostMessage(                                               //
                    pIME->_msgWndHandle,                                   //
                    WM_CheckGlobalCompartment,                             //
                    Global::DataToTsfWorkerThreadMsgType::SwitchToChinese, //
                    0);
            }
            else if (buf.msg_type == Global::DataToTsfWorkerThreadMsgType::SwitchToPuncEn)
            {
                PostMessage(                                              //
                    pIME->_msgWndHandle,                                  //
                    WM_CheckGlobalCompartment,                            //
                    Global::DataToTsfWorkerThreadMsgType::SwitchToPuncEn, //
                    0);
            }
            else if (buf.msg_type == Global::DataToTsfWorkerThreadMsgType::SwitchToPuncCn)
            {
                PostMessage(                                              //
                    pIME->_msgWndHandle,                                  //
                    WM_CheckGlobalCompartment,                            //
                    Global::DataToTsfWorkerThreadMsgType::SwitchToPuncCn, //
                    0);
            }
            else if (buf.msg_type == Global::DataToTsfWorkerThreadMsgType::SwitchToFullwidth)
            {
                PostMessage(                                                 //
                    pIME->_msgWndHandle,                                     //
                    WM_CheckGlobalCompartment,                               //
                    Global::DataToTsfWorkerThreadMsgType::SwitchToFullwidth, //
                    0);
            }
            else if (buf.msg_type == Global::DataToTsfWorkerThreadMsgType::SwitchToHalfwidth)
            {
                PostMessage(                                                 //
                    pIME->_msgWndHandle,                                     //
                    WM_CheckGlobalCompartment,                               //
                    Global::DataToTsfWorkerThreadMsgType::SwitchToHalfwidth, //
                    0);
            }
            else if (buf.msg_type == Global::DataToTsfWorkerThreadMsgType::PagingCommaPeriodChanged)
            {
                const bool enabled = buf.data[0] == L'1';
                Global::PagingCommaPeriodEnabled.store(enabled, std::memory_order_relaxed);
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
            pIME->GetCompositionProcessorEngine()->SetIMEMode(pIME->_GetThreadMgr(), pIME->_GetClientId(), FALSE);
        }
        else if (wParam == Global::DataToTsfWorkerThreadMsgType::SwitchToChinese)
        {
            pIME->GetCompositionProcessorEngine()->SetIMEMode(pIME->_GetThreadMgr(), pIME->_GetClientId(), TRUE);
        }
        else if (wParam == Global::DataToTsfWorkerThreadMsgType::SwitchToPuncEn)
        {
            pIME->GetCompositionProcessorEngine()->SetPunctuationMode(pIME->_GetThreadMgr(), pIME->_GetClientId(),
                                                                      FALSE);
        }
        else if (wParam == Global::DataToTsfWorkerThreadMsgType::SwitchToPuncCn)
        {
            pIME->GetCompositionProcessorEngine()->SetPunctuationMode(pIME->_GetThreadMgr(), pIME->_GetClientId(),
                                                                      TRUE);
        }
        else if (wParam == Global::DataToTsfWorkerThreadMsgType::SwitchToFullwidth)
        {
            pIME->GetCompositionProcessorEngine()->SetDoubleSingleByteMode(pIME->_GetThreadMgr(), pIME->_GetClientId(),
                                                                           TRUE);
        }
        else if (wParam == Global::DataToTsfWorkerThreadMsgType::SwitchToHalfwidth)
        {
            pIME->GetCompositionProcessorEngine()->SetDoubleSingleByteMode(pIME->_GetThreadMgr(), pIME->_GetClientId(),
                                                                           FALSE);
        }
        break;
    }
    case WM_ConnectNamedpipe: {
        KillTimer(hWnd, TIMER_CONNECT_ALL_NAMEDPIPE);
        g_connectAllNamedpipeRetryCount = 0;
        SetTimer(hWnd, TIMER_CONNECT_ALL_NAMEDPIPE, CONNECT_NAMEDPIPE_RETRY_INTERVAL_MS, nullptr);
        break;
    }
    case WM_DisconnectNamedpipe: {
        KillTimer(hWnd, TIMER_CONNECT_ALL_NAMEDPIPE);
        KillTimer(hWnd, TIMER_CONNECT_TO_TSF_NAMEDPIPE);
        g_connectAllNamedpipeRetryCount = 0;
        g_connectToTsfNamedpipeRetryCount = 0;
        SendClientDeactivatedEventToServerViaNamedPipe();
        break;
    }
    case WM_ConnectToTsfNamedpipe: {
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

            if (ConnectToAllNamedpipe())
            {
                pIME->_hToTsfWorkerThreadPipe = GetToTsfWorkerThreadNamedpipe();
                KillTimer(hWnd, TIMER_CONNECT_ALL_NAMEDPIPE);
                g_connectAllNamedpipeRetryCount = 0;
                SendClientActivatedEventToServerViaNamedPipe();
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
    case WM_AsyncFinalizeCandidate: {
        PerfTimer timer;
        ITfDocumentMgr *pDocMgrFocus = nullptr;
        ITfContext *pContext = nullptr;

        if (SUCCEEDED(pIME->_GetThreadMgr()->GetFocus(&pDocMgrFocus)) && pDocMgrFocus)
        {
            if (SUCCEEDED(pDocMgrFocus->GetTop(&pContext)) && pContext)
            {
                _KEYSTROKE_STATE KeystrokeState;
                KeystrokeState.Category = CATEGORY_CANDIDATE;
                KeystrokeState.Function = FUNCTION_FINALIZE_CANDIDATELIST;
                pIME->_InvokeKeyHandler(pContext, VK_SPACE, L' ', 0, KeystrokeState);
                pContext->Release();
            }
            pDocMgrFocus->Release();
        }
        break;
    }
    case WM_AsyncPunctuationCommit: {
        PerfTimer timer;
        ITfDocumentMgr *pDocMgrFocus = nullptr;
        ITfContext *pContext = nullptr;
        const UINT code = static_cast<UINT>(wParam);
        const WCHAR wch = static_cast<WCHAR>(lParam);

        if (SUCCEEDED(pIME->_GetThreadMgr()->GetFocus(&pDocMgrFocus)) && pDocMgrFocus)
        {
            if (SUCCEEDED(pDocMgrFocus->GetTop(&pContext)) && pContext)
            {
                const bool useDirectPunctuationSession = pIME->_candidateMode == CANDIDATE_NONE && !pIME->_IsComposing();
                if (useDirectPunctuationSession)
                {
                    pIME->_RequestDirectPunctuationEditSession(pContext, code, wch);
                }
                else
                {
                    _KEYSTROKE_STATE KeystrokeState;
                    KeystrokeState.Category = CATEGORY_COMPOSING;
                    KeystrokeState.Function = FUNCTION_PUNCTUATION;
                    pIME->_InvokeKeyHandler(pContext, code, wch, 0, KeystrokeState);
                }
                pContext->Release();
            }
            pDocMgrFocus->Release();
        }
        break;
    }
    case WM_AsyncServerCandidateKey: {
        const UINT code = static_cast<UINT>(wParam);
        const WCHAR wch = static_cast<WCHAR>(lParam);
        FanyImeNamedpipeDataToTsf *receivedData = TryReadDataFromServerPipeWithTimeout();
        if (receivedData->msg_type == Global::DataFromServerMsgType::Normal &&
            std::wstring(receivedData->candidate_string) == L"T")
        {
            receivedData = TryReadDataFromServerPipeWithTimeout();
        }

        if (receivedData->msg_type == Global::DataFromServerMsgType::Normal)
        {
            if (wch == 0)
            {
                break;
            }
            const WCHAR *punctuation = pIME->_pCompositionProcessorEngine->GetPunctuation(wch);
            std::wstring commitText = receivedData->candidate_string;
            if (punctuation)
            {
                commitText.append(punctuation);
            }
            pIME->_QueuePendingPunctuationCommitText(commitText.c_str());
            PostMessage(hWnd, WM_AsyncPunctuationCommit, code, static_cast<LPARAM>(wch));
        }
        // Navigation responses are acknowledgements only. The Server owns and
        // refreshes candidate paging/selection state, so TSF must not apply the
        // same movement to its presenter a second time.
        break;
    }
    case WM_AsyncNumberCandidateCommit: {
        PerfTimer timer;
        ITfDocumentMgr *pDocMgrFocus = nullptr;
        ITfContext *pContext = nullptr;

        if (SUCCEEDED(pIME->_GetThreadMgr()->GetFocus(&pDocMgrFocus)) && pDocMgrFocus)
        {
            if (SUCCEEDED(pDocMgrFocus->GetTop(&pContext)) && pContext)
            {
                _KEYSTROKE_STATE KeystrokeState;
                KeystrokeState.Category = CATEGORY_CANDIDATE;
                KeystrokeState.Function = FUNCTION_SELECT_BY_NUMBER;
                pIME->_InvokeKeyHandler(pContext, static_cast<UINT>(wParam), static_cast<WCHAR>(lParam), 0,
                                        KeystrokeState);
                pContext->Release();
            }
            pDocMgrFocus->Release();
        }
        break;
    }
    case WM_CleanupCandidatePresenter: {
        pIME->_DrainPendingCandidatePresenterCleanup();
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
        if (_pITfFnSearchCandidateProvider != nullptr)
        {
            hr = _pITfFnSearchCandidateProvider->QueryInterface(riid, (void **)ppunk);
        }
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
