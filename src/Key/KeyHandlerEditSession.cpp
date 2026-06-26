#include "Private.h"
#include "KeyHandlerEditSession.h"
#include "EditSession.h"
#include "MetasequoiaIME.h"
#include "CompositionProcessorEngine.h"
#include "KeyStateCategory.h"
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
