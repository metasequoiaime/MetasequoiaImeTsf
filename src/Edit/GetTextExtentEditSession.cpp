#include "Private.h"
#include "EditSession.h"
#include "GetTextExtentEditSession.h"
#include "TfTextLayoutSink.h"
#include "Ipc.h"
#include "FanyDefines.h"
#include <debugapi.h>
#include <fmt/xchar.h>
#include "../Utils/PerfTimer.h"

//+---------------------------------------------------------------------------
//
// ctor
//
//----------------------------------------------------------------------------

CGetTextExtentEditSession::CGetTextExtentEditSession(_In_ CMetasequoiaIME *pTextService, _In_ ITfContext *pContext,
                                                     _In_ ITfContextView *pContextView,
                                                     _In_ ITfRange *pRangeComposition,
                                                     _In_ CTfTextLayoutSink *pTfTextLayoutSink)
    : CEditSessionBase(pTextService, pContext)
{
    _pContextView = pContextView;
    _pRangeComposition = pRangeComposition;
    _pTfTextLayoutSink = pTfTextLayoutSink;
}

//+---------------------------------------------------------------------------
//
// ITfEditSession::DoEditSession
//
//----------------------------------------------------------------------------

STDAPI CGetTextExtentEditSession::DoEditSession(TfEditCookie ec)
{
    PerfTimer timer;
    RECT rc = {0, 0, 0, 0};
    BOOL isClipped = TRUE;
    HRESULT hr = S_OK;
    double getTextExtElapsedMs = 0;
    double layoutChangeElapsedMs = 0;

    PerfTimer getTextExtTimer;
    hr = _pContextView->GetTextExt(ec, _pRangeComposition, &rc, &isClipped);
    getTextExtElapsedMs = getTextExtTimer.ElapsedMs();

    if (SUCCEEDED(hr))
    {
#ifdef FANY_DEBUG
        // TODO: Log rc position
#endif
        Global::Point[0] = rc.left * Global::DpiScale;
        Global::Point[1] = rc.bottom * Global::DpiScale;
        if (Global::current_process_name == Global::ZEN_BROWSER)
        {
            Global::firefox_like_cnt++;
            if (Global::firefox_like_cnt == 3)
            {
                PerfTimer layoutChangeTimer;
                _pTfTextLayoutSink->_LayoutChangeNotification(&rc);
                layoutChangeElapsedMs = layoutChangeTimer.ElapsedMs();
            }
        }
        else
        {
            PerfTimer layoutChangeTimer;
            _pTfTextLayoutSink->_LayoutChangeNotification(&rc);
            layoutChangeElapsedMs = layoutChangeTimer.ElapsedMs();
        }
    }


    return S_OK;
}
