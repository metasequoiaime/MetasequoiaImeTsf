#pragma once

#include "EditSession.h"
#include "Globals.h"

class CKeyHandlerEditSession : public CEditSessionBase
{
  public:
    CKeyHandlerEditSession(CMetasequoiaIME *pTextService, ITfContext *pContext, UINT uCode, WCHAR wch,
                           _KEYSTROKE_STATE keyState, LARGE_INTEGER requestStartQpc = {})
        : CEditSessionBase(pTextService, pContext)
    {
        _uCode = uCode;
        _wch = wch;
        _KeyState = keyState;
        _requestStartQpc = requestStartQpc;
        if (_requestStartQpc.QuadPart == 0)
        {
            QueryPerformanceCounter(&_requestStartQpc);
        }
    }

    // ITfEditSession
    STDMETHODIMP DoEditSession(TfEditCookie ec);

  private:
    UINT _uCode;                // virtual key code
    WCHAR _wch;                 // character code
    _KEYSTROKE_STATE _KeyState; // key function regarding virtual key
    LARGE_INTEGER _requestStartQpc;
};
