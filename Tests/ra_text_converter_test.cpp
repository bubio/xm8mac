#include "ra_text_converter.h"

#include <cassert>
#include <string>

int main()
{
	using Xm8Ra::RaTextConverter;

	auto text = RaTextConverter::SanitizeUtf8("Achievement 123");
	assert(text.utf8 == "Achievement 123");
	assert(text.replacement_count == 0);

	const std::string japanese = "実績解除";
	text = RaTextConverter::SanitizeUtf8(japanese);
	assert(text.utf8 == japanese);
	assert(text.replacement_count == 0);

	text = RaTextConverter::SanitizeUtf8("badge \xf0\x9f\x8f\x86");
	assert(text.utf8 == "badge ?");
	assert(text.replacement_count == 1);

	const std::string controls("A\0B\nC", 5);
	text = RaTextConverter::SanitizeUtf8(controls);
	assert(text.utf8 == "A?B?C");
	assert(text.replacement_count == 2);

	text = RaTextConverter::SanitizeUtf8(std::string("\xc0\xaf", 2));
	assert(text.utf8 == "?");
	assert(text.replacement_count == 1);

	text = RaTextConverter::SanitizeUtf8(std::string("\xed\xa0\x80", 3));
	assert(text.utf8 == "?");
	assert(text.replacement_count == 1);

	text = RaTextConverter::SanitizeUtf8(std::string("\xe3\x81", 2));
	assert(text.utf8 == "?");
	assert(text.replacement_count == 1);

	const char sjis[] = {'A', static_cast<char>(0x82),
		static_cast<char>(0xa0), static_cast<char>(0x82), '\0'};
	assert(RaTextConverter::SjisCharBytes(sjis, 0) == 1);
	assert(RaTextConverter::SjisCharBytes(sjis, 1) == 2);
	assert(RaTextConverter::SjisCharBytes(sjis, 3) == 1);
	assert(RaTextConverter::SjisTextWidth(sjis) == 32);
	assert(RaTextConverter::SjisCharCount(sjis) == 3);
	assert(RaTextConverter::SjisByteOffsetForChar(sjis, 0) == 0);
	assert(RaTextConverter::SjisByteOffsetForChar(sjis, 1) == 1);
	assert(RaTextConverter::SjisByteOffsetForChar(sjis, 2) == 3);
	assert(RaTextConverter::SjisByteOffsetForChar(sjis, 3) == 4);
	assert(RaTextConverter::SjisPrefix(sjis, 0).empty());
	assert(RaTextConverter::SjisPrefix(sjis, 1) == "A");
	assert(RaTextConverter::SjisPrefix(sjis, 2) == "A");
	assert(RaTextConverter::SjisPrefix(sjis, 3).size() == 3);
	assert(RaTextConverter::SjisPrefix(sjis, 4).size() == 4);

	return 0;
}
