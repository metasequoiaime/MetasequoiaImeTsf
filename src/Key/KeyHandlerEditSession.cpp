#include "Private.h"
#include "KeyHandlerEditSession.h"
#include "EditSession.h"
#include "MetasequoiaIME.h"
#include "CompositionProcessorEngine.h"
#include "KeyStateCategory.h"
#include "fmt/xchar.h"
#include "../Utils/PerfTimer.h"

//////////////////////////////////////////////////////////////////////
//
//    ITfEditSession
//        CEditSessionBase
// CKeyHandlerEditSession class
//
//////////////////////////////////////////////////////////////////////

//+---------------------------------------------------------------------------
//
// CKeyHandlerEditSession::DoEditSession
//
//----------------------------------------------------------------------------

STDAPI CKeyHandlerEditSession::DoEditSession(TfEditCookie ec)
{
    struct DeferredReplayCompletion
    {
        CMetasequoiaIME *textService;
        uint64_t token;
        bool applied = false;
        ~DeferredReplayCompletion()
        {
            if (textService && token != 0)
            {
                if (applied)
                {
                    textService->_CompleteDeferredKeyReplay(token);
                }
                else
                {
                    textService->_RetryDeferredKeyReplay(token);
                }
            }
        }
    } deferredReplayCompletion{_pTextService, _deferredReplayToken, false};

    const bool isLocalReset = _localResetToken != 0;
    if (isLocalReset)
    {
        if (!_pTextService->_IsLocalSessionResetCurrent(_localResetToken))
        {
            // A newer focus/reset token may supersede this granted session.
            // It must still retire its exact queue slot so Complete can queue
            // the newer token; returning without bookkeeping would gate every
            // subsequent key forever.
            _pTextService->_CompleteLocalSessionReset(_localResetToken);
            return S_FALSE;
        }
    }
    else if (!_pTextService->_IsServerUnavailableFallbackActive() &&
             !_pTextService->_IsFocusSessionCurrent(_focusToken, _pContext))
    {
        return S_FALSE;
    }
    if (!isLocalReset && _expectedCompositionEpoch != 0 &&
        !_pTextService->_IsCompositionEpochCurrent(_expectedCompositionEpoch))
    {
        return S_FALSE;
    }

    HRESULT hResult = S_OK;
    PerfTimer doEditSessionTimer;
    LARGE_INTEGER freq, nowQpc;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&nowQpc);
    double queueElapsedMs = static_cast<double>(nowQpc.QuadPart - _requestStartQpc.QuadPart) * 1000.0 /
                            static_cast<double>(freq.QuadPart);

    CKeyStateCategoryFactory *pKeyStateCategoryFactory = CKeyStateCategoryFactory::Instance();
    CKeyStateCategory *pKeyStateCategory = pKeyStateCategoryFactory
                                              ? pKeyStateCategoryFactory->MakeKeyStateCategory(
                                                    _KeyState.Category, _pTextService)
                                              : nullptr;

    if (pKeyStateCategory)
    {
        KeyHandlerEditSessionDTO keyHandlerEditSessioDTO(ec, _pContext, _uCode, _wch, _KeyState.Function,
                                                         _requestId, _prefetchedText);
        hResult = pKeyStateCategory->KeyStateHandler(_KeyState.Function, keyHandlerEditSessioDTO);
        deferredReplayCompletion.applied = hResult == S_OK;

        pKeyStateCategory->Release();
    }
    if (pKeyStateCategoryFactory)
    {
        pKeyStateCategoryFactory->Release();
    }

    if (isLocalReset)
    {
        _pTextService->_CompleteLocalSessionReset(_localResetToken);
    }
    return hResult;
}
