#pragma once
#include <string>
#include <string_view>
#include <unordered_set>
#include <Windows.h>

namespace Global
{
inline std::wstring ZEN_BROWSER = L"zen.exe";
// inline std::unordered_set<std::wstring> VSCodeSeries = {L"Code.exe", L"Code - Insiders.exe", L"VSCodium.exe"};
// inline bool IsVSCodeLike = false;
inline LONG INVALID_Y = -100000;
inline thread_local float DpiScale = 1.0f;
} // namespace Global

namespace GlobalSettings
{
//
// 支持的 TSF 预编辑格式
//  - raw: 原始按键序列
//  - pinyin: 分词后的拼音序列
//  - empty: 行内不显示预编辑
//  - cand: 当前高亮的候选词序列（预留）
//
namespace TsfPreeditStyle
{
constexpr std::string_view Raw = "raw";
constexpr std::string_view Pinyin = "pinyin";
constexpr std::string_view Empty = "empty";
constexpr std::string_view Cand = "cand";
} // namespace TsfPreeditStyle

inline bool isKnownTsfPreeditStyle(std::string_view style)
{
    return style == TsfPreeditStyle::Raw || style == TsfPreeditStyle::Pinyin ||
           style == TsfPreeditStyle::Empty;
}

inline std::string normalizeTsfPreeditStyle(std::string_view style)
{
    if (style == TsfPreeditStyle::Pinyin || style == TsfPreeditStyle::Empty)
    {
        return std::string(style);
    }
    return std::string(TsfPreeditStyle::Raw);
}

inline std::string &tsfPreeditStyleStorage()
{
    static std::string style = std::string(TsfPreeditStyle::Raw); // 默认的原始按键序列
    return style;
}

inline const std::string &getTsfPreeditStyle()
{
    return tsfPreeditStyleStorage();
}

inline void setTsfPreeditStyle(std::string_view newStyle)
{
    tsfPreeditStyleStorage() = normalizeTsfPreeditStyle(newStyle);
}

inline void setTsfPreeditStyleFromWide(const wchar_t *style)
{
    if (!style)
    {
        setTsfPreeditStyle(TsfPreeditStyle::Raw);
        return;
    }
    if (wcscmp(style, L"pinyin") == 0)
    {
        setTsfPreeditStyle(TsfPreeditStyle::Pinyin);
    }
    else if (wcscmp(style, L"empty") == 0)
    {
        setTsfPreeditStyle(TsfPreeditStyle::Empty);
    }
    else
    {
        setTsfPreeditStyle(TsfPreeditStyle::Raw);
    }
}

inline bool isKnownTsfPreeditStyleWide(const wchar_t *style)
{
    return style && (wcscmp(style, L"raw") == 0 || wcscmp(style, L"pinyin") == 0 ||
                     wcscmp(style, L"empty") == 0);
}
} // namespace GlobalSettings

namespace GlobalIme
{
inline thread_local std::wstring word_for_creating_word = L"";
// One-shot override for TSF inline preedit after NeedToCreateWord (pinyin mode).
// Consumed by _HandleCompositionInputWorker, then cleared.
inline thread_local std::wstring pending_create_word_preedit = L"";
} // namespace GlobalIme

namespace Global
{
inline thread_local HWND msgWndHandle = nullptr;
}
