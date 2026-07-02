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
    OutputDebugString(fmt::format(L"[msime-perf] DoEditSession begin keycode={} category={} function={} queue_elapsed={:.3f}ms",
                                  _uCode, static_cast<int>(_KeyState.Category),
                                  static_cast<int>(_KeyState.Function), queueElapsedMs)
                          .c_str());

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

    OutputDebugString(fmt::format(L"[msime-perf] DoEditSession end keycode={} category={} function={} elapsed={:.3f}ms hr={:#x}",
                                  _uCode, static_cast<int>(_KeyState.Category),
                                  static_cast<int>(_KeyState.Function), doEditSessionTimer.ElapsedMs(),
                                  static_cast<unsigned int>(hResult))
                          .c_str());

    return hResult;
}
