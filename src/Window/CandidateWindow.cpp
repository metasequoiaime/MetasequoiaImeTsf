#include "Private.h"
#include "Globals.h"
#include "BaseWindow.h"
#include "Globals.h"
#include <corecrt_wstring.h>
#include <debugapi.h>
#include <string>
#include <winnt.h>
#include <winuser.h>
#include <dwmapi.h>
#include "CandidateWindow.h"
#include "CompositionProcessorEngine.h"
#include "FanyUtils.h"

#pragma comment(lib, "dwmapi.lib")

COLORREF _AdjustTextColor(_In_ COLORREF crColor, _In_ COLORREF crBkColor);

//+---------------------------------------------------------------------------
//
// ctor
//
//----------------------------------------------------------------------------

CCandidateWindow::CCandidateWindow(_In_ CANDWNDCALLBACK pfnCallback, _In_ void *pv, _In_ CCandidateRange *pIndexRange,
                                   _In_ BOOL isStoreAppMode, _In_ CMetasequoiaIME *pTextService)
{
    _currentSelection = 0;

    _SetTextColor(CANDWND_ITEM_COLOR, GetSysColor(COLOR_WINDOW)); // text color is black
    _SetFillColor((HBRUSH)(COLOR_WINDOW + 1));

    _pIndexRange = pIndexRange;

    _pfnCallback = pfnCallback;
    _pObj = pv;

    _pShadowWnd = nullptr;

    _cyRow = CANDWND_ROW_WIDTH;
    _cxTitle = 0;

    _wndWidth = 0;

    _dontAdjustOnEmptyItemPage = FALSE;

    _isStoreAppMode = isStoreAppMode;

    _pTextService = pTextService;
}

//+---------------------------------------------------------------------------
//
// dtor
//
//----------------------------------------------------------------------------

CCandidateWindow::~CCandidateWindow()
{
    _ClearList();
    _DeleteShadowWnd();
}

//+---------------------------------------------------------------------------
//
// _Create
//
// CandidateWinow is the top window
//----------------------------------------------------------------------------

BOOL CCandidateWindow::_Create(ATOM atom, _In_ UINT wndWidth, _In_opt_ HWND parentWndHandle)
{
    BOOL ret = FALSE;
    _wndWidth = wndWidth;

    ret = _CreateMainWindow(atom, parentWndHandle);
    if (FALSE == ret)
    {
        goto Exit;
    }

    ret = _CreateBackGroundShadowWindow();
    if (FALSE == ret)
    {
        goto Exit;
    }

    _ResizeWindow();

Exit:
    return TRUE;
}

void CCandidateWindow::_Destroy()
{
    if (this->_GetWnd() != nullptr)
    {
        DestroyWindow(this->_GetWnd());
        this->_SetWnd(nullptr);
    }
}

BOOL CCandidateWindow::_CreateMainWindow(ATOM atom, _In_opt_ HWND parentWndHandle)
{
    _SetUIWnd(this);

    if (!CBaseWindow::_Create(                                          //
            atom,                                                       //
            WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED, WS_POPUP, //
            NULL,                                                       //
            0,                                                          //
            0,                                                          //
            parentWndHandle))                                           //
    {
        return FALSE;
    }
    else
    {
        //
        // Default layerd window is transparent, if you want to show it,
        // you need to set the alpha value, so uncomment the following code
        //
        // SetLayeredWindowAttributes(this->_GetWnd(), RGB(0, 0, 0), 255, LWA_ALPHA);
        // MARGINS margins = {-1}; // -1 表示整个窗口
        // DwmExtendFrameIntoClientArea(this->_GetWnd(), &margins);
    }

    return TRUE;
}

BOOL CCandidateWindow::_CreateBackGroundShadowWindow()
{
    _pShadowWnd = new (std::nothrow) CShadowWindow(this);
    if (_pShadowWnd == nullptr)
    {
        return FALSE;
    }

    if (!_pShadowWnd->_Create(Global::AtomShadowWindow, WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
                              WS_DISABLED | WS_POPUP, this))
    {
        _DeleteShadowWnd();
        return FALSE;
    }

    return TRUE;
}

void CCandidateWindow::_ResizeWindow()
{
    CBaseWindow::_Resize(0, 0, 20, 10); // Mock size, nonsense
}

//+---------------------------------------------------------------------------
//
// _Move
//
//----------------------------------------------------------------------------

void CCandidateWindow::_Move(int x, int y)
{
    CBaseWindow::_Move(x, y);
}

//+---------------------------------------------------------------------------
//
// _Show
//
//----------------------------------------------------------------------------

void CCandidateWindow::_Show(BOOL isShowWnd)
{
    if (_pShadowWnd)
    {
        //
        // We do not need shadow window during current development time
        //
        _pShadowWnd->_Show(FALSE);
    }
    CBaseWindow::_Show(isShowWnd);
}

//+---------------------------------------------------------------------------
//
// _SetTextColor
// _SetFillColor
//
//----------------------------------------------------------------------------

VOID CCandidateWindow::_SetTextColor(_In_ COLORREF crColor, _In_ COLORREF crBkColor)
{
    _crTextColor = _AdjustTextColor(crColor, crBkColor);
    _crBkColor = crBkColor;
}

VOID CCandidateWindow::_SetFillColor(_In_ HBRUSH hBrush)
{
    _brshBkColor = hBrush;
}

//+---------------------------------------------------------------------------
//
// _WindowProcCallback
//
// Cand window proc.
//----------------------------------------------------------------------------

const int PageCountPosition = 1;
const int StringPosition = 4;

LRESULT CALLBACK CCandidateWindow::_WindowProcCallback(_In_ HWND wndHandle, UINT uMsg, _In_ WPARAM wParam,
                                                       _In_ LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_ERASEBKGND:
        return 0;
    case WM_CREATE: {
    }
        return 0;
    case WM_DESTROY:
        _DeleteShadowWnd();
        return 0;

    case WM_WINDOWPOSCHANGED: {
        WINDOWPOS *pWndPos = (WINDOWPOS *)lParam;

        // move shadow
        if (_pShadowWnd)
        {
            _pShadowWnd->_OnOwnerWndMoved((pWndPos->flags & SWP_NOSIZE) == 0);
        }

        _FireMessageToLightDismiss(wndHandle, pWndPos);
    }
    break;

    case WM_WINDOWPOSCHANGING: {
        WINDOWPOS *pWndPos = (WINDOWPOS *)lParam;

        // show/hide shadow
        if (_pShadowWnd)
        {
            if ((pWndPos->flags & SWP_HIDEWINDOW) != 0)
            {
                _pShadowWnd->_Show(FALSE);
            }

            // don't go behaind of shadow
            if (((pWndPos->flags & SWP_NOZORDER) == 0) && (pWndPos->hwndInsertAfter == _pShadowWnd->_GetWnd()))
            {
                pWndPos->flags |= SWP_NOZORDER;
            }

            _pShadowWnd->_OnOwnerWndMoved((pWndPos->flags & SWP_NOSIZE) == 0);
        }
    }
    break;

    case WM_SHOWWINDOW:
        // show/hide shadow
        if (_pShadowWnd)
        {
            _pShadowWnd->_Show((BOOL)wParam);
        }

        break;

    case WM_PAINT: {
        // _OnPaintWithWebview2();
    }
        return 0;

    case WM_SETCURSOR: {
        POINT cursorPoint;

        GetCursorPos(&cursorPoint);
        MapWindowPoints(NULL, wndHandle, &cursorPoint, 1);

        // handle mouse message
        _HandleMouseMsg(HIWORD(lParam), cursorPoint);
    }
        return 1;

    case WM_MOUSEMOVE:
    case WM_LBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_MBUTTONUP:
    case WM_RBUTTONUP: {
        POINT point;

        POINTSTOPOINT(point, MAKEPOINTS(lParam));

        // handle mouse message
        _HandleMouseMsg(uMsg, point);
    }
        // we processes this message, it should return zero.
        return 0;

    case WM_MOUSEACTIVATE: {
        WORD mouseEvent = HIWORD(lParam);
        if (mouseEvent == WM_LBUTTONDOWN || mouseEvent == WM_RBUTTONDOWN || mouseEvent == WM_MBUTTONDOWN)
        {
            return MA_NOACTIVATE;
        }
    }
    break;

    case WM_POINTERACTIVATE:
        return PA_NOACTIVATE;
    }

    return DefWindowProc(wndHandle, uMsg, wParam, lParam);
}

//+---------------------------------------------------------------------------
//
// _HandleMouseMsg
//
//----------------------------------------------------------------------------

void CCandidateWindow::_HandleMouseMsg(_In_ UINT mouseMsg, _In_ POINT point)
{
    switch (mouseMsg)
    {
    case WM_MOUSEMOVE:
        _OnMouseMove(point);
        break;
    case WM_LBUTTONDOWN:
        _OnLButtonDown(point);
        break;
    case WM_LBUTTONUP:
        _OnLButtonUp(point);
        break;
    }
}

//+---------------------------------------------------------------------------
//
// _OnPaint
//
//----------------------------------------------------------------------------

void CCandidateWindow::_OnPaint(_In_ HDC dcHandle, _In_ PAINTSTRUCT *pPaintStruct)
{
    SetBkMode(dcHandle, TRANSPARENT);

    HFONT hFontOld = (HFONT)SelectObject(dcHandle, Global::defaultlFontHandle);

    FillRect(dcHandle, &pPaintStruct->rcPaint, _brshBkColor);

    UINT currentPageIndex = 0;
    UINT currentPage = 0;

    if (FAILED(_GetCurrentPage(&currentPage)))
    {
        goto cleanup;
    }

    _AdjustPageIndex(currentPage, currentPageIndex);

    _DrawList(dcHandle, currentPageIndex, &pPaintStruct->rcPaint);

cleanup:
    SelectObject(dcHandle, hFontOld);
}

void CCandidateWindow::_OnPaintWithWebview2()
{
    UINT currentPageIndex = 0;
    UINT currentPage = 0;

    if (FAILED(_GetCurrentPage(&currentPage)))
    {
        return;
    }

    _AdjustPageIndex(currentPage, currentPageIndex);
    _DrawListWithWebview2(currentPageIndex);
}

//+---------------------------------------------------------------------------
//
// _OnLButtonDown
//
//----------------------------------------------------------------------------

void CCandidateWindow::_OnLButtonDown(POINT pt)
{
    RECT rcWindow = {0, 0, 0, 0};
    ;
    _GetClientRect(&rcWindow);

    int cyLine = _cyRow;

    UINT candidateListPageCnt = _pIndexRange->Count();
    UINT index = 0;
    int currentPage = 0;

    if (FAILED(_GetCurrentPage(&currentPage)))
    {
        return;
    }

    // Hit test on list items
    index = *_PageIndex.GetAt(currentPage);

    for (UINT pageCount = 0; (index < _candidateList.Count()) && (pageCount < candidateListPageCnt);
         index++, pageCount++)
    {
        RECT rc = {0, 0, 0, 0};

        rc.left = rcWindow.left;
        rc.right = rcWindow.right - GetSystemMetrics(SM_CXVSCROLL) * 2;
        rc.top = rcWindow.top + (pageCount * cyLine);
        rc.bottom = rcWindow.top + ((pageCount + 1) * cyLine);

        if (PtInRect(&rc, pt) && _pfnCallback)
        {
            SetCursor(LoadCursor(NULL, IDC_HAND));
            _currentSelection = index;
            _pfnCallback(_pObj, CAND_ITEM_SELECT);
            return;
        }
    }

    SetCursor(LoadCursor(NULL, IDC_ARROW));
}

//+---------------------------------------------------------------------------
//
// _OnMouseMove
//
//----------------------------------------------------------------------------

void CCandidateWindow::_OnMouseMove(POINT pt)
{
    RECT rcWindow = {0, 0, 0, 0};

    _GetClientRect(&rcWindow);

    RECT rc = {0, 0, 0, 0};

    rc.left = rcWindow.left;
    rc.right = rcWindow.right - GetSystemMetrics(SM_CXVSCROLL) * 2;

    rc.top = rcWindow.top;
    rc.bottom = rcWindow.bottom;

    if (PtInRect(&rc, pt))
    {
        SetCursor(LoadCursor(NULL, IDC_HAND));
        return;
    }

    SetCursor(LoadCursor(NULL, IDC_ARROW));
}

//+---------------------------------------------------------------------------
//
// _DrawList
//
//----------------------------------------------------------------------------

void CCandidateWindow::_DrawList(_In_ HDC dcHandle, _In_ UINT iIndex, _In_ RECT *prc)
{
    // 创建并选择字体
    HFONT hFont = CreateFont(-22, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS,
                             CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, METASEQUOIAIME_FONT_DEFAULT);
    HFONT hOldFont = (HFONT)SelectObject(dcHandle, hFont);

    int pageCount = 0;
    int candidateListPageCnt = _pIndexRange->Count();

    int cxLine = _TextMetric.tmAveCharWidth;
    int cyLine = max(_cyRow, _TextMetric.tmHeight);
    int cyOffset = (cyLine == _cyRow ? (cyLine - _TextMetric.tmHeight) / 2 : 0);

    RECT rc;

    const size_t lenOfPageCount = 16;
    for (; (iIndex < _candidateList.Count()) && (pageCount < candidateListPageCnt); iIndex++, pageCount++)
    {
        WCHAR pageCountString[lenOfPageCount] = {'\0'};
        CCandidateListItem *pItemList = nullptr;

        rc.top = prc->top + pageCount * cyLine;
        rc.bottom = rc.top + cyLine;

        rc.left = prc->left + PageCountPosition * cxLine;
        rc.right = prc->left + StringPosition * cxLine;

        // Number Font Color And BK
        SetTextColor(dcHandle, CANDWND_NUM_COLOR);
        SetBkColor(dcHandle, GetSysColor(COLOR_3DHIGHLIGHT));

        StringCchPrintf(pageCountString, ARRAYSIZE(pageCountString), L"%d", (LONG)*_pIndexRange->GetAt(pageCount));
        ExtTextOut(dcHandle, PageCountPosition * cxLine, pageCount * cyLine + cyOffset, ETO_OPAQUE, &rc,
                   pageCountString, lenOfPageCount, NULL);

        rc.left = prc->left + StringPosition * cxLine;
        rc.right = prc->right;

        // Candidate Font Color And BK
        if (_currentSelection != iIndex)
        {
            SetTextColor(dcHandle, _crTextColor);
            SetBkColor(dcHandle, GetSysColor(COLOR_3DHIGHLIGHT));
        }
        else
        {
            SetTextColor(dcHandle, CANDWND_SELECTED_ITEM_COLOR);
            SetBkColor(dcHandle, CANDWND_SELECTED_BK_COLOR);
        }

        pItemList = _candidateList.GetAt(iIndex);
        ExtTextOut(dcHandle, StringPosition * cxLine, pageCount * cyLine + cyOffset, ETO_OPAQUE, &rc,
                   pItemList->_ItemString.Get(), (DWORD)pItemList->_ItemString.GetLength(), NULL);
    }
    for (; (pageCount < candidateListPageCnt); pageCount++)
    {
        rc.top = prc->top + pageCount * cyLine;
        rc.bottom = rc.top + cyLine;

        rc.left = prc->left + PageCountPosition * cxLine;
        rc.right = prc->left + StringPosition * cxLine;

        FillRect(dcHandle, &rc, (HBRUSH)(COLOR_3DHIGHLIGHT + 1));
    }
    // Restore original fonts
    SelectObject(dcHandle, hOldFont);
    DeleteObject(hFont);
}

void CCandidateWindow::_DrawListWithWebview2(_In_ UINT iIndex)
{
    int pageCount = 0;
    int candidateListPageCnt = _pIndexRange->Count();
    int currentPageItemCnt = _candidateList.Count() - iIndex > candidateListPageCnt //
                                 ? candidateListPageCnt                             //
                                 : _candidateList.Count() - iIndex;

    const size_t lenOfPageCount = 16;

    CStringRange preeditStringRange = _pTextService->GetCompositionProcessorEngine()->GetKeystrokeBuffer();

    std::wstring preeditString(preeditStringRange.Get(), preeditStringRange.GetLength());

    std::wstring candWString = preeditString;

    //
    // Draw candidate list
    //
    for (; (iIndex < _candidateList.Count()) && (pageCount < candidateListPageCnt); iIndex++, pageCount++)
    {
        std::wstring pageCountString = std::to_wstring(*_pIndexRange->GetAt(pageCount));
        CCandidateListItem *pItemList = _candidateList.GetAt(iIndex);
        std::wstring itemString(pItemList->_ItemString.Get(), pItemList->_ItemString.GetLength());
        candWString = candWString + L"," + pageCountString + L". " + itemString;
    }

    if (Global::CandidateWString != candWString)
    {
        Global::CandidateWString = candWString;

        HWND UIHwnd = FindWindow(L"global_candidate_window", NULL);
        if (UIHwnd == NULL)
        {
            DWORD error = GetLastError();
            std::wstring errorString = L"FindWindow failed with error: " + std::to_wstring(error);
#ifdef FANY_DEBUG
            OutputDebugString(fmt::format(L"[msime]: {}", errorString).c_str());
#endif
        }
        COPYDATASTRUCT cds;
        cds.dwData = 1;
        cds.cbData = (candWString.size() + 1) * sizeof(WCHAR);
        cds.lpData = (PVOID)candWString.c_str();
        LRESULT result = SendMessage(UIHwnd, WM_COPYDATA, (WPARAM)UIHwnd, (LPARAM)&cds);
        if (result == 0)
        {
            DWORD error = GetLastError();
            if (error != 0)
            {
                std::wstring errorString = L"SendMessage CandidateWString failed with error: " + std::to_wstring(error);
#ifdef FANY_DEBUG
                OutputDebugString(fmt::format(L"[msime]: {}", errorString).c_str());
#endif
            }
            else
            {
#ifdef FANY_DEBUG
                OutputDebugString(
                    fmt::format(L"[msime]: SendMessage CandidateWString success, but result is 0").c_str());
#endif
            }
        }
    }
}

//+---------------------------------------------------------------------------
//
// _DrawBorder
//
//----------------------------------------------------------------------------
void CCandidateWindow::_DrawBorder(_In_ HWND wndHandle, _In_ int cx)
{
    RECT rcWnd;

    HDC dcHandle = GetWindowDC(wndHandle);

    GetWindowRect(wndHandle, &rcWnd);
    // zero based
    OffsetRect(&rcWnd, -rcWnd.left, -rcWnd.top);

    HPEN hPen = CreatePen(PS_DOT, cx, CANDWND_BORDER_COLOR);
    HPEN hPenOld = (HPEN)SelectObject(dcHandle, hPen);
    HBRUSH hBorderBrush = (HBRUSH)GetStockObject(NULL_BRUSH);
    HBRUSH hBorderBrushOld = (HBRUSH)SelectObject(dcHandle, hBorderBrush);

    Rectangle(dcHandle, rcWnd.left, rcWnd.top, rcWnd.right, rcWnd.bottom);

    SelectObject(dcHandle, hPenOld);
    SelectObject(dcHandle, hBorderBrushOld);
    DeleteObject(hPen);
    DeleteObject(hBorderBrush);
    ReleaseDC(wndHandle, dcHandle);
}

//+---------------------------------------------------------------------------
//
// _AddString
//
// Inflate the candidate list
//
//----------------------------------------------------------------------------

void CCandidateWindow::_AddString(_Inout_ CCandidateListItem *pCandidateItem, _In_ BOOL isAddFindKeyCode)
{
    DWORD_PTR dwItemString = pCandidateItem->_ItemString.GetLength();
    const WCHAR *pwchString = nullptr;
    if (dwItemString)
    {
        pwchString = new (std::nothrow) WCHAR[dwItemString];
        if (!pwchString)
        {
            return;
        }
        memcpy((void *)pwchString, pCandidateItem->_ItemString.Get(), dwItemString * sizeof(WCHAR));
    }

    DWORD_PTR itemWildcard = pCandidateItem->_FindKeyCode.GetLength();
    const WCHAR *pwchWildcard = nullptr;
    if (itemWildcard && isAddFindKeyCode)
    {
        pwchWildcard = new (std::nothrow) WCHAR[itemWildcard];
        if (!pwchWildcard)
        {
            if (pwchString)
            {
                delete[] pwchString;
            }
            return;
        }
        memcpy((void *)pwchWildcard, pCandidateItem->_FindKeyCode.Get(), itemWildcard * sizeof(WCHAR));
    }

    CCandidateListItem *pLI = nullptr;
    pLI = _candidateList.Append();
    if (!pLI)
    {
        if (pwchString)
        {
            delete[] pwchString;
            pwchString = nullptr;
        }
        if (pwchWildcard)
        {
            delete[] pwchWildcard;
            pwchWildcard = nullptr;
        }
        return;
    }

    if (pwchString)
    {
        pLI->_ItemString.Set(pwchString, dwItemString);
    }
    if (pwchWildcard)
    {
        pLI->_FindKeyCode.Set(pwchWildcard, itemWildcard);
    }

    return;
}

//+---------------------------------------------------------------------------
//
// _ClearList
//
//----------------------------------------------------------------------------

void CCandidateWindow::_ClearList()
{
    for (UINT index = 0; index < _candidateList.Count(); index++)
    {
        CCandidateListItem *pItemList = nullptr;
        pItemList = _candidateList.GetAt(index);
        delete[] pItemList->_ItemString.Get();
        delete[] pItemList->_FindKeyCode.Get();
    }
    _currentSelection = 0;
    _candidateList.Clear();
    _PageIndex.Clear();
}

//+---------------------------------------------------------------------------
//
// _SetScrollInfo
//
//----------------------------------------------------------------------------

void CCandidateWindow::_SetScrollInfo(_In_ int nMax, _In_ int nPage)
{
    CScrollInfo si;
    si.nMax = nMax;
    si.nPage = nPage;
    si.nPos = 0;
}

//+---------------------------------------------------------------------------
//
// _GetCandidateString
//
//----------------------------------------------------------------------------

DWORD CCandidateWindow::_GetCandidateString(_In_ int iIndex,
                                            _Outptr_result_maybenull_z_ const WCHAR **ppwchCandidateString)
{
    CCandidateListItem *pItemList = nullptr;

    if (iIndex < 0)
    {
        *ppwchCandidateString = nullptr;
        return 0;
    }

    UINT index = static_cast<UINT>(iIndex);

    if (index >= _candidateList.Count())
    {
        *ppwchCandidateString = nullptr;
        return 0;
    }

    pItemList = _candidateList.GetAt(iIndex);
    if (ppwchCandidateString)
    {
        *ppwchCandidateString = pItemList->_ItemString.Get();
    }
    return (DWORD)pItemList->_ItemString.GetLength();
}

//+---------------------------------------------------------------------------
//
// _GetSelectedCandidateString
//
//----------------------------------------------------------------------------

DWORD CCandidateWindow::_GetSelectedCandidateString(_Outptr_result_maybenull_ const WCHAR **ppwchCandidateString)
{
    CCandidateListItem *pItemList = nullptr;

    if (_currentSelection >= _candidateList.Count())
    {
        *ppwchCandidateString = nullptr;
        return 0;
    }

    pItemList = _candidateList.GetAt(_currentSelection);
    if (ppwchCandidateString)
    {
        *ppwchCandidateString = pItemList->_ItemString.Get();
    }
    return (DWORD)pItemList->_ItemString.GetLength();
}

//+---------------------------------------------------------------------------
//
// _SetSelectionInPage
//
//----------------------------------------------------------------------------

BOOL CCandidateWindow::_SetSelectionInPage(int nPos)
{
    if (nPos < 0)
    {
        return FALSE;
    }

    UINT pos = static_cast<UINT>(nPos);

    if (pos >= _candidateList.Count())
    {
        return FALSE;
    }

    int currentPage = 0;
    if (FAILED(_GetCurrentPage(&currentPage)))
    {
        return FALSE;
    }

    _currentSelection = *_PageIndex.GetAt(currentPage) + nPos;

    return TRUE;
}

//+---------------------------------------------------------------------------
//
// _MoveSelection
//
//----------------------------------------------------------------------------

BOOL CCandidateWindow::_MoveSelection(_In_ int offSet, _In_ BOOL isNotify)
{
    if (_currentSelection + offSet >= _candidateList.Count())
    {
        return FALSE;
    }

    _currentSelection += offSet;

    _dontAdjustOnEmptyItemPage = TRUE;

    return TRUE;
}

//+---------------------------------------------------------------------------
//
// _SetSelection
//
//----------------------------------------------------------------------------

BOOL CCandidateWindow::_SetSelection(_In_ int selectedIndex, _In_ BOOL isNotify)
{
    if (selectedIndex == -1)
    {
        selectedIndex = _candidateList.Count() - 1;
    }

    if (selectedIndex < 0)
    {
        return FALSE;
    }

    int candCnt = static_cast<int>(_candidateList.Count());
    if (selectedIndex >= candCnt)
    {
        return FALSE;
    }

    _currentSelection = static_cast<UINT>(selectedIndex);

    BOOL ret = _AdjustPageIndexForSelection();

    return ret;
}

//+---------------------------------------------------------------------------
//
// _SetSelection
//
//----------------------------------------------------------------------------
void CCandidateWindow::_SetSelection(_In_ int nIndex)
{
    _currentSelection = nIndex;
}

//+---------------------------------------------------------------------------
//
// _MovePage
//
//----------------------------------------------------------------------------

BOOL CCandidateWindow::_MovePage(_In_ int offSet, _In_ BOOL isNotify)
{
    if (offSet == 0)
    {
        return TRUE;
    }

    int currentPage = 0;
    int selectionOffset = 0;
    int newPage = 0;

    if (FAILED(_GetCurrentPage(&currentPage)))
    {
        return FALSE;
    }

    newPage = currentPage + offSet;
    if ((newPage < 0) || (newPage >= static_cast<int>(_PageIndex.Count())))
    {
        return FALSE;
    }

    // If current selection is at the top of the page AND
    // we are on the "default" page border, then we don't
    // want adjustment to eliminate empty entries.
    //
    // We do this for keeping behavior inline with downlevel.
    if (_currentSelection % _pIndexRange->Count() == 0 && _currentSelection == *_PageIndex.GetAt(currentPage))
    {
        _dontAdjustOnEmptyItemPage = TRUE;
    }

    selectionOffset = _currentSelection - *_PageIndex.GetAt(currentPage);
    _currentSelection = *_PageIndex.GetAt(newPage) + selectionOffset;
    _currentSelection = _candidateList.Count() > _currentSelection ? _currentSelection : _candidateList.Count() - 1;

    return TRUE;
}

//+---------------------------------------------------------------------------
//
// _SetSelectionOffset
//
//----------------------------------------------------------------------------

BOOL CCandidateWindow::_SetSelectionOffset(_In_ int offSet)
{
    if (_currentSelection + offSet >= _candidateList.Count())
    {
        return FALSE;
    }

    BOOL fCurrentPageHasEmptyItems = FALSE;
    BOOL fAdjustPageIndex = TRUE;

    _CurrentPageHasEmptyItems(&fCurrentPageHasEmptyItems);

    int newOffset = _currentSelection + offSet;

    // For SB_LINEUP and SB_LINEDOWN, we need to special case if CurrentPageHasEmptyItems.
    // CurrentPageHasEmptyItems if we are on the last page.
    if ((offSet == 1 || offSet == -1) && fCurrentPageHasEmptyItems && _PageIndex.Count() > 1)
    {
        int iPageIndex = *_PageIndex.GetAt(_PageIndex.Count() - 1);
        // Moving on the last page and last page has empty items.
        if (newOffset >= iPageIndex)
        {
            fAdjustPageIndex = FALSE;
        }
        // Moving across page border.
        else if (newOffset < iPageIndex)
        {
            fAdjustPageIndex = TRUE;
        }

        _dontAdjustOnEmptyItemPage = TRUE;
    }

    _currentSelection = newOffset;

    if (fAdjustPageIndex)
    {
        return _AdjustPageIndexForSelection();
    }

    return TRUE;
}

//+---------------------------------------------------------------------------
//
// _GetPageIndex
//
//----------------------------------------------------------------------------

HRESULT CCandidateWindow::_GetPageIndex(UINT *pIndex, _In_ UINT uSize, _Inout_ UINT *puPageCnt)
{
    HRESULT hr = S_OK;

    if (uSize > _PageIndex.Count())
    {
        uSize = _PageIndex.Count();
    }
    else
    {
        hr = S_FALSE;
    }

    if (pIndex)
    {
        for (UINT i = 0; i < uSize; i++)
        {
            *pIndex = *_PageIndex.GetAt(i);
            pIndex++;
        }
    }

    *puPageCnt = _PageIndex.Count();

    return hr;
}

//+---------------------------------------------------------------------------
//
// _SetPageIndex
//
//----------------------------------------------------------------------------

HRESULT CCandidateWindow::_SetPageIndex(UINT *pIndex, _In_ UINT uPageCnt)
{
    uPageCnt;

    _PageIndex.Clear();

    for (UINT i = 0; i < uPageCnt; i++)
    {
        UINT *pLastNewPageIndex = _PageIndex.Append();
        if (pLastNewPageIndex != nullptr)
        {
            *pLastNewPageIndex = *pIndex;
            pIndex++;
        }
    }

    return S_OK;
}

//+---------------------------------------------------------------------------
//
// _GetCurrentPage
//
//----------------------------------------------------------------------------

HRESULT CCandidateWindow::_GetCurrentPage(_Inout_ UINT *pCurrentPage)
{
    HRESULT hr = S_OK;

    if (pCurrentPage == nullptr)
    {
        hr = E_INVALIDARG;
        goto Exit;
    }

    *pCurrentPage = 0;

    if (_PageIndex.Count() == 0)
    {
        hr = E_UNEXPECTED;
        goto Exit;
    }

    if (_PageIndex.Count() == 1)
    {
        *pCurrentPage = 0;
        goto Exit;
    }

    UINT i = 0;
    for (i = 1; i < _PageIndex.Count(); i++)
    {
        UINT uPageIndex = *_PageIndex.GetAt(i);

        if (uPageIndex > _currentSelection)
        {
            break;
        }
    }

    *pCurrentPage = i - 1;

Exit:
    return hr;
}

//+---------------------------------------------------------------------------
//
// _GetCurrentPage
//
//----------------------------------------------------------------------------

HRESULT CCandidateWindow::_GetCurrentPage(_Inout_ int *pCurrentPage)
{
    HRESULT hr = E_FAIL;
    UINT needCastCurrentPage = 0;

    if (nullptr == pCurrentPage)
    {
        goto Exit;
    }

    *pCurrentPage = 0;

    hr = _GetCurrentPage(&needCastCurrentPage);
    if (FAILED(hr))
    {
        goto Exit;
    }

    hr = UIntToInt(needCastCurrentPage, pCurrentPage);
    if (FAILED(hr))
    {
        goto Exit;
    }

Exit:
    return hr;
}

//+---------------------------------------------------------------------------
//
// _AdjustPageIndexForSelection
//
//----------------------------------------------------------------------------

BOOL CCandidateWindow::_AdjustPageIndexForSelection()
{
    UINT candidateListPageCnt = _pIndexRange->Count();
    UINT *pNewPageIndex = nullptr;
    UINT newPageCnt = 0;

    if (_candidateList.Count() < candidateListPageCnt)
    {
        // no needed to restruct page index
        return TRUE;
    }

    // B is number of pages before the current page
    // A is number of pages after the current page
    // uNewPageCount is A + B + 1;
    // A is (uItemsAfter - 1) / candidateListPageCnt + 1 ->
    //      (_CandidateListCount - _currentSelection - CandidateListPageCount - 1) / candidateListPageCnt + 1->
    //      (_CandidateListCount - _currentSelection - 1) / candidateListPageCnt
    // B is (uItemsBefore - 1) / candidateListPageCnt + 1 ->
    //      (_currentSelection - 1) / candidateListPageCnt + 1
    // A + B is (_CandidateListCount - 2) / candidateListPageCnt + 1

    BOOL isBefore = _currentSelection;
    BOOL isAfter = _candidateList.Count() > _currentSelection + candidateListPageCnt;

    // only have current page
    if (!isBefore && !isAfter)
    {
        newPageCnt = 1;
    }
    // only have after pages; just count the total number of pages
    else if (!isBefore && isAfter)
    {
        newPageCnt = (_candidateList.Count() - 1) / candidateListPageCnt + 1;
    }
    // we are at the last page
    else if (isBefore && !isAfter)
    {
        newPageCnt = 2 + (_currentSelection - 1) / candidateListPageCnt;
    }
    else if (isBefore && isAfter)
    {
        newPageCnt = (_candidateList.Count() - 2) / candidateListPageCnt + 2;
    }

    pNewPageIndex = new (std::nothrow) UINT[newPageCnt];
    if (pNewPageIndex == nullptr)
    {
        return FALSE;
    }
    pNewPageIndex[0] = 0;
    UINT firstPage = _currentSelection % candidateListPageCnt;
    if (firstPage && newPageCnt > 1)
    {
        pNewPageIndex[1] = firstPage;
    }

    for (UINT i = firstPage ? 2 : 1; i < newPageCnt; ++i)
    {
        pNewPageIndex[i] = pNewPageIndex[i - 1] + candidateListPageCnt;
    }

    _SetPageIndex(pNewPageIndex, newPageCnt);

    delete[] pNewPageIndex;

    return TRUE;
}

//+---------------------------------------------------------------------------
//
// _AdjustTextColor
//
//----------------------------------------------------------------------------

COLORREF _AdjustTextColor(_In_ COLORREF crColor, _In_ COLORREF crBkColor)
{
    if (!Global::IsTooSimilar(crColor, crBkColor))
    {
        return crColor;
    }
    else
    {
        return crColor ^ RGB(255, 255, 255);
    }
}

//+---------------------------------------------------------------------------
//
// _CurrentPageHasEmptyItems
//
//----------------------------------------------------------------------------

HRESULT CCandidateWindow::_CurrentPageHasEmptyItems(_Inout_ BOOL *hasEmptyItems)
{
    int candidateListPageCnt = _pIndexRange->Count();
    UINT currentPage = 0;

    if (FAILED(_GetCurrentPage(&currentPage)))
    {
        return S_FALSE;
    }

    if ((currentPage == 0 || currentPage == _PageIndex.Count() - 1) && (_PageIndex.Count() > 0) &&
        (*_PageIndex.GetAt(currentPage) > (UINT)(_candidateList.Count() - candidateListPageCnt)))
    {
        *hasEmptyItems = TRUE;
    }
    else
    {
        *hasEmptyItems = FALSE;
    }

    return S_OK;
}

//+---------------------------------------------------------------------------
//
// _FireMessageToLightDismiss
//      fire EVENT_OBJECT_IME_xxx to let LightDismiss know about IME window.
//----------------------------------------------------------------------------

void CCandidateWindow::_FireMessageToLightDismiss(_In_ HWND wndHandle, _In_ WINDOWPOS *pWndPos)
{
    if (nullptr == pWndPos)
    {
        return;
    }

    BOOL isShowWnd = ((pWndPos->flags & SWP_SHOWWINDOW) != 0);
    BOOL isHide = ((pWndPos->flags & SWP_HIDEWINDOW) != 0);
    BOOL needResize = ((pWndPos->flags & SWP_NOSIZE) == 0);
    BOOL needMove = ((pWndPos->flags & SWP_NOMOVE) == 0);
    BOOL needRedraw = ((pWndPos->flags & SWP_NOREDRAW) == 0);

    if (isShowWnd)
    {
        NotifyWinEvent(EVENT_OBJECT_IME_SHOW, wndHandle, OBJID_CLIENT, CHILDID_SELF);
    }
    else if (isHide)
    {
        NotifyWinEvent(EVENT_OBJECT_IME_HIDE, wndHandle, OBJID_CLIENT, CHILDID_SELF);
    }
    else if (needResize || needMove || needRedraw)
    {
        if (IsWindowVisible(wndHandle))
        {
            NotifyWinEvent(EVENT_OBJECT_IME_CHANGE, wndHandle, OBJID_CLIENT, CHILDID_SELF);
        }
    }
}

HRESULT CCandidateWindow::_AdjustPageIndex(_Inout_ UINT &currentPage, _Inout_ UINT &currentPageIndex)
{
    HRESULT hr = E_FAIL;
    UINT candidateListPageCnt = _pIndexRange->Count();

    currentPageIndex = *_PageIndex.GetAt(currentPage);

    BOOL hasEmptyItems = FALSE;
    if (FAILED(_CurrentPageHasEmptyItems(&hasEmptyItems)))
    {
        goto Exit;
    }

    if (FALSE == hasEmptyItems)
    {
        goto Exit;
    }

    if (TRUE == _dontAdjustOnEmptyItemPage)
    {
        goto Exit;
    }

    UINT tempSelection = _currentSelection;

    // Last page
    UINT candNum = _candidateList.Count();
    UINT pageNum = _PageIndex.Count();

    if ((currentPageIndex > candNum - candidateListPageCnt) && (pageNum > 0) && (currentPage == (pageNum - 1)))
    {
        _currentSelection = candNum - candidateListPageCnt;

        _AdjustPageIndexForSelection();

        _currentSelection = tempSelection;

        if (FAILED(_GetCurrentPage(&currentPage)))
        {
            goto Exit;
        }

        currentPageIndex = *_PageIndex.GetAt(currentPage);
    }
    // First page
    else if ((currentPageIndex < candidateListPageCnt) && (currentPage == 0))
    {
        _currentSelection = 0;

        _AdjustPageIndexForSelection();

        _currentSelection = tempSelection;
    }

    _dontAdjustOnEmptyItemPage = FALSE;
    hr = S_OK;

Exit:
    return hr;
}
void CCandidateWindow::_DeleteShadowWnd()
{
    if (nullptr != _pShadowWnd)
    {
        delete _pShadowWnd;
        _pShadowWnd = nullptr;
    }
}