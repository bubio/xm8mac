#include "ra_text_converter.h"

#include <cstdint>

namespace Xm8Ra {

namespace {

bool IsContinuation(unsigned char ch)
{
	return (ch & 0xc0U) == 0x80U;
}

void AppendReplacement(RaSanitizedText *result, const std::string& source,
	size_t *offset, size_t sequence_bytes)
{
	result->utf8.push_back('?');
	++result->replacement_count;
	if (sequence_bytes != 0) {
		*offset += sequence_bytes;
		return;
	}
	++*offset;
	while (*offset < source.size() &&
		IsContinuation(static_cast<unsigned char>(source[*offset]))) {
		++*offset;
	}
}

bool IsControl(uint32_t codepoint)
{
	return codepoint < 0x20U || codepoint == 0x7fU ||
		(codepoint >= 0x80U && codepoint <= 0x9fU);
}

} // namespace

RaSanitizedText RaTextConverter::SanitizeUtf8(const std::string& source)
{
	RaSanitizedText result;
	result.utf8.reserve(source.size());

	for (size_t offset = 0; offset < source.size();) {
		const unsigned char first =
			static_cast<unsigned char>(source[offset]);
		if (first < 0x80U) {
			if (IsControl(first)) {
				AppendReplacement(&result, source, &offset, 1);
			}
			else {
				result.utf8.push_back(static_cast<char>(first));
				++offset;
			}
			continue;
		}

		if (first >= 0xc2U && first <= 0xdfU &&
			offset + 1 < source.size()) {
			const unsigned char second =
				static_cast<unsigned char>(source[offset + 1]);
			if (IsContinuation(second)) {
				const uint32_t codepoint =
					((first & 0x1fU) << 6) | (second & 0x3fU);
				if (IsControl(codepoint)) {
					AppendReplacement(&result, source, &offset, 2);
				}
				else {
					result.utf8.append(source, offset, 2);
					offset += 2;
				}
				continue;
			}
		}

		if (first >= 0xe0U && first <= 0xefU &&
			offset + 2 < source.size()) {
			const unsigned char second =
				static_cast<unsigned char>(source[offset + 1]);
			const unsigned char third =
				static_cast<unsigned char>(source[offset + 2]);
			const bool valid_second = IsContinuation(second) &&
				(first != 0xe0U || second >= 0xa0U) &&
				(first != 0xedU || second <= 0x9fU);
			if (valid_second && IsContinuation(third)) {
				result.utf8.append(source, offset, 3);
				offset += 3;
				continue;
			}
		}

		if (first >= 0xf0U && first <= 0xf4U &&
			offset + 3 < source.size()) {
			const unsigned char second =
				static_cast<unsigned char>(source[offset + 1]);
			const unsigned char third =
				static_cast<unsigned char>(source[offset + 2]);
			const unsigned char fourth =
				static_cast<unsigned char>(source[offset + 3]);
			const bool valid_second = IsContinuation(second) &&
				(first != 0xf0U || second >= 0x90U) &&
				(first != 0xf4U || second <= 0x8fU);
			if (valid_second && IsContinuation(third) &&
				IsContinuation(fourth)) {
				AppendReplacement(&result, source, &offset, 4);
				continue;
			}
		}

		AppendReplacement(&result, source, &offset, 0);
	}

	return result;
}

bool RaTextConverter::IsSjisLeadByte(unsigned char ch)
{
	return (ch >= 0x80U && ch < 0xa0U) || ch >= 0xe0U;
}

size_t RaTextConverter::SjisCharBytes(const char *source, size_t offset)
{
	if (source == nullptr || source[offset] == '\0') {
		return 0;
	}
	const unsigned char ch = static_cast<unsigned char>(source[offset]);
	return IsSjisLeadByte(ch) && source[offset + 1] != '\0' ? 2 : 1;
}

int RaTextConverter::SjisCharWidth(const char *source, size_t offset)
{
	return SjisCharBytes(source, offset) == 2 ? 16 :
		SjisCharBytes(source, offset) == 1 ? 8 : 0;
}

int RaTextConverter::SjisTextWidth(const char *source)
{
	if (source == nullptr) {
		return 0;
	}
	int width = 0;
	for (size_t offset = 0; source[offset] != '\0';) {
		width += SjisCharWidth(source, offset);
		offset += SjisCharBytes(source, offset);
	}
	return width;
}

size_t RaTextConverter::SjisCharCount(const char *source)
{
	if (source == nullptr) {
		return 0;
	}
	size_t count = 0;
	for (size_t offset = 0; source[offset] != '\0'; ++count) {
		offset += SjisCharBytes(source, offset);
	}
	return count;
}

size_t RaTextConverter::SjisByteOffsetForChar(const char *source,
	size_t char_index)
{
	if (source == nullptr) {
		return 0;
	}
	size_t offset = 0;
	for (size_t index = 0; index < char_index && source[offset] != '\0';
		++index) {
		offset += SjisCharBytes(source, offset);
	}
	return offset;
}

std::string RaTextConverter::SjisPrefix(const std::string& source,
	size_t max_bytes)
{
	size_t offset = 0;
	while (offset < source.size()) {
		const size_t char_bytes = SjisCharBytes(source.c_str(), offset);
		if (char_bytes == 0 || offset + char_bytes > source.size() ||
			offset + char_bytes > max_bytes) {
			break;
		}
		offset += char_bytes;
	}
	return source.substr(0, offset);
}

} // namespace Xm8Ra
