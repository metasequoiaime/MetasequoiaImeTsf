#include "Private.h"
#include "Globals.h"
#include "MetasequoiaIME.h"
#include "CandidateListUIPresenter.h"
#include "CompositionProcessorEngine.h"
#include "KeyHandlerEditSession.h"
#include "Compartment.h"
#include "MetasequoiaIMEBaseStructure.h"
#include "fmt/xchar.h"
#include <debugapi.h>
#include <string>
#include "Ipc.h"
#include "FanyUtils.h"
#include "FanyLog.h"

// 0xF003, 0xF004 are the keys that the touch keyboard sends for next/previous
#define THIRDPARTY_NEXTPAGE static_cast<WORD>(0xF003)
#define THIRDPARTY_PREVPAGE static_cast<WORD>(0xF004)

// Because the code mostly works with VKeys, here map a WCHAR back to a VKKey for certain
// vkeys that the IME handles specially
__inline UINT VKeyFromVKPacketAndWchar(UINT vk, WCHAR wch)
{
    UINT vkRet = vk;
    if (LOWORD(vk) == VK_PACKET)
    {
        if (wch == L' ')
        {
            vkRet = VK_SPACE;
        }
        else if ((wch >= L'0') && (wch <= L'9'))
        {
            vkRet = static_cast<UINT>(wch);
        }
        else if ((wch >= L'a') && (wch <= L'z'))
        {
            vkRet = (UINT)(L'A') + ((UINT)(L'z') - static_cast<UINT>(wch));
        }
        else if ((wch >= L'A') && (wch <= L'Z'))
        {
            vkRet = static_cast<UINT>(wch);
        }
        else if (wch == THIRDPARTY_NEXTPAGE)
        {
            vkRet = VK_NEXT;
        }
        else if (wch == THIRDPARTY_PREVPAGE)
        {
            vkRet = VK_PRIOR;
        }
    }
    return vkRet;
}

//+---------------------------------------------------------------------------
//
// _IsKeyEaten
//
//----------------------------------------------------------------------------

BOOL CMetasequoiaIME::_IsKeyEaten(        //
    _In_ ITfContext *pContext,            //
    UINT codeIn,                          //
    _Out_ UINT *pCodeOut,                 //
    _Out_writes_(1) WCHAR *pwch,          //
    _Out_opt_ _KEYSTROKE_STATE *pKeyState //
)
{
    pContext;

    *pCodeOut = codeIn;

    BOOL isOpen = FALSE;
    CCompartment CompartmentKeyboardOpen(_pThreadMgr, _tfClientId, GUID_COMPARTMENT_KEYBOARD_OPENCLOSE);
    CompartmentKeyboardOpen._GetCompartmentBOOL(isOpen);

    BOOL isDoubleSingleByte = FALSE;
    CCompartment CompartmentDoubleSingleByte(_pThreadMgr, _tfClientId,
                                             Global::MetasequoiaIMEGuidCompartmentDoubleSingleByte);
    CompartmentDoubleSingleByte._GetCompartmentBOOL(isDoubleSingleByte);

    BOOL isPunctuation = FALSE;
    CCompartment CompartmentPunctuation(_pThreadMgr, _tfClientId, Global::MetasequoiaIMEGuidCompartmentPunctuation);
    CompartmentPunctuation._GetCompartmentBOOL(isPunctuation);

    if (pKeyState)
    {
        pKeyState->Category = CATEGORY_NONE;
        pKeyState->Function = FUNCTION_NONE;
    }
    if (pwch)
    {
        *pwch = L'\0';
    }

    // If the keyboard is disabled(e.g. no focused edit control), we don't eat keys.
    if (_IsKeyboardDisabled())
    {
        return FALSE;
    }

    //
    // Map virtual key to character code
    //
    BOOL isTouchKeyboardSpecialKeys = FALSE;
    WCHAR wch = ConvertVKey(codeIn);
    *pCodeOut = VKeyFromVKPacketAndWchar(codeIn, wch);
    if ((wch == THIRDPARTY_NEXTPAGE) || (wch == THIRDPARTY_PREVPAGE))
    {
        // We always eat the above softkeyboard special keys
        isTouchKeyboardSpecialKeys = TRUE;
        if (pwch)
        {
            *pwch = wch;
        }
    }

    // if the keyboard is closed, we don't eat keys, with the exception of the touch keyboard specials keys
    if (!isOpen && !isDoubleSingleByte && !isPunctuation)
    {
        return isTouchKeyboardSpecialKeys;
    }

    if (pwch)
    {
        *pwch = wch;
    }

    //
    // Get composition engine
    //
    CCompositionProcessorEngine *pCompositionProcessorEngine;
    pCompositionProcessorEngine = _pCompositionProcessorEngine;

    if (isOpen) // Chinese mode
    {
        const bool isComposing = _IsComposing() ? true : false;
        const bool isCapsLockOn = (GetKeyState(VK_CAPITAL) & 0x0001) != 0;
        const bool isUppercaseAlphabet = (wch >= L'A' && wch <= L'Z') && (*pCodeOut >= L'A' && *pCodeOut <= L'Z');
        const bool isInputInProgress =
            isComposing || (_candidateMode != CANDIDATE_NONE) ||
            (pCompositionProcessorEngine && pCompositionProcessorEngine->GetVirtualKeyLength() > 0);

        // CapsLock ON + uppercase(没有按 Shift) alphabet:
        // - start of input: don't eat
        // - middle of input: eat
        if (isCapsLockOn && isUppercaseAlphabet && !isInputInProgress)
        {
            return isTouchKeyboardSpecialKeys;
        }

        //
        // The candidate or phrase list handles the keys through ITfKeyEventSink.
        //
        // eat only keys that CKeyHandlerEditSession can handles.
        //
        auto ret = pCompositionProcessorEngine->IsVirtualKeyNeed( //
            *pCodeOut,                                            //
            pwch,                                                 //
            isComposing,                                          //
            _candidateMode,                                       //
            _isCandidateWithWildcard,                             //
            pKeyState                                             //
        );
        if (ret)
        {
            return TRUE;
        }
    }

#ifdef FANY_DEBUG
    DebugLog(L"IsPunctuation _candidateMode: {}, isPunctuation: {}", (int)_candidateMode, isPunctuation);
#endif

    //
    // Punctuation
    //
    if (pCompositionProcessorEngine->IsPunctuation(wch))
    {
        if ((_candidateMode == CANDIDATE_NONE || _candidateMode == CANDIDATE_INCREMENTAL) && isPunctuation)
        {
            if (pKeyState)
            {
                pKeyState->Category = CATEGORY_COMPOSING;
                pKeyState->Function = FUNCTION_PUNCTUATION;
            }
            return TRUE;
        }
    }

    //
    // Double/Single byte
    //
    if (isDoubleSingleByte && pCompositionProcessorEngine->IsDoubleSingleByte(wch))
    {
        if (_candidateMode == CANDIDATE_NONE)
        {
            if (pKeyState)
            {
                pKeyState->Category = CATEGORY_COMPOSING;
                pKeyState->Function = FUNCTION_DOUBLE_SINGLE_BYTE;
            }
            return TRUE;
        }
    }

    return isTouchKeyboardSpecialKeys;
}

//+---------------------------------------------------------------------------
//
// ConvertVKey
//
//----------------------------------------------------------------------------

WCHAR CMetasequoiaIME::ConvertVKey(UINT code)
{
    //
    // Map virtual key to scan code
    //
    UINT scanCode = 0;
    scanCode = MapVirtualKey(code, 0);

    //
    // Keyboard state
    //
    BYTE abKbdState[256] = {'\0'};
    if (!GetKeyboardState(abKbdState))
    {
        return 0;
    }

    //
    // Map virtual key to character code
    //
    WCHAR wch = '\0';
    if (ToUnicode(code, scanCode, abKbdState, &wch, 1, 0) == 1)
    {
        return wch;
    }

    return 0;
}

//+---------------------------------------------------------------------------
//
// _IsKeyboardDisabled
//
//----------------------------------------------------------------------------

BOOL CMetasequoiaIME::_IsKeyboardDisabled()
{
    /* Steal from weasel: https://github.com/rime/weasel */
    ITfCompartmentMgr *pCompMgr = NULL;
    ITfDocumentMgr *pDocMgrFocus = NULL;
    ITfContext *pContext = NULL;
    BOOL fDisabled = FALSE;

    if ((_pThreadMgr->GetFocus(&pDocMgrFocus) != S_OK) || (pDocMgrFocus == NULL))
    {
        fDisabled = TRUE;
        goto Exit;
    }

    if ((pDocMgrFocus->GetTop(&pContext) != S_OK) || (pContext == NULL))
    {
        fDisabled = TRUE;
        goto Exit;
    }

    if (pContext->QueryInterface(IID_ITfCompartmentMgr, (void **)&pCompMgr) == S_OK)
    {
        ITfCompartment *pCompartmentDisabled;
        ITfCompartment *pCompartmentEmptyContext;

        /* Check GUID_COMPARTMENT_KEYBOARD_DISABLED */
        if (pCompMgr->GetCompartment(GUID_COMPARTMENT_KEYBOARD_DISABLED, &pCompartmentDisabled) == S_OK)
        {
            VARIANT var;
            if (pCompartmentDisabled->GetValue(&var) == S_OK)
            {
                if (var.vt == VT_I4) // Even VT_EMPTY, GetValue() can succeed
                    fDisabled = (BOOL)var.lVal;
            }
            pCompartmentDisabled->Release();
        }

        /* Check GUID_COMPARTMENT_EMPTYCONTEXT */
        if (pCompMgr->GetCompartment(GUID_COMPARTMENT_EMPTYCONTEXT, &pCompartmentEmptyContext) == S_OK)
        {
            VARIANT var;
            if (pCompartmentEmptyContext->GetValue(&var) == S_OK)
            {
                if (var.vt == VT_I4) // Even VT_EMPTY, GetValue() can succeed
                    fDisabled = (BOOL)var.lVal;
            }
            pCompartmentEmptyContext->Release();
        }
        pCompMgr->Release();
    }

Exit:
    if (pContext)
        pContext->Release();
    if (pDocMgrFocus)
        pDocMgrFocus->Release();
    return fDisabled;
}

//+---------------------------------------------------------------------------
//
// ITfKeyEventSink::OnSetFocus
//
// Called by the system whenever this service gets the keystroke device focus.
//----------------------------------------------------------------------------

STDAPI CMetasequoiaIME::OnSetFocus(BOOL fForeground)
{
    fForeground;

    return S_OK;
}

//+---------------------------------------------------------------------------
//
// ITfKeyEventSink::OnTestKeyDown
//
// Called by the system to query this service wants a potential keystroke.
//----------------------------------------------------------------------------

STDAPI CMetasequoiaIME::OnTestKeyDown(ITfContext *pContext, WPARAM wParam, LPARAM lParam, BOOL *pIsEaten)
{
    Global::UpdateModifiers(wParam, lParam);

    _KEYSTROKE_STATE KeystrokeState;
    WCHAR wch = '\0';
    UINT code = 0;
    *pIsEaten = _IsKeyEaten(pContext, (UINT)wParam, &code, &wch, &KeystrokeState);

    if (KeystrokeState.Category == CATEGORY_INVOKE_COMPOSITION_EDIT_SESSION)
    {
        //
        // Invoke key handler edit session
        //
        KeystrokeState.Category = CATEGORY_COMPOSING;

        _InvokeKeyHandler(pContext, code, wch, (DWORD)lParam, KeystrokeState);
    }

    return S_OK;
}

//+---------------------------------------------------------------------------
//
// ITfKeyEventSink::OnKeyDown
//
// Called by the system to offer this service a keystroke.
// on exit, the application will not handle the keystroke.
//----------------------------------------------------------------------------

STDAPI CMetasequoiaIME::OnKeyDown(ITfContext *pContext, WPARAM wParam, LPARAM lParam, BOOL *pIsEaten)
{
    Global::UpdateModifiers(wParam, lParam);

    _KEYSTROKE_STATE KeystrokeState;
    WCHAR wch = '\0';
    UINT code = 0;

    *pIsEaten = _IsKeyEaten( //
        pContext,            //
        (UINT)wParam,        //
        &code,               //
        &wch,                //
        &KeystrokeState      //
    );

    Global::firefox_like_cnt = 0;

    /* Send key event to server process */
    if (*pIsEaten)
    {
        // 检查是否应该跳过发送此键到服务器，由于长度限制。
        // 当达到限制时，我们只阻止字符输入键（FUNCTION_INPUT）。
        // 这允许功能键如 Backspace、Space、Enter 仍然工作。
        if (KeystrokeState.Function == FUNCTION_INPUT &&
            _pCompositionProcessorEngine->GetVirtualKeyLength() >= MAX_PINYIN_LENGTH)
        {
            // 这个键仍然被吃掉（以防止它到达应用程序），
            // 但我们不把它发送到 server 端，也不进一步处理它。
            return S_OK;
        }

        Global::Keycode = code;
        Global::wch = wch;
        if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0)
            Global::ModifiersDown |= 0b00000001;
        else
            Global::ModifiersDown &= ~0b00000001;
        WriteDataToSharedMemory(Global::Keycode, wch, Global::ModifiersDown, nullptr, 0, L"", 0b000111);
        SendKeyEventToUIProcess();
        ClearNamedpipeDataIfExists();
    }

    if (*pIsEaten)
    {
        bool needInvokeKeyHandler = true;
        /* Invoke key handler edit session */
        if (code == VK_ESCAPE)
        {
            KeystrokeState.Category = CATEGORY_COMPOSING;
        }

        /* Always eat THIRDPARTY_NEXTPAGE and THIRDPARTY_PREVPAGE
        keys, but don't always process them. */
        if ((wch == THIRDPARTY_NEXTPAGE) || (wch == THIRDPARTY_PREVPAGE))
        {
            needInvokeKeyHandler = !((KeystrokeState.Category == CATEGORY_NONE) && //
                                     (KeystrokeState.Function == FUNCTION_NONE));
        }
        if (needInvokeKeyHandler)
        {
            _InvokeKeyHandler(pContext, code, wch, (DWORD)lParam, KeystrokeState);
        }
    }
    else if (KeystrokeState.Category == CATEGORY_INVOKE_COMPOSITION_EDIT_SESSION)
    {
        // Invoke key handler edit session
        KeystrokeState.Category = CATEGORY_COMPOSING;
        _InvokeKeyHandler(pContext, code, wch, (DWORD)lParam, KeystrokeState);
    }

    return S_OK;
}

//+---------------------------------------------------------------------------
//
// ITfKeyEventSink::OnTestKeyUp
//
// Called by the system to query this service wants a potential keystroke.
//----------------------------------------------------------------------------

STDAPI CMetasequoiaIME::OnTestKeyUp(ITfContext *pContext, WPARAM wParam, LPARAM lParam, BOOL *pIsEaten)
{
    if (pIsEaten == nullptr)
    {
        return E_INVALIDARG;
    }

    Global::UpdateModifiers(wParam, lParam);

    WCHAR wch = '\0';
    UINT code = 0;

    *pIsEaten = _IsKeyEaten(pContext, (UINT)wParam, &code, &wch, NULL);

    return S_OK;
}

//+---------------------------------------------------------------------------
//
// ITfKeyEventSink::OnKeyUp
//
// Called by the system to offer this service a keystroke.  If *pIsEaten == TRUE
// on exit, the application will not handle the keystroke.
//----------------------------------------------------------------------------

STDAPI CMetasequoiaIME::OnKeyUp(ITfContext *pContext, WPARAM wParam, LPARAM lParam, BOOL *pIsEaten)
{
    Global::UpdateModifiers(wParam, lParam);

    _KEYSTROKE_STATE KeystrokeState;
    WCHAR wch = '\0';
    UINT code = 0;
    *pIsEaten = _IsKeyEaten(pContext, (UINT)wParam, &code, &wch, &KeystrokeState);

#ifdef FANY_DEBUG
    OutputDebugString(L"fanyfull ITfKeyEventSink::OnKeyUp");
    OutputDebugString(fmt::format(L"Global::PureShiftKeyUp: {}", Global::PureShiftKeyUp).c_str());
#endif

    // if (code == VK_SHIFT)
    // {
    //     if (Global::PureShiftKeyUp)
    //     {
    //         KeystrokeState.Category = CATEGORY_COMPOSING;
    //         KeystrokeState.Function = FUNCTION_TOGGLE_IME_MODE;
    //         _InvokeKeyHandler(pContext, code, wch, (DWORD)lParam, KeystrokeState);
    //     }
    // }

    return S_OK;
}

//+---------------------------------------------------------------------------
//
// ITfKeyEventSink::OnPreservedKey
//
// Called when a hotkey (registered by us, or by the system) is typed.
//----------------------------------------------------------------------------

STDAPI CMetasequoiaIME::OnPreservedKey(ITfContext *pContext, REFGUID rguid, BOOL *pIsEaten)
{
    pContext;

    CCompositionProcessorEngine *pCompositionProcessorEngine;
    pCompositionProcessorEngine = _pCompositionProcessorEngine;

    BOOL pNeedToggleIMEMode = FALSE;

    pCompositionProcessorEngine->OnPreservedKey( //
        pContext,                                //
        rguid,                                   //
        pIsEaten,                                //
        _GetThreadMgr(),                         //
        _GetClientId(),                          //
        &pNeedToggleIMEMode                      //
    );

    if (pNeedToggleIMEMode)
    {
        _KEYSTROKE_STATE KeystrokeState;
        WCHAR wch = '\0';
        UINT code = 0;
        KeystrokeState.Category = CATEGORY_COMPOSING;
        KeystrokeState.Function = FUNCTION_TOGGLE_IME_MODE;
        _InvokeKeyHandler(pContext, code, wch, (DWORD)0, KeystrokeState);
    }

    return S_OK;
}

//+---------------------------------------------------------------------------
//
// _InitKeyEventSink
//
// Advise a keystroke sink.
//----------------------------------------------------------------------------

BOOL CMetasequoiaIME::_InitKeyEventSink()
{
    ITfKeystrokeMgr *pKeystrokeMgr = nullptr;
    HRESULT hr = S_OK;

    if (FAILED(_pThreadMgr->QueryInterface(IID_ITfKeystrokeMgr, (void **)&pKeystrokeMgr)))
    {
        return FALSE;
    }

    hr = pKeystrokeMgr->AdviseKeyEventSink(_tfClientId, (ITfKeyEventSink *)this, TRUE);

    pKeystrokeMgr->Release();

    return (hr == S_OK);
}

//+---------------------------------------------------------------------------
//
// _UninitKeyEventSink
//
// Unadvise a keystroke sink.  Assumes we have advised one already.
//----------------------------------------------------------------------------

void CMetasequoiaIME::_UninitKeyEventSink()
{
    ITfKeystrokeMgr *pKeystrokeMgr = nullptr;

    if (FAILED(_pThreadMgr->QueryInterface(IID_ITfKeystrokeMgr, (void **)&pKeystrokeMgr)))
    {
        return;
    }

    pKeystrokeMgr->UnadviseKeyEventSink(_tfClientId);

    pKeystrokeMgr->Release();
}
