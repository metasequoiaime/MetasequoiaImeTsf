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
#include <cwctype>
#include <string>
#include "Ipc.h"
#include "FanyUtils.h"
#include "FanyLog.h"
#include "../Utils/PerfTimer.h"

// 0xF003, 0xF004 are the keys that the touch keyboard sends for next/previous
#define THIRDPARTY_NEXTPAGE static_cast<WORD>(0xF003)
#define THIRDPARTY_PREVPAGE static_cast<WORD>(0xF004)

namespace
{
// A recovery checkpoint may need one INPUT for every raw pinyin character and
// one MOVE_LEFT for every character to the right of the caret.  Keep enough
// additional room for a short burst that arrives while the replacement IPC
// epoch is becoming ready.
constexpr size_t MAX_DEFERRED_KEY_DOWN_COUNT =
    static_cast<size_t>(MAX_PINYIN_LENGTH) * 2 + 32;

struct DeferredShadowState
{
    bool imeOpen = false;
    bool punctuationOpen = false;
    bool doubleSingleByteOpen = false;
    size_t inputLength = 0;
    bool candidateActive = false;
    bool unicodeMode = false;
};

bool IsBareModifierKey(UINT code)
{
    switch (code)
    {
    case VK_SHIFT:
    case VK_LSHIFT:
    case VK_RSHIFT:
    case VK_CONTROL:
    case VK_LCONTROL:
    case VK_RCONTROL:
    case VK_MENU:
    case VK_LMENU:
    case VK_RMENU:
    case VK_LWIN:
    case VK_RWIN:
        return true;
    default:
        return false;
    }
}

void ApplyDeferredKeyState(DeferredShadowState &shadow,
                           const _KEYSTROKE_STATE &keyState, WCHAR wch = 0)
{
    switch (keyState.Function)
    {
    case FUNCTION_INPUT:
        if (shadow.inputLength == 0)
        {
            shadow.unicodeMode = (wch == L'U');
        }
        if (shadow.inputLength < MAX_PINYIN_LENGTH)
        {
            ++shadow.inputLength;
        }
        shadow.candidateActive = false;
        break;
    case FUNCTION_FINALIZE_TEXTSTORE_AND_INPUT:
    case FUNCTION_FINALIZE_CANDIDATELIST_AND_INPUT:
        shadow.inputLength = 1;
        shadow.candidateActive = false;
        shadow.unicodeMode = (wch == L'U');
        break;
    case FUNCTION_BACKSPACE:
        if (shadow.inputLength > 0)
        {
            --shadow.inputLength;
        }
        if (shadow.inputLength == 0)
        {
            shadow.candidateActive = false;
            shadow.unicodeMode = false;
        }
        break;
    case FUNCTION_CONVERT_WILDCARD:
        shadow.candidateActive = shadow.inputLength > 0;
        break;
    case FUNCTION_CONVERT:
        // This TIP routes Space+Convert to WM_AsyncFinalizeCandidate, which
        // commits and ends the composition rather than merely opening a list.
        shadow.inputLength = 0;
        shadow.candidateActive = false;
        shadow.unicodeMode = false;
        break;
    case FUNCTION_CANCEL:
        shadow.inputLength = 0;
        shadow.candidateActive = false;
        shadow.unicodeMode = false;
        break;
    case FUNCTION_FINALIZE_TEXTSTORE:
    case FUNCTION_FINALIZE_CANDIDATELIST:
    case FUNCTION_FINALIZE_CANDIDATELISTForVKReturn:
    case FUNCTION_SELECT_BY_NUMBER:
    case FUNCTION_TOGGLE_IME_MODE:
    case FUNCTION_PUNCTUATION:
    case FUNCTION_DOUBLE_SINGLE_BYTE:
        shadow.inputLength = 0;
        shadow.candidateActive = false;
        shadow.unicodeMode = false;
        break;
    default:
        break;
    }
}

bool IsRecoverableDeferredPrefix(const _KEYSTROKE_STATE &keyState)
{
    switch (keyState.Function)
    {
    case FUNCTION_INPUT:
    case FUNCTION_BACKSPACE:
    case FUNCTION_MOVE_LEFT:
    case FUNCTION_MOVE_RIGHT:
    case FUNCTION_MOVE_UP:
    case FUNCTION_MOVE_DOWN:
    case FUNCTION_MOVE_PAGE_UP:
    case FUNCTION_MOVE_PAGE_DOWN:
    case FUNCTION_MOVE_PAGE_TOP:
    case FUNCTION_MOVE_PAGE_BOTTOM:
    case FUNCTION_CONVERT_WILDCARD:
    case FUNCTION_SERVER_CANDIDATE_KEY:
        return true;
    default:
        return false;
    }
}

bool StartsNewDeferredPrefix(const _KEYSTROKE_STATE &keyState)
{
    return keyState.Function == FUNCTION_FINALIZE_TEXTSTORE_AND_INPUT ||
           keyState.Function == FUNCTION_FINALIZE_CANDIDATELIST_AND_INPUT;
}

UINT CaptureIpcModifiers()
{
    UINT modifiers = 0;
    if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0)
        modifiers |= 0b00000001;
    if ((GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0)
        modifiers |= 0b00000010;
    if ((GetAsyncKeyState(VK_MENU) & 0x8000) != 0)
        modifiers |= 0b00000100;
    return modifiers;
}

void PostOwnerMessageWithSyncFallback(HWND window, UINT message,
                                      WPARAM wParam = 0, LPARAM lParam = 0)
{
    if (!window || !IsWindow(window))
    {
        return;
    }
    if (!PostMessage(window, message, wParam, lParam) &&
        GetWindowThreadProcessId(window, nullptr) == GetCurrentThreadId())
    {
        SendMessage(window, message, wParam, lParam);
    }
}
} // namespace

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
    _Out_opt_ _KEYSTROKE_STATE *pKeyState, //
    _In_opt_ const WCHAR *translatedWch,   //
    bool freshCompositionState             //
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
    WCHAR wch = translatedWch ? *translatedWch : ConvertVKey(codeIn);
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
        const bool isComposing = freshCompositionState ? false : _IsComposing() != FALSE;
        const CANDIDATE_MODE candidateMode =
            freshCompositionState ? CANDIDATE_NONE : _candidateMode;
        const bool isCapsLockOn = (GetKeyState(VK_CAPITAL) & 0x0001) != 0;
        const bool isUppercaseAlphabet = (wch >= L'A' && wch <= L'Z') && (*pCodeOut >= L'A' && *pCodeOut <= L'Z');
        const bool isInputInProgress =
            !freshCompositionState &&
            (isComposing || (candidateMode != CANDIDATE_NONE) ||
             (pCompositionProcessorEngine && pCompositionProcessorEngine->GetVirtualKeyLength() > 0));

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
        const BOOL ret = freshCompositionState
                             ? pCompositionProcessorEngine->IsVirtualKeyNeedForFreshComposition(
                                   *pCodeOut, pwch, pKeyState)
                             : pCompositionProcessorEngine->IsVirtualKeyNeed(
                                   *pCodeOut, pwch, isComposing, candidateMode,
                                   _isCandidateWithWildcard, pKeyState);
        if (ret)
        {
            return TRUE;
        }
    }

    //
    // Punctuation
    //
    if (pCompositionProcessorEngine->IsPunctuation(wch))
    {
        const CANDIDATE_MODE candidateMode =
            freshCompositionState ? CANDIDATE_NONE : _candidateMode;
        if ((candidateMode == CANDIDATE_NONE || candidateMode == CANDIDATE_INCREMENTAL) && isPunctuation)
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
        if ((freshCompositionState ? CANDIDATE_NONE : _candidateMode) == CANDIDATE_NONE)
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

bool CMetasequoiaIME::_HasDeferredKeyBarrier() const
{
    if (_localSessionResetPending.load(std::memory_order_acquire) ||
        _focusResetPending || _activationRequired ||
        _deferredKeyProjectionValid || !_deferredKeyDowns.empty() ||
        _hasDeferredKeyInFlight)
    {
        return true;
    }
    if (Global::g_connected)
    {
        const uint64_t expectedToken =
            _expectedWorkerFocusToken.load(std::memory_order_acquire);
        return expectedToken == 0 ||
               _acknowledgedWorkerFocusToken.load(std::memory_order_acquire) !=
                   expectedToken ||
               !_workerCommitReady.load(std::memory_order_acquire);
    }
    return false;
}

bool CMetasequoiaIME::_DeferredKeyQueueHasCapacity() const
{
    size_t deferredCount =
        _deferredKeyDowns.size() + _deferredAppliedPrefix.size();
    if (_hasDeferredKeyInFlight)
    {
        ++deferredCount;
    }
    return deferredCount < MAX_DEFERRED_KEY_DOWN_COUNT;
}

void CMetasequoiaIME::_EnsureDeferredKeyProjection()
{
    if (_deferredKeyProjectionValid)
    {
        return;
    }
    _deferredKeyProjectionValid = true;
    _deferredProjectedImeOpen = _pCompositionProcessorEngine && _pThreadMgr &&
                               _pCompositionProcessorEngine->GetIMEMode(
                                   _pThreadMgr, _tfClientId) != FALSE;
    _deferredProjectedPunctuationOpen =
        _pCompositionProcessorEngine && _pThreadMgr &&
        _pCompositionProcessorEngine->GetPunctuationMode(
            _pThreadMgr, _tfClientId) != FALSE;
    _deferredProjectedDoubleSingleByteOpen =
        _pCompositionProcessorEngine && _pThreadMgr &&
        _pCompositionProcessorEngine->GetDoubleSingleByteMode(
            _pThreadMgr, _tfClientId) != FALSE;
    // In the healthy path every IME-owned key enters the FIFO as well, so its
    // first projection starts from the composition that is already visible.
    // A transport-recovery checkpoint arms this projection before the local
    // reset cancels that composition.
    _deferredProjectedInputLength =
        _pCompositionProcessorEngine
            ? min(static_cast<size_t>(MAX_PINYIN_LENGTH),
                  static_cast<size_t>(
                      _pCompositionProcessorEngine->GetVirtualKeyLength()))
            : 0;
    // Incremental candidates are still the ordinary composing path: another
    // letter extends the same raw input.  Only an explicit/original candidate
    // list makes the next input a finalize-and-start-new boundary.
    _deferredProjectedCandidateActive = _candidateMode == CANDIDATE_ORIGINAL;
    _deferredProjectedUnicodeMode =
        _pCompositionProcessorEngine &&
        _pCompositionProcessorEngine->IsUnicodeModeComposition() != FALSE;
}

void CMetasequoiaIME::_ApplyDeferredKeyProjection(
    const _KEYSTROKE_STATE &keyState, WCHAR wch)
{
    _EnsureDeferredKeyProjection();
    DeferredShadowState shadow;
    shadow.imeOpen = _deferredProjectedImeOpen;
    shadow.punctuationOpen = _deferredProjectedPunctuationOpen;
    shadow.doubleSingleByteOpen = _deferredProjectedDoubleSingleByteOpen;
    shadow.inputLength = _deferredProjectedInputLength;
    shadow.candidateActive = _deferredProjectedCandidateActive;
    shadow.unicodeMode = _deferredProjectedUnicodeMode;
    ApplyDeferredKeyState(shadow, keyState, wch);
    _deferredProjectedInputLength = shadow.inputLength;
    _deferredProjectedCandidateActive = shadow.candidateActive;
    _deferredProjectedUnicodeMode = shadow.unicodeMode;
}

void CMetasequoiaIME::_ApplyDeferredPreservedKeyProjection(
    REFGUID preservedKey)
{
    _EnsureDeferredKeyProjection();
    if (_pCompositionProcessorEngine == nullptr)
    {
        return;
    }
    switch (_pCompositionProcessorEngine->GetPreservedKeyAction(preservedKey))
    {
    case CCompositionProcessorEngine::PreservedKeyAction::ToggleImeMode:
        _deferredProjectedImeOpen = !_deferredProjectedImeOpen;
        _deferredProjectedPunctuationOpen = _deferredProjectedImeOpen;
        _deferredProjectedInputLength = 0;
        _deferredProjectedCandidateActive = false;
        _deferredProjectedUnicodeMode = false;
        break;
    case CCompositionProcessorEngine::PreservedKeyAction::ToggleDoubleSingleByteMode:
        _deferredProjectedDoubleSingleByteOpen =
            !_deferredProjectedDoubleSingleByteOpen;
        break;
    case CCompositionProcessorEngine::PreservedKeyAction::TogglePunctuationMode:
        _deferredProjectedPunctuationOpen =
            !_deferredProjectedPunctuationOpen;
        break;
    default:
        break;
    }
}

bool CMetasequoiaIME::_RefreshDeferredRecoveryPrefix(
    _In_ ITfContext *pContext)
{
    while (!_deferredAppliedPrefix.empty())
    {
        ITfContext *prefixContext = _deferredAppliedPrefix.front().context;
        _deferredAppliedPrefix.pop_front();
        if (prefixContext)
        {
            prefixContext->Release();
        }
    }

    if (pContext == nullptr || _pCompositionProcessorEngine == nullptr)
    {
        return false;
    }

    CStringRange &raw = _pCompositionProcessorEngine->GetKeystrokeBuffer();
    const size_t rawLength = static_cast<size_t>(raw.GetLength());
    const size_t caret = min(
        rawLength,
        static_cast<size_t>(_pCompositionProcessorEngine->GetCaretPosition()));
    const size_t moveLeftCount = rawLength - caret;
    if (rawLength + moveLeftCount > MAX_DEFERRED_KEY_DOWN_COUNT)
    {
        return false;
    }

    for (size_t index = 0; index < rawLength; ++index)
    {
        const WCHAR wch = raw.Get()[index];
        const SHORT virtualKey = VkKeyScanW(wch);
        DeferredKeyDown recoveryKey;
        recoveryKey.kind = DeferredKeyDown::Kind::KeyDown;
        recoveryKey.context = pContext;
        recoveryKey.wParam = virtualKey == -1
                                 ? static_cast<WPARAM>(VK_PACKET)
                                 : static_cast<WPARAM>(LOBYTE(virtualKey));
        recoveryKey.translatedWch = wch;
        recoveryKey.modifiersDown =
            virtualKey != -1 && (HIBYTE(virtualKey) & 1) != 0 ? 1u : 0u;
        recoveryKey.keyState.Category = CATEGORY_COMPOSING;
        recoveryKey.keyState.Function = FUNCTION_INPUT;
        recoveryKey.focusGeneration = _deferredKeyFocusGeneration;
        pContext->AddRef();
        _deferredAppliedPrefix.push_back(recoveryKey);
    }
    for (size_t index = 0; index < moveLeftCount; ++index)
    {
        DeferredKeyDown recoveryKey;
        recoveryKey.kind = DeferredKeyDown::Kind::KeyDown;
        recoveryKey.context = pContext;
        recoveryKey.wParam = VK_LEFT;
        recoveryKey.keyState.Category = CATEGORY_COMPOSING;
        recoveryKey.keyState.Function = FUNCTION_MOVE_LEFT;
        recoveryKey.focusGeneration = _deferredKeyFocusGeneration;
        pContext->AddRef();
        _deferredAppliedPrefix.push_back(recoveryKey);
    }
    return true;
}

void CMetasequoiaIME::_ArmDeferredRecoveryForTransport(
    _In_opt_ ITfContext *pContext)
{
    // An in-flight key owns its exact retry token.  Its failure path moves the
    // checkpoint in front of that key; moving it here as well would duplicate
    // the prefix.
    if (_hasDeferredKeyInFlight)
    {
        return;
    }

    ITfContext *recoveryContext = pContext ? pContext : _pContext;
    const bool activeDeferredState =
        !_deferredKeyDowns.empty() || _deferredKeyProjectionValid;
    if (!activeDeferredState)
    {
        // The dormant checkpoint may have been superseded by a non-eaten
        // application key that finalized/cancelled the composition.  Refresh
        // from the engine at the transport boundary instead of trusting stale
        // history.
        if (recoveryContext)
        {
            (void)_RefreshDeferredRecoveryPrefix(recoveryContext);
        }
    }
    else if (_deferredAppliedPrefix.empty())
    {
        // A retry has already materialized the checkpoint in the FIFO.  The
        // real engine can still contain the same raw text until the queued
        // reset edit session runs, so snapshotting it again would duplicate
        // the whole prefix.
        return;
    }
    if (_deferredAppliedPrefix.empty())
    {
        return;
    }

    // Capture the current real state before the local reset cancels it.  The
    // queued checkpoint then represents that same future state while reset is
    // pending, so later key classification remains ordered.
    _EnsureDeferredKeyProjection();
    while (!_deferredAppliedPrefix.empty())
    {
        _deferredKeyDowns.push_front(_deferredAppliedPrefix.back());
        _deferredAppliedPrefix.pop_back();
    }
    _ScheduleDeferredKeyDownDrain();
}

bool CMetasequoiaIME::_ClassifyDeferredKeyDown(
    _In_ ITfContext *pContext, WPARAM wParam,
    _In_opt_ const WCHAR *translatedWch,
    _In_opt_ const UINT *modifiersDown,
    _Out_ WCHAR *classifiedWch, _Out_ UINT *classifiedCode,
    _Out_ _KEYSTROKE_STATE *keyState)
{
    if (pContext == nullptr || classifiedWch == nullptr ||
        classifiedCode == nullptr || keyState == nullptr ||
        _pCompositionProcessorEngine == nullptr || _pThreadMgr == nullptr)
    {
        return false;
    }

    *classifiedWch = translatedWch ? *translatedWch : ConvertVKey(static_cast<UINT>(wParam));
    *classifiedCode = VKeyFromVKPacketAndWchar(static_cast<UINT>(wParam), *classifiedWch);
    keyState->Category = CATEGORY_NONE;
    keyState->Function = FUNCTION_NONE;

    const UINT capturedModifiers = modifiersDown ? *modifiersDown : CaptureIpcModifiers();
    // Ctrl/Alt/Windows combinations belong to the application. In particular,
    // never turn a recovery FIFO into a shortcut sink.
    if ((capturedModifiers & 0b00000110) != 0 ||
        (GetAsyncKeyState(VK_LWIN) & 0x8000) != 0 ||
        (GetAsyncKeyState(VK_RWIN) & 0x8000) != 0 ||
        IsBareModifierKey(*classifiedCode) || _IsKeyboardDisabled())
    {
        return false;
    }

    DeferredShadowState shadow;
    if (_deferredKeyProjectionValid)
    {
        shadow.imeOpen = _deferredProjectedImeOpen;
        shadow.punctuationOpen = _deferredProjectedPunctuationOpen;
        shadow.doubleSingleByteOpen =
            _deferredProjectedDoubleSingleByteOpen;
        shadow.inputLength = _deferredProjectedInputLength;
        shadow.candidateActive = _deferredProjectedCandidateActive;
        shadow.unicodeMode = _deferredProjectedUnicodeMode;
    }
    else
    {
        shadow.imeOpen =
            _pCompositionProcessorEngine->GetIMEMode(_pThreadMgr, _tfClientId) != FALSE;
        shadow.punctuationOpen =
            _pCompositionProcessorEngine->GetPunctuationMode(_pThreadMgr, _tfClientId) != FALSE;
        shadow.doubleSingleByteOpen =
            _pCompositionProcessorEngine->GetDoubleSingleByteMode(
                _pThreadMgr, _tfClientId) != FALSE;
        shadow.inputLength = min(
            static_cast<size_t>(MAX_PINYIN_LENGTH),
            static_cast<size_t>(
                _pCompositionProcessorEngine->GetVirtualKeyLength()));
        shadow.candidateActive = _candidateMode == CANDIDATE_ORIGINAL;
        shadow.unicodeMode =
            _pCompositionProcessorEngine->IsUnicodeModeComposition() != FALSE;
    }

    const auto setKeyState = [keyState](KEYSTROKE_CATEGORY category,
                                        KEYSTROKE_FUNCTION function) {
        keyState->Category = category;
        keyState->Function = function;
        return true;
    };

    _KEYSTROKE_STATE inputState = {};
    WCHAR inputWch = *classifiedWch;
    bool isInputKey = false;
    if (shadow.imeOpen)
    {
        isInputKey =
            _pCompositionProcessorEngine->IsVirtualKeyNeedForFreshComposition(
                *classifiedCode, &inputWch, &inputState) != FALSE;
        if (!isInputKey && shadow.inputLength > 0 &&
            ((*classifiedWch == L'\'') ||
             (_pCompositionProcessorEngine->IsWildcard() &&
              _pCompositionProcessorEngine->IsWildcardChar(*classifiedWch))))
        {
            isInputKey = true;
        }
        if (shadow.inputLength == 0 &&
            (GetKeyState(VK_CAPITAL) & 0x0001) != 0 &&
            *classifiedWch >= L'A' && *classifiedWch <= L'Z' &&
            *classifiedCode >= L'A' && *classifiedCode <= L'Z')
        {
            // Match the normal fresh-composition path: CapsLock uppercase at
            // the beginning belongs to the application.
            isInputKey = false;
        }
    }

    if (shadow.candidateActive && isInputKey)
    {
        return setKeyState(CATEGORY_CANDIDATE,
                           FUNCTION_FINALIZE_CANDIDATELIST_AND_INPUT);
    }
    if (!shadow.candidateActive && isInputKey)
    {
        return setKeyState(CATEGORY_COMPOSING, FUNCTION_INPUT);
    }

    if (shadow.imeOpen && shadow.inputLength > 0)
    {
        const bool candidateKey = shadow.candidateActive;
        switch (*classifiedCode)
        {
        case VK_BACK:
            return candidateKey
                       ? setKeyState(CATEGORY_CANDIDATE, FUNCTION_CANCEL)
                       : setKeyState(CATEGORY_COMPOSING, FUNCTION_BACKSPACE);
        case VK_SPACE:
            return setKeyState(CATEGORY_CANDIDATE, FUNCTION_CONVERT);
        case VK_RETURN:
            return candidateKey
                       ? setKeyState(CATEGORY_CANDIDATE,
                                     FUNCTION_FINALIZE_CANDIDATELIST)
                       : setKeyState(CATEGORY_CANDIDATE,
                                     FUNCTION_FINALIZE_CANDIDATELISTForVKReturn);
        case VK_ESCAPE:
            return setKeyState(CATEGORY_CANDIDATE, FUNCTION_CANCEL);
        case VK_LEFT:
            return setKeyState(CATEGORY_COMPOSING, FUNCTION_MOVE_LEFT);
        case VK_RIGHT:
            return setKeyState(CATEGORY_COMPOSING, FUNCTION_MOVE_RIGHT);
        case VK_HOME:
            return setKeyState(CATEGORY_CANDIDATE, FUNCTION_MOVE_PAGE_TOP);
        case VK_END:
            return setKeyState(CATEGORY_CANDIDATE, FUNCTION_MOVE_PAGE_BOTTOM);
        case VK_OEM_MINUS:
        case VK_OEM_PLUS:
        case VK_OEM_COMMA:
        case VK_OEM_PERIOD:
        case VK_TAB:
        case VK_PRIOR:
        case VK_NEXT:
        case VK_UP:
        case VK_DOWN:
            if (*classifiedCode == VK_OEM_PLUS && shadow.unicodeMode &&
                shadow.inputLength == 1 && *classifiedWch == L'+')
            {
                return setKeyState(CATEGORY_COMPOSING, FUNCTION_INPUT);
            }
            return setKeyState(CATEGORY_CANDIDATE,
                               FUNCTION_SERVER_CANDIDATE_KEY);
        default:
            break;
        }

        if (*classifiedCode >= L'1' && *classifiedCode <= L'9')
        {
            // U-mode: bare digits compose hex; Shift+1..9 selects candidates.
            if (shadow.unicodeMode)
            {
                const bool shift_only =
                    (capturedModifiers & 0b00000111u) == 0b00000001u;
                if (shift_only)
                {
                    return setKeyState(CATEGORY_CANDIDATE,
                                       FUNCTION_SELECT_BY_NUMBER);
                }
                return setKeyState(CATEGORY_COMPOSING, FUNCTION_INPUT);
            }
            return setKeyState(CATEGORY_CANDIDATE, FUNCTION_SELECT_BY_NUMBER);
        }
        if (*classifiedCode == L'0' && shadow.unicodeMode)
        {
            return setKeyState(CATEGORY_COMPOSING, FUNCTION_INPUT);
        }
        if (shadow.punctuationOpen &&
            _pCompositionProcessorEngine->IsPunctuation(*classifiedWch))
        {
            return setKeyState(CATEGORY_COMPOSING, FUNCTION_PUNCTUATION);
        }
        return false;
    }

    if (shadow.punctuationOpen &&
        _pCompositionProcessorEngine->IsPunctuation(*classifiedWch))
    {
        return setKeyState(CATEGORY_COMPOSING, FUNCTION_PUNCTUATION);
    }
    if (shadow.doubleSingleByteOpen &&
        _pCompositionProcessorEngine->IsDoubleSingleByte(*classifiedWch))
    {
        return setKeyState(CATEGORY_COMPOSING, FUNCTION_DOUBLE_SINGLE_BYTE);
    }
    if (!shadow.imeOpen && *classifiedWch != L'\0' &&
        std::iswprint(static_cast<wint_t>(*classifiedWch)) != 0)
    {
        // A queued Shift may make this future key English. It still belongs
        // behind the FIFO prefix; CATEGORY_NONE/FUNCTION_NONE marks a direct
        // application-text replay instead of misclassifying it as Chinese.
        return true;
    }
    return false;
}

//+---------------------------------------------------------------------------
//
// ITfKeyEventSink::OnTestKeyDown
//
// Called by the system to query this service wants a potential keystroke.
//----------------------------------------------------------------------------

STDAPI CMetasequoiaIME::OnTestKeyDown(ITfContext *pContext, WPARAM wParam, LPARAM lParam, BOOL *pIsEaten)
{
    if (pContext == nullptr || pIsEaten == nullptr)
    {
        return E_INVALIDARG;
    }
    PerfTimer onTestKeyDownTimer;
    Global::UpdateModifiers(wParam, lParam);

    if (_HasDeferredKeyBarrier())
    {
        if (!_DeferredKeyQueueHasCapacity())
        {
            *pIsEaten = FALSE;
            return S_OK;
        }
        _KEYSTROKE_STATE deferredState = {};
        WCHAR deferredWch = L'\0';
        UINT deferredCode = 0;
        *pIsEaten = _ClassifyDeferredKeyDown(
                        pContext, wParam, nullptr, nullptr, &deferredWch,
                        &deferredCode, &deferredState)
                        ? TRUE
                        : FALSE;
        return S_OK;
    }

    _KEYSTROKE_STATE KeystrokeState;
    WCHAR wch = '\0';
    UINT code = 0;
    *pIsEaten = _IsKeyEaten(pContext, (UINT)wParam, &code, &wch,
                            &KeystrokeState);

    if (KeystrokeState.Category == CATEGORY_INVOKE_COMPOSITION_EDIT_SESSION)
    {
        //
        // Invoke key handler edit session
        //
        KeystrokeState.Category = CATEGORY_COMPOSING;

        _InvokeKeyHandler(pContext, code, wch, (DWORD)lParam, KeystrokeState,
                          FANY_IME_NO_REQUEST_ID);
    }


    return S_OK;
}

bool CMetasequoiaIME::_QueueDeferredKeyDown(_In_ ITfContext *pContext, WPARAM wParam,
                                             LPARAM lParam, WCHAR translatedWch,
                                             UINT modifiersDown,
                                             const _KEYSTROKE_STATE &keyState)
{
    if (pContext == nullptr || !_DeferredKeyQueueHasCapacity())
    {
        return false;
    }

    pContext->AddRef();
    DeferredKeyDown key;
    key.kind = keyState.Category == CATEGORY_NONE &&
                       keyState.Function == FUNCTION_NONE
                   ? DeferredKeyDown::Kind::ApplicationText
                   : DeferredKeyDown::Kind::KeyDown;
    key.context = pContext;
    key.wParam = wParam;
    key.lParam = lParam;
    key.translatedWch = translatedWch;
    key.modifiersDown = modifiersDown;
    key.keyState = keyState;
    key.focusGeneration = _deferredKeyFocusGeneration;
    _deferredKeyDowns.push_back(key);
    if (key.kind == DeferredKeyDown::Kind::KeyDown)
    {
        _ApplyDeferredKeyProjection(keyState, translatedWch);
    }
    else
    {
        _EnsureDeferredKeyProjection();
    }
    _ScheduleDeferredKeyDownDrain();
    return true;
}

bool CMetasequoiaIME::_QueueDeferredPreservedKey(_In_ ITfContext *pContext,
                                                  REFGUID preservedKey)
{
    if (pContext == nullptr || !_DeferredKeyQueueHasCapacity())
    {
        return false;
    }

    pContext->AddRef();
    DeferredKeyDown key;
    key.kind = DeferredKeyDown::Kind::PreservedKey;
    key.context = pContext;
    key.preservedKey = preservedKey;
    key.focusGeneration = _deferredKeyFocusGeneration;
    _deferredKeyDowns.push_back(key);
    _ApplyDeferredPreservedKeyProjection(preservedKey);
    _ScheduleDeferredKeyDownDrain();
    return true;
}

void CMetasequoiaIME::_ClearDeferredKeyDowns()
{
    // A posted drain belongs to the current message window/generation.  It
    // may never run if Deactivate destroys that window, so never carry this
    // latch into a later Activate on the same TIP instance.
    _deferredKeyDrainPosted = false;
    if (++_deferredKeyFocusGeneration == 0)
    {
        ++_deferredKeyFocusGeneration;
    }
    while (!_deferredKeyDowns.empty())
    {
        ITfContext *context = _deferredKeyDowns.front().context;
        _deferredKeyDowns.pop_front();
        if (context)
        {
            context->Release();
        }
    }
    while (!_deferredAppliedPrefix.empty())
    {
        ITfContext *context = _deferredAppliedPrefix.front().context;
        _deferredAppliedPrefix.pop_front();
        if (context)
        {
            context->Release();
        }
    }
    if (_hasDeferredKeyInFlight)
    {
        ITfContext *context = _deferredKeyInFlight.context;
        _hasDeferredKeyInFlight = false;
        _deferredKeyReplayToken = 0;
        _deferredKeyInFlight = {};
        if (context)
        {
            context->Release();
        }
    }
    _deferredKeyProjectionValid = false;
    _deferredProjectedImeOpen = false;
    _deferredProjectedPunctuationOpen = false;
    _deferredProjectedDoubleSingleByteOpen = false;
    _deferredProjectedInputLength = 0;
    _deferredProjectedCandidateActive = false;
    _deferredProjectedUnicodeMode = false;
}

void CMetasequoiaIME::_CompleteDeferredKeyReplay(uint64_t replayToken)
{
    if (replayToken == 0 || !_hasDeferredKeyInFlight ||
        _deferredKeyReplayToken != replayToken)
    {
        return;
    }

    ITfContext *context = _deferredKeyInFlight.context;
    if (_deferredKeyDowns.empty())
    {
        // The exact final edit session has completed, so the real composition
        // has caught up with the future projection.  Compact the applied
        // event history into a bounded raw-text/caret checkpoint.  This
        // checkpoint is dormant (not a barrier) until a transport reset needs
        // to rebuild the composition.
        _deferredKeyProjectionValid = false;
        _deferredProjectedInputLength = 0;
        _deferredProjectedCandidateActive = false;
        _deferredProjectedUnicodeMode = false;
        (void)_RefreshDeferredRecoveryPrefix(context);
        _deferredKeyInFlight = {};
        _hasDeferredKeyInFlight = false;
        _deferredKeyReplayToken = 0;
        if (context)
        {
            context->Release();
        }
        _TryLeaveServerUnavailableFallback();
        _ScheduleDeferredKeyDownDrain();
        return;
    }

    bool contextTransferredToPrefix = false;
    const auto clearAppliedPrefix = [this]() {
        while (!_deferredAppliedPrefix.empty())
        {
            ITfContext *prefixContext =
                _deferredAppliedPrefix.front().context;
            _deferredAppliedPrefix.pop_front();
            if (prefixContext)
            {
                prefixContext->Release();
            }
        }
    };
    if (_deferredKeyInFlight.kind == DeferredKeyDown::Kind::KeyDown)
    {
        if (StartsNewDeferredPrefix(_deferredKeyInFlight.keyState))
        {
            clearAppliedPrefix();
            // The old text/candidate side of this combined operation has
            // already committed successfully.  A replacement Server epoch
            // must rebuild only the new raw character; replaying the original
            // finalize-and-input function would try to finalize state that no
            // longer exists.
            _deferredKeyInFlight.keyState.Category = CATEGORY_COMPOSING;
            _deferredKeyInFlight.keyState.Function = FUNCTION_INPUT;
            _deferredAppliedPrefix.push_back(_deferredKeyInFlight);
            contextTransferredToPrefix = true;
        }
        else if (IsRecoverableDeferredPrefix(_deferredKeyInFlight.keyState))
        {
            _deferredAppliedPrefix.push_back(_deferredKeyInFlight);
            contextTransferredToPrefix = true;
        }
        else
        {
            clearAppliedPrefix();
        }
    }
    else if (_deferredKeyInFlight.kind ==
                 DeferredKeyDown::Kind::PreservedKey &&
             _pCompositionProcessorEngine &&
             _pCompositionProcessorEngine->GetPreservedKeyAction(
                 _deferredKeyInFlight.preservedKey) ==
                 CCompositionProcessorEngine::PreservedKeyAction::ToggleImeMode)
    {
        clearAppliedPrefix();
    }

    _deferredKeyInFlight = {};
    _hasDeferredKeyInFlight = false;
    _deferredKeyReplayToken = 0;
    if (context && !contextTransferredToPrefix)
    {
        context->Release();
    }
    _TryLeaveServerUnavailableFallback();
    _ScheduleDeferredKeyDownDrain();
}

bool CMetasequoiaIME::_IsDeferredKeyReplayCurrent(
    uint64_t replayToken, uint64_t focusGeneration,
    _In_opt_ ITfContext *expectedContext) const
{
    return replayToken != 0 && _hasDeferredKeyInFlight &&
           _deferredKeyReplayToken == replayToken &&
           focusGeneration == _deferredKeyFocusGeneration &&
           _deferredKeyInFlight.focusGeneration == focusGeneration &&
           (expectedContext == nullptr ||
            _deferredKeyInFlight.context == expectedContext);
}

void CMetasequoiaIME::_RetryDeferredKeyReplay(uint64_t replayToken)
{
    if (replayToken == 0 || !_hasDeferredKeyInFlight ||
        _deferredKeyReplayToken != replayToken)
    {
        return;
    }
    if (_deferredKeyInFlight.focusGeneration != _deferredKeyFocusGeneration)
    {
        _CompleteDeferredKeyReplay(replayToken);
        return;
    }

    const bool needsBackoff = _deferredKeyInFlight.replayAttempts >= 2;
    _deferredKeyDowns.push_front(_deferredKeyInFlight);
    while (!_deferredAppliedPrefix.empty())
    {
        _deferredKeyDowns.push_front(_deferredAppliedPrefix.back());
        _deferredAppliedPrefix.pop_back();
    }
    _deferredKeyInFlight = {};
    _hasDeferredKeyInFlight = false;
    _deferredKeyReplayToken = 0;
    // Finish the ownership transfer before a dirty notification can fall
    // back to synchronous window dispatch and re-enter reset bookkeeping.
    MarkNamedpipeSessionDirtyForOwner(this);
    if (!needsBackoff)
    {
        _ScheduleDeferredKeyDownDrain();
    }
    else if (_msgWndHandle && IsWindow(_msgWndHandle))
    {
        PostOwnerMessageWithSyncFallback(_msgWndHandle, WM_IpcReconnect);
    }
}

void CMetasequoiaIME::_ScheduleDeferredKeyDownDrain()
{
    if (!_deferredKeyDowns.empty() && !_hasDeferredKeyInFlight &&
        !_deferredKeyDrainPosted &&
        _msgWndHandle && IsWindow(_msgWndHandle))
    {
        _deferredKeyDrainPosted = true;
        if (!PostMessage(_msgWndHandle, WM_DrainDeferredKeyDown, 0, 0))
        {
            // The owner window is thread-affine. A synchronous fallback keeps
            // a transient queue-post failure from stranding an eaten key.
            SendMessage(_msgWndHandle, WM_DrainDeferredKeyDown, 0, 0);
        }
    }
}

bool CMetasequoiaIME::_IsServerUnavailableFallbackActive() const
{
    return _serverUnavailableFallbackActive;
}

void CMetasequoiaIME::_TryLeaveServerUnavailableFallback()
{
    const uint64_t expectedToken =
        _expectedWorkerFocusToken.load(std::memory_order_acquire);
    if (_serverUnavailableFallbackActive && !_IsComposing() &&
        expectedToken != 0 &&
        _workerCommitReady.load(std::memory_order_acquire) &&
        _acknowledgedWorkerFocusToken.load(std::memory_order_acquire) ==
            expectedToken)
    {
        _serverUnavailableFallbackActive = false;
    }
}

void CMetasequoiaIME::_DrainOneDeferredKeyDown()
{
    _deferredKeyDrainPosted = false;
    if (_hasDeferredKeyInFlight || _deferredKeyDowns.empty() ||
        !Global::g_connected)
    {
        return;
    }
    if (_localSessionResetPending.load(std::memory_order_acquire))
    {
        PostOwnerMessageWithSyncFallback(_msgWndHandle, WM_IpcReconnect);
        return;
    }
    if (!_serverUnavailableFallbackActive &&
        !EnsureNamedpipeFocusSessionActivated())
    {
        // Do not strand an eaten key when the Server cannot establish a
        // focus session. The queued FIFO becomes an isolated local/raw-input
        // lane until its composition has been finalized.
        _serverUnavailableFallbackActive = true;
    }

    _deferredKeyInFlight = _deferredKeyDowns.front();
    _deferredKeyDowns.pop_front();
    _hasDeferredKeyInFlight = true;
    do
    {
        _deferredKeyReplayToken = ++_nextDeferredKeyReplayToken;
    } while (_deferredKeyReplayToken == 0);

    DeferredKeyDown &key = _deferredKeyInFlight;
    ++key.replayAttempts;
    const uint64_t replayToken = _deferredKeyReplayToken;
    const uint64_t focusToken = _CaptureFocusSessionToken();
    if (key.focusGeneration != _deferredKeyFocusGeneration)
    {
        // All queued keys belong to the focused top context that captured
        // them. Never replay into another editor after another focus change.
        _ClearDeferredKeyDowns();
        return;
    }
    if (!_serverUnavailableFallbackActive &&
        !_IsFocusSessionCurrent(focusToken, key.context))
    {
        // A token/Ready fence can close without a document focus change.
        // Preserve the exact item for the replacement Server epoch; an actual
        // focus-generation change is handled by _ClearDeferredKeyDowns above.
        _RetryDeferredKeyReplay(replayToken);
        return;
    }

    BOOL eaten = FALSE;
    if (key.kind == DeferredKeyDown::Kind::ApplicationText)
    {
        (void)_RequestDeferredApplicationTextEditSession(
            key.context, key.translatedWch, focusToken,
            key.focusGeneration, replayToken);
        return;
    }
    if (key.kind == DeferredKeyDown::Kind::PreservedKey)
    {
        const auto preservedAction = _pCompositionProcessorEngine
                                         ? _pCompositionProcessorEngine
                                               ->GetPreservedKeyAction(
                                                   key.preservedKey)
                                         : CCompositionProcessorEngine::
                                               PreservedKeyAction::None;
        const bool awaitsEditSession =
            preservedAction == CCompositionProcessorEngine::
                                   PreservedKeyAction::ToggleImeMode;
        if (!key.preservedApplied)
        {
            // OnPreservedKey changes compartments synchronously. Mark that
            // phase before entering COM so a failed async commit retries only
            // the commit phase and never toggles twice.
            key.preservedApplied = true;
            _DispatchPreservedKey(key.context, key.preservedKey, &eaten,
                                  key.focusGeneration, true, replayToken);
        }
        else if (awaitsEditSession)
        {
            _KEYSTROKE_STATE toggleState = {};
            toggleState.Category = CATEGORY_COMPOSING;
            toggleState.Function = FUNCTION_TOGGLE_IME_MODE;
            _InvokeKeyHandler(key.context, 0, L'\0', 0, toggleState,
                              FANY_IME_NO_REQUEST_ID, {}, 0, 0, 0,
                              replayToken);
        }
        if (!awaitsEditSession && _deferredKeyReplayToken == replayToken)
        {
            _CompleteDeferredKeyReplay(replayToken);
        }
        return;
    }

    if (_serverUnavailableFallbackActive)
    {
        _KEYSTROKE_STATE offlineState = key.keyState;
        offlineState.Category = CATEGORY_COMPOSING;
        switch (offlineState.Function)
        {
        case FUNCTION_CONVERT:
        case FUNCTION_FINALIZE_CANDIDATELIST:
        case FUNCTION_FINALIZE_CANDIDATELISTForVKReturn:
        case FUNCTION_SELECT_BY_NUMBER:
        case FUNCTION_SERVER_CANDIDATE_KEY:
            // There is no candidate authority offline. Commit exactly the
            // raw text already held by the TSF composition.
            offlineState.Function = FUNCTION_FINALIZE_TEXTSTORE;
            break;
        case FUNCTION_MOVE_PAGE_UP:
        case FUNCTION_MOVE_PAGE_DOWN:
        case FUNCTION_MOVE_PAGE_TOP:
        case FUNCTION_MOVE_PAGE_BOTTOM:
            _CompleteDeferredKeyReplay(replayToken);
            return;
        default:
            break;
        }
        _InvokeKeyHandler(key.context, key.wParam, key.translatedWch,
                          static_cast<DWORD>(key.lParam), offlineState,
                          FANY_IME_NO_REQUEST_ID, {}, 0, 0, 0, replayToken);
        return;
    }

    const KeyDownDispatchResult result =
        _DispatchKeyDown(key.context, key.wParam, key.lParam, &eaten,
                         &key.translatedWch, &key.modifiersDown, &key.keyState, false,
                         key.focusGeneration, replayToken);
    if (result == KeyDownDispatchResult::Retry)
    {
        _RetryDeferredKeyReplay(replayToken);
        return;
    }

    if (result == KeyDownDispatchResult::Complete &&
        _deferredKeyReplayToken == replayToken)
    {
        _CompleteDeferredKeyReplay(replayToken);
    }
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
    if (pContext == nullptr || pIsEaten == nullptr)
    {
        return E_INVALIDARG;
    }
    PerfTimer onKeyDownTimer;
    const uint64_t focusGeneration = _deferredKeyFocusGeneration;
    (void)_DispatchKeyDown(pContext, wParam, lParam, pIsEaten, nullptr, nullptr,
                           nullptr, true, focusGeneration);
    return S_OK;
}

CMetasequoiaIME::KeyDownDispatchResult CMetasequoiaIME::_DispatchKeyDown(
    _In_ ITfContext *pContext, WPARAM wParam, LPARAM lParam, _Out_ BOOL *pIsEaten,
    _In_opt_ const WCHAR *translatedWch, _In_opt_ const UINT *modifiersDown,
    _In_opt_ const _KEYSTROKE_STATE *prevalidatedKeyState,
    bool canDefer, uint64_t expectedFocusGeneration,
    uint64_t deferredReplayToken)
{
    if (pContext == nullptr || pIsEaten == nullptr)
    {
        return KeyDownDispatchResult::Complete;
    }

    if (translatedWch == nullptr)
    {
        Global::UpdateModifiers(wParam, lParam);
    }

    _KEYSTROKE_STATE KeystrokeState = {};
    WCHAR wch = '\0';
    UINT code = 0;
    uint64_t requestId = FANY_IME_NO_REQUEST_ID;
    const UINT capturedModifiers = modifiersDown ? *modifiersDown : CaptureIpcModifiers();

    if (canDefer && _HasDeferredKeyBarrier())
    {
        if (!_DeferredKeyQueueHasCapacity() ||
            !_ClassifyDeferredKeyDown(pContext, wParam, translatedWch,
                                      &capturedModifiers, &wch, &code,
                                      &KeystrokeState))
        {
            *pIsEaten = FALSE;
            return KeyDownDispatchResult::Complete;
        }
        if (expectedFocusGeneration == 0 ||
            expectedFocusGeneration != _deferredKeyFocusGeneration)
        {
            *pIsEaten = TRUE;
            return KeyDownDispatchResult::Complete;
        }
        *pIsEaten = _QueueDeferredKeyDown(
                        pContext, wParam, lParam, wch, capturedModifiers,
                        KeystrokeState)
                        ? TRUE
                        : FALSE;
        if (*pIsEaten &&
            _localSessionResetPending.load(std::memory_order_acquire))
        {
            const UINT resetToken =
                _localSessionResetToken.load(std::memory_order_acquire);
            _RequestLocalSessionReset(pContext, resetToken);
        }
        return KeyDownDispatchResult::Complete;
    }

    if (prevalidatedKeyState != nullptr)
    {
        KeystrokeState = *prevalidatedKeyState;
        wch = translatedWch ? *translatedWch : ConvertVKey(static_cast<UINT>(wParam));
        code = VKeyFromVKPacketAndWchar(static_cast<UINT>(wParam), wch);
        *pIsEaten = TRUE;
    }
    else
    {
        PerfTimer isKeyEatenTimer;
        *pIsEaten = _IsKeyEaten( //
            pContext,            //
            (UINT)wParam,        //
            &code,               //
            &wch,                //
            &KeystrokeState,     //
            translatedWch        //
        );
    }

    if (expectedFocusGeneration == 0 ||
        expectedFocusGeneration != _deferredKeyFocusGeneration)
    {
        // A COM callback inside key classification changed the focused
        // topology. The old key must not enter the replacement Server epoch.
        *pIsEaten = TRUE;
        return KeyDownDispatchResult::Complete;
    }

    const bool resetPending =
        _localSessionResetPending.load(std::memory_order_acquire);
    if (resetPending)
    {
        if (!canDefer)
        {
            return KeyDownDispatchResult::Retry;
        }

        // The reset gate may have closed concurrently with normal
        // classification. Reclassify against the FIFO's future state before
        // retaining the key.
        if (!_DeferredKeyQueueHasCapacity() ||
            !_ClassifyDeferredKeyDown(pContext, wParam, translatedWch,
                                      &capturedModifiers, &wch, &code,
                                      &KeystrokeState) ||
            !_QueueDeferredKeyDown(pContext, wParam, lParam, wch,
                                   capturedModifiers, KeystrokeState))
        {
            *pIsEaten = FALSE;
        }
        const UINT resetToken = _localSessionResetToken.load(std::memory_order_acquire);
        _RequestLocalSessionReset(pContext, resetToken);
        return KeyDownDispatchResult::Complete;
    }

    if (canDefer && *pIsEaten &&
        (KeystrokeState.Category != CATEGORY_NONE ||
         KeystrokeState.Function != FUNCTION_NONE))
    {
        // Give every IME-owned key a member-owned replay token before any IPC
        // write or asynchronous TSF edit session is started.  Consequently a
        // write success followed by a reply/edit failure follows the same
        // exact retry path as a key that arrived behind a reconnect barrier.
        *pIsEaten = _QueueDeferredKeyDown(
                        pContext, wParam, lParam, wch, capturedModifiers,
                        KeystrokeState)
                        ? TRUE
                        : FALSE;
        return KeyDownDispatchResult::Complete;
    }

    const bool isPunctuationKey = _pCompositionProcessorEngine &&
                                  _pCompositionProcessorEngine->IsPunctuation(wch);

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
            return KeyDownDispatchResult::Complete;
        }

        if (expectedFocusGeneration != _deferredKeyFocusGeneration)
        {
            return KeyDownDispatchResult::Complete;
        }

        Global::Keycode = code;
        Global::wch = wch;
        Global::ModifiersDown = capturedModifiers;

        PerfTimer writeShmTimer;
        WriteDataToSharedMemory(Global::Keycode, wch, Global::ModifiersDown, nullptr, 0, L"", 0b000111);

        PerfTimer sendKeyEventTimer;
        const KeyEventSendResult sendResult = SendKeyEventToUIProcess(&requestId);
        if (sendResult != KeyEventSendResult::Sent)
        {
            if (!canDefer)
            {
                return KeyDownDispatchResult::Retry;
            }
            const bool queued = _QueueDeferredKeyDown(
                pContext, wParam, lParam, wch, capturedModifiers,
                KeystrokeState);
            if (!queued)
            {
                // This path is defensive now that every normal eaten key is
                // tokenized before dispatch.  If it is ever reached, handing
                // the key back is preferable to silently dropping an
                // ambiguous delivery.
                *pIsEaten = FALSE;
            }
            // A failed write dirties the session and forces a new activation
            // token. If the old Server epoch did receive the ambiguous frame,
            // that epoch is rejected/cleared before this queued key is replayed.
            // Thus retrying after the exact FocusSessionReady fence cannot
            // apply the same key twice to one Server composition.
            return KeyDownDispatchResult::Complete;
        }

        if (KeystrokeState.Function == FUNCTION_SERVER_CANDIDATE_KEY && _msgWndHandle)
        {
            _PostAsyncKeyRequest(WM_AsyncServerCandidateKey, code, wch, requestId,
                                 {}, 0, 0, deferredReplayToken);
            return deferredReplayToken != 0
                       ? KeyDownDispatchResult::AwaitingCompletion
                       : KeyDownDispatchResult::Complete;
        }

        if (code == VK_SPACE && KeystrokeState.Function == FUNCTION_CONVERT)
        {
            if (_msgWndHandle)
            {
                _PostAsyncKeyRequest(WM_AsyncFinalizeCandidate, code, wch, requestId,
                                     {}, 0, 0, deferredReplayToken);
                return deferredReplayToken != 0
                           ? KeyDownDispatchResult::AwaitingCompletion
                           : KeyDownDispatchResult::Complete;
            }
        }

        if (KeystrokeState.Function == FUNCTION_PUNCTUATION && _msgWndHandle)
        {
            PerfTimer asyncPuncTimer;
            std::wstring punctuationCommitText;
            const WCHAR *punctuation = _pCompositionProcessorEngine->GetPunctuation(wch);
            punctuationCommitText = punctuation ? punctuation : L"";
            const bool shouldFinalizeFirstCandidateWithPunctuation =
                _candidateMode != CANDIDATE_NONE && _pCandidateListUIPresenter &&
                Global::CommitWithFirstCandPunc.count(wch) > 0;
            if (!shouldFinalizeFirstCandidateWithPunctuation)
            {
                // The text is carried by this exact async request. No global
                // single-slot cache can be overwritten by the next key.
            }
            else
            {
                // Empty means the edit session must consume this request's
                // candidate reply and append the punctuation derived from wch.
                punctuationCommitText.clear();
            }
            _PostAsyncKeyRequest(WM_AsyncPunctuationCommit, code, wch, requestId,
                                 std::move(punctuationCommitText), 0, 0,
                                 deferredReplayToken);
            return deferredReplayToken != 0
                       ? KeyDownDispatchResult::AwaitingCompletion
                       : KeyDownDispatchResult::Complete;
        }

        if (KeystrokeState.Function == FUNCTION_SELECT_BY_NUMBER && _msgWndHandle)
        {
            _PostAsyncKeyRequest(WM_AsyncNumberCandidateCommit, code, wch, requestId,
                                 {}, 0, 0, deferredReplayToken);
            return deferredReplayToken != 0
                       ? KeyDownDispatchResult::AwaitingCompletion
                       : KeyDownDispatchResult::Complete;
        }
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
            PerfTimer invokeTimer;
            _InvokeKeyHandler(pContext, code, wch, (DWORD)lParam, KeystrokeState,
                              requestId, {}, 0, 0, 0,
                              deferredReplayToken);
            if (deferredReplayToken != 0)
            {
                return KeyDownDispatchResult::AwaitingCompletion;
            }
        }
    }
    else if (KeystrokeState.Category == CATEGORY_INVOKE_COMPOSITION_EDIT_SESSION)
    {
        // Invoke key handler edit session
        KeystrokeState.Category = CATEGORY_COMPOSING;
        PerfTimer invokeTimer;
        _InvokeKeyHandler(pContext, code, wch, (DWORD)lParam, KeystrokeState,
                          FANY_IME_NO_REQUEST_ID);
    }


    if (isPunctuationKey)
    {
    }
    return KeyDownDispatchResult::Complete;
}

//+---------------------------------------------------------------------------
//
// ITfKeyEventSink::OnTestKeyUp
//
// Called by the system to query this service wants a potential keystroke.
//----------------------------------------------------------------------------

STDAPI CMetasequoiaIME::OnTestKeyUp(ITfContext *pContext, WPARAM wParam, LPARAM lParam, BOOL *pIsEaten)
{
    if (pContext == nullptr || pIsEaten == nullptr)
    {
        return E_INVALIDARG;
    }

    Global::UpdateModifiers(wParam, lParam);

    if (_HasDeferredKeyBarrier())
    {
        // A matching deferred key-down may or may not have fit in the bounded
        // queue. Letting all key-ups through is harmless and guarantees the
        // application never observes a down without its release.
        *pIsEaten = FALSE;
        return S_OK;
    }

    _KEYSTROKE_STATE KeystrokeState = {};
    WCHAR wch = '\0';
    UINT code = 0;

    *pIsEaten = _IsKeyEaten(pContext, (UINT)wParam, &code, &wch,
                            &KeystrokeState);

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
    if (pContext == nullptr || pIsEaten == nullptr)
    {
        return E_INVALIDARG;
    }
    Global::UpdateModifiers(wParam, lParam);

    if (_HasDeferredKeyBarrier())
    {
        const bool isShift = wParam == VK_SHIFT || wParam == VK_LSHIFT ||
                             wParam == VK_RSHIFT;
        const uint64_t expectedToken =
            _expectedWorkerFocusToken.load(std::memory_order_acquire);
        const bool transportUnavailable =
            expectedToken == 0 ||
            !_workerCommitReady.load(std::memory_order_acquire) ||
            _acknowledgedWorkerFocusToken.load(std::memory_order_acquire) !=
                expectedToken;
        if (isShift && Global::PureShiftKeyUp && transportUnavailable &&
            _pCompositionProcessorEngine && _DeferredKeyQueueHasCapacity())
        {
            // A preserved Shift is not delivered while the reconnect barrier
            // owns the keystroke stream. Apply the local compartment toggle
            // here and queue only the composition-finalization phase. This is
            // restricted to an unavailable transport, so the ordinary
            // OnPreservedKey path cannot toggle the same key twice.
            _serverUnavailableFallbackActive = true;
            _pCompositionProcessorEngine->ToggleIMEMode(_GetThreadMgr(),
                                                         _GetClientId());
            _KEYSTROKE_STATE toggleState = {};
            toggleState.Category = CATEGORY_COMPOSING;
            toggleState.Function = FUNCTION_TOGGLE_IME_MODE;
            *pIsEaten = _QueueDeferredKeyDown(
                            pContext, wParam, lParam, L'\0',
                            CaptureIpcModifiers(), toggleState)
                            ? TRUE
                            : FALSE;
            return S_OK;
        }
        *pIsEaten = FALSE;
        return S_OK;
    }

    _KEYSTROKE_STATE KeystrokeState = {};
    WCHAR wch = '\0';
    UINT code = 0;
    *pIsEaten = _IsKeyEaten(pContext, (UINT)wParam, &code, &wch,
                            &KeystrokeState);

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
    if (pContext == nullptr || pIsEaten == nullptr)
    {
        return E_INVALIDARG;
    }
    const bool eligible = _pCompositionProcessorEngine &&
                          _pCompositionProcessorEngine->IsPreservedKeyEligible(rguid) &&
                          _DeferredKeyQueueHasCapacity();
    *pIsEaten = eligible && _QueueDeferredPreservedKey(pContext, rguid)
                    ? TRUE
                    : FALSE;
    if (*pIsEaten &&
        _localSessionResetPending.load(std::memory_order_acquire))
    {
        const UINT resetToken =
            _localSessionResetToken.load(std::memory_order_acquire);
        _RequestLocalSessionReset(pContext, resetToken);
    }
    return S_OK;
}

void CMetasequoiaIME::_DispatchPreservedKey(_In_ ITfContext *pContext,
                                             REFGUID preservedKey,
                                             _Out_ BOOL *pIsEaten,
                                             uint64_t expectedFocusGeneration,
                                             bool isPrevalidated,
                                             uint64_t deferredReplayToken)
{
    *pIsEaten = FALSE;
    if (pContext == nullptr || _pCompositionProcessorEngine == nullptr ||
        expectedFocusGeneration == 0 ||
        expectedFocusGeneration != _deferredKeyFocusGeneration)
    {
        return;
    }

    BOOL pNeedToggleIMEMode = FALSE;

    _pCompositionProcessorEngine->OnPreservedKey( //
        pContext,                                //
        preservedKey,                            //
        pIsEaten,                                //
        _GetThreadMgr(),                         //
        _GetClientId(),                          //
        &pNeedToggleIMEMode,                     //
        isPrevalidated ? TRUE : FALSE,           //
        _serverUnavailableFallbackActive ? FALSE : TRUE //
    );

    if (pNeedToggleIMEMode &&
        expectedFocusGeneration == _deferredKeyFocusGeneration)
    {
        // The preserved-key implementation also sends the Shift event to the
        // Server.  A failed/ambiguous send marks the local session dirty.  Do
        // not let the subsequent local edit falsely Complete this token; the
        // replacement epoch receives the authoritative status snapshot and
        // then retries only the already-applied local toggle phase.
        if (deferredReplayToken != 0 &&
            _localSessionResetPending.load(std::memory_order_acquire))
        {
            _RetryDeferredKeyReplay(deferredReplayToken);
            return;
        }
        _KEYSTROKE_STATE KeystrokeState = {};
        WCHAR wch = '\0';
        UINT code = 0;
        KeystrokeState.Category = CATEGORY_COMPOSING;
        KeystrokeState.Function = FUNCTION_TOGGLE_IME_MODE;
        _InvokeKeyHandler(pContext, code, wch, (DWORD)0, KeystrokeState,
                          FANY_IME_NO_REQUEST_ID, {}, 0, 0, 0,
                          deferredReplayToken);
    }
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
