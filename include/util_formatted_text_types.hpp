/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __UTIL_FORMATTED_TEXT_TYPES_HPP__
#define __UTIL_FORMATTED_TEXT_TYPES_HPP__

#include <cinttypes>
#include <limits>
#include <memory>

namespace util
{
	namespace text
	{
		using TextLength = size_t;
		using TextOffset = size_t;

		using LineIndex = uint32_t;
		using CharOffset = uint32_t;

		using ShiftOffset = int32_t;

		static const LineIndex LAST_LINE = std::numeric_limits<LineIndex>::max();
		static const LineIndex INVALID_LINE_INDEX = LAST_LINE;
		static const CharOffset LAST_CHAR = std::numeric_limits<CharOffset>::max();
		static const TextLength UNTIL_THE_END = std::numeric_limits<TextLength>::max();
		static const TextOffset END_OF_TEXT = std::numeric_limits<TextOffset>::max();

		class AnchorPoint;

		class FormattedTextLine;
		using PFormattedTextLine = std::shared_ptr<FormattedTextLine>;
	};
};

#endif
