#include "m3u.h"

#include <fstream>
#include <iterator>
#include <vector>
#ifdef _WIN32
#include <filesystem>
#include <windows.h>
#endif
#include <string>
#include <cctype>
#if defined(__APPLE__) || (defined(__linux__) && !defined(__ANDROID__))
#include <iconv.h>
#endif
#ifdef __ANDROID__
#include "os.h"
#include "common.h"
#include "converter.h"
#endif

namespace {

std::string Trim(const std::string& value)
{
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }

    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

bool IsValidUtf8(const std::string& value)
{
    for (size_t i = 0; i < value.size();) {
        const unsigned char c = static_cast<unsigned char>(value[i]);
        size_t count = 0;
        if (c < 0x80) count = 1;
        else if (c >= 0xc2 && c <= 0xdf) count = 2;
        else if (c >= 0xe0 && c <= 0xef) count = 3;
        else if (c >= 0xf0 && c <= 0xf4) count = 4;
        else return false;
        if (i + count > value.size()) return false;
        for (size_t j = 1; j < count; ++j)
            if ((static_cast<unsigned char>(value[i + j]) & 0xc0) != 0x80) return false;
        i += count;
    }
    return true;
}

void AppendUtf8(std::string *out, uint32_t codepoint)
{
    if (codepoint <= 0x7f) out->push_back(static_cast<char>(codepoint));
    else if (codepoint <= 0x7ff) {
        out->push_back(static_cast<char>(0xc0 | (codepoint >> 6)));
        out->push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    } else if (codepoint <= 0xffff) {
        out->push_back(static_cast<char>(0xe0 | (codepoint >> 12)));
        out->push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
        out->push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    } else if (codepoint <= 0x10ffff) {
        out->push_back(static_cast<char>(0xf0 | (codepoint >> 18)));
        out->push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3f)));
        out->push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
        out->push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    }
}

bool DecodeUtf16(const std::string& bytes, bool little_endian, std::string *out)
{
    if ((bytes.size() & 1) != 0) return false;
    out->clear();
    for (size_t i = 0; i < bytes.size(); i += 2) {
        const unsigned char a = static_cast<unsigned char>(bytes[i]);
        const unsigned char b = static_cast<unsigned char>(bytes[i + 1]);
        uint32_t value = little_endian ? a | (b << 8) : (a << 8) | b;
        if (value >= 0xd800 && value <= 0xdbff) {
            if (i + 3 >= bytes.size()) return false;
            const unsigned char c = static_cast<unsigned char>(bytes[i + 2]);
            const unsigned char d = static_cast<unsigned char>(bytes[i + 3]);
            const uint32_t low = little_endian ? c | (d << 8) : (c << 8) | d;
            if (low < 0xdc00 || low > 0xdfff) return false;
            value = 0x10000 + ((value - 0xd800) << 10) + (low - 0xdc00);
            i += 2;
        } else if (value >= 0xdc00 && value <= 0xdfff) return false;
        AppendUtf8(out, value);
    }
    return true;
}

bool DecodeShiftJis(const std::string& bytes, std::string *out)
{
#ifdef _WIN32
    const int wide_length = MultiByteToWideChar(932, MB_ERR_INVALID_CHARS,
        bytes.data(), static_cast<int>(bytes.size()), NULL, 0);
    if (wide_length == 0) return false;
    std::vector<wchar_t> wide(wide_length);
    if (MultiByteToWideChar(932, MB_ERR_INVALID_CHARS, bytes.data(),
        static_cast<int>(bytes.size()), wide.data(), wide_length) == 0) return false;
    const int utf8_length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
        wide.data(), wide_length, NULL, 0, NULL, NULL);
    if (utf8_length == 0) return false;
    out->resize(utf8_length);
    return WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide.data(),
        wide_length, &(*out)[0], utf8_length, NULL, NULL) != 0;
#elif defined(__ANDROID__)
    // Android's NDK does not provide iconv. Reuse XM8's CP932 table, which
    // is already used for the application's Japanese file-name conversion.
    std::string terminated = bytes;
    terminated.push_back('\0');
    std::vector<char> converted(bytes.size() * 3 + 1);
    Converter converter;
    converter.SjisToUtf(terminated.c_str(), converted.data());
    *out = converted.data();
    return true;
#elif defined(__APPLE__) || defined(__linux__)
    iconv_t converter = iconv_open("UTF-8", "CP932");
    if (converter == reinterpret_cast<iconv_t>(-1)) return false;
    std::vector<char> converted(bytes.size() * 4 + 4);
    char *input = const_cast<char *>(bytes.data());
    size_t input_size = bytes.size();
    char *output = converted.data();
    size_t output_size = converted.size();
    const size_t result = iconv(converter, &input, &input_size, &output, &output_size);
    iconv_close(converter);
    if (result == static_cast<size_t>(-1) || input_size != 0) return false;
    out->assign(converted.data(), static_cast<size_t>(output - converted.data()));
    return true;
#else
    (void)bytes; (void)out;
    return false;
#endif
}

bool DecodePlaylist(const std::string& path, const std::string& bytes,
    std::string *text, std::string *error)
{
    if (bytes.size() >= 3 && bytes.compare(0, 3, "\xef\xbb\xbf") == 0) {
        *text = bytes.substr(3);
        if (IsValidUtf8(*text)) return true;
        *error = "invalid playlist text encoding: " + path;
        return false;
    }
    if (bytes.size() >= 2 && bytes.compare(0, 2, "\xff\xfe") == 0) {
        if (DecodeUtf16(bytes.substr(2), true, text)) return true;
        *error = "invalid playlist text encoding: " + path;
        return false;
    }
    if (bytes.size() >= 2 && bytes.compare(0, 2, "\xfe\xff") == 0) {
        if (DecodeUtf16(bytes.substr(2), false, text)) return true;
        *error = "invalid playlist text encoding: " + path;
        return false;
    }
    if (IsValidUtf8(bytes)) { *text = bytes; return true; }
    if (IsM3UPath(path) && path.size() >= 4 &&
        std::tolower(static_cast<unsigned char>(path[path.size() - 1])) != '8' &&
        DecodeShiftJis(bytes, text)) return true;
    *error = "invalid playlist text encoding: " + path;
    return false;
}

std::string ParentDirectory(const std::string& path)
{
    const std::string::size_type slash = path.find_last_of("/\\");
    if (slash == std::string::npos) {
        return "";
    }
    return path.substr(0, slash + 1);
}

std::string ResolveM3UEntry(const std::string& baseDir,
                            const std::string& entry)
{
    if (entry.empty()) {
        return entry;
    }

    if (entry[0] == '/') {
        return entry;
    }

    if (entry.size() >= 2 &&
        std::isalpha(static_cast<unsigned char>(entry[0])) &&
        entry[1] == ':') {
        return entry;
    }

    return baseDir + entry;
}

} // namespace

bool IsM3UPath(const std::string& path)
{
    const auto dot = path.find_last_of('.');
    if (dot == std::string::npos) return false;
    std::string extension = path.substr(dot);
    for (char& ch : extension) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return extension == ".m3u" || extension == ".m3u8";
}

M3UResult LoadM3U(const std::string& path)
{
    M3UResult result;
    result.success = false;

#ifdef _WIN32
    std::ifstream file(std::filesystem::u8path(path), std::ios::binary);
#else
    std::ifstream file(path, std::ios::binary);
#endif
    if (!file.is_open()) {
        result.error = "unable to open m3u: " + path;
        return result;
    }

    const std::string bytes((std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>());
    std::string text;
    if (!DecodePlaylist(path, bytes, &text, &result.error)) return result;

    const std::string baseDir = ParentDirectory(path);

    size_t line_start = 0;
    while (line_start <= text.size()) {
        const size_t line_end = text.find('\n', line_start);
        std::string line = text.substr(line_start, line_end - line_start);
        line_start = line_end == std::string::npos ? text.size() + 1 : line_end + 1;
        line = Trim(line);

        if (line.empty()) {
            continue;
        }

        if (line[0] == '#') {
            continue;
        }

        // 👇 resolve relative paths properly
        std::string resolved = ResolveM3UEntry(baseDir, line);

        result.entries.push_back(resolved);
    }

    result.success = true;
    return result;
}
