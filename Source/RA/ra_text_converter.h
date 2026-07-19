#ifndef XM8_RA_TEXT_CONVERTER_H
#define XM8_RA_TEXT_CONVERTER_H

#include <cstddef>
#include <string>

namespace Xm8Ra {

struct RaSanitizedText {
	std::string utf8;
	size_t replacement_count = 0;
};

class RaTextConverter {
public:
	// Validate RA-provided UTF-8 before the existing UTF-8 to Shift-JIS
	// converter sees it. Unsupported four-byte characters, controls, and
	// malformed sequences become one '?' per sequence.
	static RaSanitizedText SanitizeUtf8(const std::string& source);

	// Shift-JIS boundary helpers for clipping, wrapping, and scrolling text
	// rendered by the existing 8x16/16x16 KANJI ROM font.
	static bool IsSjisLeadByte(unsigned char ch);
	static size_t SjisCharBytes(const char *source, size_t offset);
	static int SjisCharWidth(const char *source, size_t offset);
	static int SjisTextWidth(const char *source);
	static size_t SjisCharCount(const char *source);
	static size_t SjisByteOffsetForChar(const char *source,
		size_t char_index);
	static std::string SjisPrefix(const std::string& source,
		size_t max_bytes);
};

} // namespace Xm8Ra

#endif
