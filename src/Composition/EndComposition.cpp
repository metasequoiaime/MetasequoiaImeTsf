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
    }
}
