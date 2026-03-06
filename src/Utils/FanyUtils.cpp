#include <algorithm>
#include "Define.h"
#include "Globals.h"
#include "FanyUtils.h"
#include <utf8cpp/utf8.h>
#include <fmt/xchar.h>

using namespace std;

namespace FanyUtils
{
std::string GetIMEDataDirPath()
{
    const char *localAppDataPath = std::getenv("LOCALAPPDATA");
    std::string IMEDataPath = std::string(localAppDataPath) + "\\" + wstring_to_string(std::wstring(IME_NAME));
    return IMEDataPath;
}

void SendKeys(std::wstring pinyin)
{
    for (wchar_t ch : pinyin)
    {
        INPUT in[2]{};

        in[0].type = INPUT_KEYBOARD;
        in[0].ki.wScan = ch;
        in[0].ki.dwFlags = KEYEVENTF_UNICODE;

        in[1] = in[0];
        in[1].ki.dwFlags |= KEYEVENTF_KEYUP;

        UINT sent = SendInput(2, in, sizeof(INPUT));
        if (sent != 2)
        {
            OutputDebugString(fmt::format(L"SendKeys failed.\n").c_str());
        }
    }
}

std::wstring string_to_wstring(const std::string &str)
{
    std::u16string utf16result;
    utf8::utf8to16(str.begin(), str.end(), std::back_inserter(utf16result));
    return std::wstring(utf16result.begin(), utf16result.end());
}

std::string wstring_to_string(const std::wstring &wstr)
{
    std::string result;
    utf8::utf16to8(wstr.begin(), wstr.end(), std::back_inserter(result));
    return result;
}

std::string to_lower_copy(const std::string &str)
{
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) { return std::tolower(c); });
    return result;
}

std::wstring GetCurrentProcessName()
{
    TCHAR fullPath[MAX_PATH] = {0};
    if (GetModuleFileName(NULL, fullPath, MAX_PATH) == 0)
        return L"";

    std::wstring wfullPath(fullPath);
    size_t pos = wfullPath.find_last_of(L"\\/");
    std::wstring wname = (pos != std::wstring::npos) ? wfullPath.substr(pos + 1) : wfullPath;
    return wname;
}

/**
 * @brief Count UTF-8 chars
 *
 * @param str
 * @return string::size_type
 */
string::size_type count_utf8_chars(const string &str)
{
    return utf8::distance(str.begin(), str.end());
}
} // namespace FanyUtils