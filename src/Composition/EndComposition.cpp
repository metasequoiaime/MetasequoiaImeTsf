#include "Private.h"
#include "Globals.h"
#include "EditSession.h"
#include "MetasequoiaIME.h"
#include <debugapi.h>
#include <fmt/xchar.h>
#include "../Utils/PerfTimer.h"

//////////////////////////////////////////////////////////////////////
//
//    ITfEditSession
//        CEditSessionBase
// CEndCompositionEditSession class
//
//////////////////////////////////////////////////////////////////////

//+---------------------------------------------------------------------------
//
// CEndCompositionEditSession
//
//----------------------------------------------------------------------------

class CEndCompositionEditSession : public CEditSessionBase
{
  public:
    CEndCompositionEditSession(_In_ CMetasequoiaIME *pTextService, _In_ ITfContext *pContext)
        : CEditSessionBase(pTextService, pContext)
    {
    }

    // ITfEditSession
    STDMETHODIMP DoEditSession(TfEditCookie ec)
    {
        _pTextService->_TerminateComposition(ec, _pContext, TRUE);
        return S_OK;
    }
};

//////////////////////////////////////////////////////////////////////
//
// CMetasequoiaIME class
//
//////////////////////////////////////////////////////////////////////

//+---------------------------------------------------------------------------
//
// _TerminateComposition
//
//----------------------------------------------------------------------------

void CMetasequoiaIME::_TerminateComposition(TfEditCookie ec, _In_ ITfContext *pContext, BOOL isCalledFromDeactivate)
{
    isCalledFromDeactivate;
    PerfTimer timer;

    if (_pComposition != nullptr)
    {
        // remove the display attribute from the composition range.
        PerfTimer clearDisplayAttrTimer;
        _ClearCompositionDisplayAttributes(ec, pContext);
        double clearDisplayAttrElapsedMs = clearDisplayAttrTimer.ElapsedMs();

        PerfTimer endCompositionTimer;
        if (FAILED(_pComposition->EndComposition(ec)))
        {
            // if we fail to EndComposition, then we need to close the reverse reading window.
            _DeleteCandidateList(TRUE, pContext);
        }
        double endCompositionElapsedMs = endCompositionTimer.ElapsedMs();

        PerfTimer releaseCompositionTimer;
        _pComposition->Release();
        _pComposition = nullptr;
        double releaseCompositionElapsedMs = releaseCompositionTimer.ElapsedMs();

        double releaseContextElapsedMs = 0;
        if (_pContext)
        {
            PerfTimer releaseContextTimer;
            _pContext->Release();
            _pContext = nullptr;
            releaseContextElapsedMs = releaseContextTimer.ElapsedMs();
        }

        OutputDebugString(fmt::format(
                              L"[msime-perf] _TerminateComposition elapsed={:.3f}ms clear_display_attr={:.3f}ms end_composition={:.3f}ms release_composition={:.3f}ms release_context={:.3f}ms",
                              timer.ElapsedMs(), clearDisplayAttrElapsedMs, endCompositionElapsedMs,
                              releaseCompositionElapsedMs, releaseContextElapsedMs)
                              .c_str());
    }
}

//+---------------------------------------------------------------------------
//
// _EndComposition
//
//----------------------------------------------------------------------------

void CMetasequoiaIME::_EndComposition(_In_opt_ ITfContext *pContext)
{
    CEndCompositionEditSession *pEditSession = new (std::nothrow) CEndCompositionEditSession(this, pContext);
    HRESULT hr = S_OK;
    PerfTimer timer;

    if (nullptr != pEditSession)
    {
        PerfTimer requestTimer;
        pContext->RequestEditSession(_tfClientId, pEditSession, TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, &hr);
        double requestElapsedMs = requestTimer.ElapsedMs();
        pEditSession->Release();
        OutputDebugString(fmt::format(L"[msime-perf] _EndComposition elapsed={:.3f}ms request_edit_session={:.3f}ms hr={:#x}",
                                      timer.ElapsedMs(), requestElapsedMs, static_cast<unsigned int>(hr))
                              .c_str());
    }
}
