#include "Ipc.h"
#include "Private.h"
#include "MetasequoiaIME.h"
#include "CandidateListUIPresenter.h"
#include <debugapi.h>
#include <cwchar>
#include "fmt/xchar.h"

namespace
{
bool IsWindowsTextInputHostWindow(HWND window)
{
    if (!window)
    {
        return false;
    }

    DWORD processId = 0;
    GetWindowThreadProcessId(window, &processId);
    if (processId == 0)
    {
        return false;
    }

    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE,
                                 processId);
    if (!process)
    {
        return false;
    }

    WCHAR imagePath[32768] = {};
    DWORD imagePathLength = ARRAYSIZE(imagePath);
    const bool queried =
        QueryFullProcessImageNameW(process, 0, imagePath, &imagePathLength) !=
        FALSE;
    CloseHandle(process);
    if (!queried || imagePathLength == 0)
    {
        return false;
    }

    const WCHAR *fileName = wcsrchr(imagePath, L'\\');
    fileName = fileName ? fileName + 1 : imagePath;
    return _wcsicmp(fileName, L"TextInputHost.exe") == 0;
}
} // namespace

bool CMetasequoiaIME::_CaptureWindowsTextInputHostFocusLoss()
{
    _focusLostToWindowsTextInputHost =
        _focusLostToWindowsTextInputHost ||
        IsWindowsTextInputHostWindow(GetForegroundWindow());
    return _focusLostToWindowsTextInputHost;
}

//+---------------------------------------------------------------------------
//
// ITfTextLayoutSink::OnSetThreadFocus
//
//----------------------------------------------------------------------------

STDAPI CMetasequoiaIME::OnSetThreadFocus()
{
    if (!IsNamedpipeFocusStateOwner(this))
    {
        return S_OK;
    }
    // Do not consume this marker here. TextInputHost owns a separate TSF
    // client, so the authoritative document OnSetFocus must use it to force a
    // new Server activation even when our local pipe handles still look live.
    // Match bcaf34e: thread focus is observational. Document OnSetFocus owns
    // disconnect/reconnect; this callback only refreshes status/presenter.
    if (_msgWndHandle && IsWindow(_msgWndHandle))
    {
        PostMessage(_msgWndHandle, WM_ThreadFocus, 0, 0);
    }
    if (_pCandidateListUIPresenter)
    {
        ITfDocumentMgr *pCandidateListDocumentMgr = nullptr;
        ITfContext *pTfContext = _pCandidateListUIPresenter->_GetContextDocument();

        if ((nullptr != pTfContext) && SUCCEEDED(pTfContext->GetDocumentMgr(&pCandidateListDocumentMgr)))
        {
            if (pCandidateListDocumentMgr == _pDocMgrLastFocused)
            {
                _pCandidateListUIPresenter->OnSetThreadFocus();
            }

            pCandidateListDocumentMgr->Release();
        }
    }
    return S_OK;
}

//+---------------------------------------------------------------------------
//
// ITfTextLayoutSink::OnKillThreadFocus
//
//----------------------------------------------------------------------------

STDAPI CMetasequoiaIME::OnKillThreadFocus()
{
    if (!IsNamedpipeFocusStateOwner(this) || !Global::g_connected)
    {
        return S_OK;
    }
    _focusLostToWindowsTextInputHost = false;
    (void)_CaptureWindowsTextInputHostFocusLoss();

    // ITfThreadFocusSink reports temporary auxiliary UI transitions as well
    // as real application focus changes. The known-good implementation did
    // not use this callback as an IPC/session boundary: document/top-context
    // changes are already handled by ITfThreadMgrEventSink, and TIP switching
    // is handled by Deactivate. Keep the current composition and pipe epoch
    // intact here, especially for TextInputHost.exe / Win+..
    if (_pCandidateListUIPresenter)
    {
        ITfDocumentMgr *pCandidateListDocumentMgr = nullptr;
        ITfContext *pTfContext = _pCandidateListUIPresenter->_GetContextDocument();

        if ((nullptr != pTfContext) && SUCCEEDED(pTfContext->GetDocumentMgr(&pCandidateListDocumentMgr)))
        {
            if (_pDocMgrLastFocused)
            {
                _pDocMgrLastFocused->Release();
                _pDocMgrLastFocused = nullptr;
            }
            // GetDocumentMgr already returned one owned reference. Transfer it
            // directly to _pDocMgrLastFocused; adding another reference here
            // leaks one on every Win+. / thread-focus round trip.
            _pDocMgrLastFocused = pCandidateListDocumentMgr;
            pCandidateListDocumentMgr = nullptr;

        }
        _pCandidateListUIPresenter->OnKillThreadFocus();
    }
    return S_OK;
}

BOOL CMetasequoiaIME::_InitThreadFocusSink()
{
    ITfSource *pSource = nullptr;

    if (FAILED(_pThreadMgr->QueryInterface(IID_ITfSource, (void **)&pSource)))
    {
        return FALSE;
    }

    if (FAILED(pSource->AdviseSink(IID_ITfThreadFocusSink, (ITfThreadFocusSink *)this, &_dwThreadFocusSinkCookie)))
    {
        pSource->Release();
        return FALSE;
    }

    pSource->Release();

    return TRUE;
}

void CMetasequoiaIME::_UninitThreadFocusSink()
{
    ITfSource *pSource = nullptr;

    if (FAILED(_pThreadMgr->QueryInterface(IID_ITfSource, (void **)&pSource)))
    {
        return;
    }

    if (FAILED(pSource->UnadviseSink(_dwThreadFocusSinkCookie)))
    {
        pSource->Release();
        return;
    }

    pSource->Release();
}
