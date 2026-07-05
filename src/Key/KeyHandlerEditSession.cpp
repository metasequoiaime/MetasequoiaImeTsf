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
    HRESULT hResult = S_OK;
    PerfTimer doEditSessionTimer;
    LARGE_INTEGER freq, nowQpc;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&nowQpc);
    double queueElapsedMs = static_cast<double>(nowQpc.QuadPart - _requestStartQpc.QuadPart) * 1000.0 /
                            static_cast<double>(freq.QuadPart);

    CKeyStateCategoryFactory *pKeyStateCategoryFactory = CKeyStateCategoryFactory::Instance();
    CKeyStateCategory *pKeyStateCategory =
        pKeyStateCategoryFactory->MakeKeyStateCategory(_KeyState.Category, _pTextService);

    if (pKeyStateCategory)
    {
        KeyHandlerEditSessionDTO keyHandlerEditSessioDTO(ec, _pContext, _uCode, _wch, _KeyState.Function);
        hResult = pKeyStateCategory->KeyStateHandler(_KeyState.Function, keyHandlerEditSessioDTO);

        pKeyStateCategory->Release();
        pKeyStateCategoryFactory->Release();
    }


    return hResult;
}
