#include "Private.h"
#include "fmt/xchar.h"
#include "Globals.h"
#include "MetasequoiaIME.h"
#include "CandidateListUIPresenter.h"
#include "Ipc.h"

//+---------------------------------------------------------------------------
//
// ITfThreadMgrEventSink::OnInitDocumentMgr
//
// Sink called by the framework just before the first context is pushed onto
// a document.
//----------------------------------------------------------------------------

STDAPI CMetasequoiaIME::OnInitDocumentMgr(_In_ ITfDocumentMgr *pDocMgr)
{
    pDocMgr;
    return E_NOTIMPL;
}

//+---------------------------------------------------------------------------
//
// ITfThreadMgrEventSink::OnUninitDocumentMgr
//
// Sink called by the framework just after the last context is popped off a
// document.
//----------------------------------------------------------------------------

STDAPI CMetasequoiaIME::OnUninitDocumentMgr(_In_ ITfDocumentMgr *pDocMgr)
{
    pDocMgr;
    return E_NOTIMPL;
}

//+---------------------------------------------------------------------------
//
// ITfThreadMgrEventSink::OnSetFocus
//
// Sink called by the framework when focus changes from one document to
// another.  Either document may be NULL, meaning previously there was no
// focus document, or now no document holds the input focus.
//----------------------------------------------------------------------------

STDAPI CMetasequoiaIME::OnSetFocus(_In_ ITfDocumentMgr *pDocMgrFocus, _In_ ITfDocumentMgr *pDocMgrPrevFocus)
{
    if (!IsNamedpipeFocusStateOwner(this))
    {
        return S_OK;
    }
    pDocMgrPrevFocus;

    const bool windowsTextInputHostTransition =
        _focusLostToWindowsTextInputHost ||
        _CaptureWindowsTextInputHostFocusLoss();

    // bcaf34e used document focus as the authoritative reconnect boundary.
    // The modern protocol preserves that behavior with a new activation epoch
    // on the existing healthy transport; EnsureNamedpipeFocusSessionActivated
    // physically reopens a channel as well when its health check fails.
    if (pDocMgrFocus &&
        (!Global::g_connected || windowsTextInputHostTransition))
    {
        Global::g_connected = true;
        _workerCommitReady.store(false, std::memory_order_release);
        RequireNamedpipeFocusActivation();
        PostMessage(_msgWndHandle, WM_ConnectNamedpipe, 0, 0);
        _focusLostToWindowsTextInputHost = false;
    }
    else if (!pDocMgrFocus && Global::g_connected)
    {
        MarkNamedpipeFocusLost();
        Global::g_connected = false;
        PostMessage(_msgWndHandle, WM_DisconnectNamedpipe, 0, 0);
    }

    _InitTextEditSink(pDocMgrFocus);

    _UpdateLanguageBarOnSetFocus(pDocMgrFocus);

    //
    // We have to hide/unhide candidate list depending on whether they are
    // associated with pDocMgrFocus.
    //
    if (_pCandidateListUIPresenter)
    {
        ITfDocumentMgr *pCandidateListDocumentMgr = nullptr;
        ITfContext *pTfContext = _pCandidateListUIPresenter->_GetContextDocument();
        if ((nullptr != pTfContext) && SUCCEEDED(pTfContext->GetDocumentMgr(&pCandidateListDocumentMgr)))
        {
            if (pCandidateListDocumentMgr != pDocMgrFocus)
            {
                _pCandidateListUIPresenter->OnKillThreadFocus();
            }
            else
            {
                _pCandidateListUIPresenter->OnSetThreadFocus();
            }

            pCandidateListDocumentMgr->Release();
        }
    }

    if (_pDocMgrLastFocused)
    {
        _pDocMgrLastFocused->Release();
        _pDocMgrLastFocused = nullptr;
    }

    _pDocMgrLastFocused = pDocMgrFocus;

    if (_pDocMgrLastFocused)
    {
        _pDocMgrLastFocused->AddRef();
    }

    return S_OK;
}

//+---------------------------------------------------------------------------
//
// ITfThreadMgrEventSink::OnPushContext
//
// Sink called by the framework when a context is pushed.
//----------------------------------------------------------------------------

STDAPI CMetasequoiaIME::OnPushContext(_In_ ITfContext *pContext)
{
    _HandleFocusedContextStackChange(pContext);
    return S_OK;
}

//+---------------------------------------------------------------------------
//
// ITfThreadMgrEventSink::OnPopContext
//
// Sink called by the framework when a context is popped.
//----------------------------------------------------------------------------

STDAPI CMetasequoiaIME::OnPopContext(_In_ ITfContext *pContext)
{
    _HandleFocusedContextStackChange(pContext);
    return S_OK;
}

void CMetasequoiaIME::_HandleFocusedContextStackChange(_In_opt_ ITfContext *changedContext)
{
    if (!IsNamedpipeFocusStateOwner(this) || changedContext == nullptr ||
        _pThreadMgr == nullptr)
    {
        return;
    }

    ITfDocumentMgr *changedDocument = nullptr;
    ITfDocumentMgr *focusedDocument = nullptr;
    if (FAILED(changedContext->GetDocumentMgr(&changedDocument)) ||
        changedDocument == nullptr ||
        FAILED(_pThreadMgr->GetFocus(&focusedDocument)) ||
        focusedDocument == nullptr || changedDocument != focusedDocument)
    {
        if (changedDocument)
        {
            changedDocument->Release();
        }
        if (focusedDocument)
        {
            focusedDocument->Release();
        }
        return;
    }

    ITfContext *newTopContext = nullptr;
    if (FAILED(focusedDocument->GetTop(&newTopContext)) || newTopContext == nullptr)
    {
        focusedDocument->Release();
        changedDocument->Release();
        return;
    }
    const bool topContextChanged = newTopContext != _pTextEditSinkContext;
    newTopContext->Release();
    if (!topContextChanged)
    {
        focusedDocument->Release();
        changedDocument->Release();
        return;
    }

    // Capture the old composition owner before rebinding the text-edit sink.
    // A pushed modal/context (or a popped editor context) must never inherit
    // replies or queued edit sessions that belonged to the previous top.
    ITfContext *resetContext = _pContext ? _pContext : _pTextEditSinkContext;
    if (resetContext)
    {
        resetContext->AddRef();
    }
    else
    {
        resetContext = changedContext;
        resetContext->AddRef();
    }

    _ClearDeferredKeyDowns();
    MarkNamedpipeSessionDirtyForOwner(this);
    const UINT resetToken = _localSessionResetToken.load(std::memory_order_acquire);
    _RequestLocalSessionReset(resetContext, resetToken);
    _InitTextEditSink(focusedDocument);

    resetContext->Release();
    focusedDocument->Release();
    changedDocument->Release();
}

//+---------------------------------------------------------------------------
//
// _InitThreadMgrEventSink
//
// Advise our sink.
//----------------------------------------------------------------------------

BOOL CMetasequoiaIME::_InitThreadMgrEventSink()
{
    ITfSource *pSource = nullptr;
    BOOL ret = FALSE;

    if (FAILED(_pThreadMgr->QueryInterface(IID_ITfSource, (void **)&pSource)))
    {
        return ret;
    }

    if (FAILED(
            pSource->AdviseSink(IID_ITfThreadMgrEventSink, (ITfThreadMgrEventSink *)this, &_threadMgrEventSinkCookie)))
    {
        _threadMgrEventSinkCookie = TF_INVALID_COOKIE;
        goto Exit;
    }

    ret = TRUE;

Exit:
    pSource->Release();
    return ret;
}

//+---------------------------------------------------------------------------
//
// _UninitThreadMgrEventSink
//
// Unadvise our sink.
//----------------------------------------------------------------------------

void CMetasequoiaIME::_UninitThreadMgrEventSink()
{
    ITfSource *pSource = nullptr;

    if (_threadMgrEventSinkCookie == TF_INVALID_COOKIE)
    {
        return;
    }

    if (SUCCEEDED(_pThreadMgr->QueryInterface(IID_ITfSource, (void **)&pSource)))
    {
        pSource->UnadviseSink(_threadMgrEventSinkCookie);
        pSource->Release();
    }

    _threadMgrEventSinkCookie = TF_INVALID_COOKIE;
}
