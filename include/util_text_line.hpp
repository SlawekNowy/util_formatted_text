/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __UTIL_TEXT_LINE_HPP__
#define __UTIL_TEXT_LINE_HPP__

#include "util_formatted_text_config.hpp"
#include "util_formatted_text_types.hpp"
#include <sharedutils/util_utf8.hpp>
#include <string>
#include <string_view>
#include <sstream>
#include <memory>
#include <vector>
#include <optional>

namespace util
{
	namespace text
	{
		class FormattedText;
		class FormattedTextLine;
		class TextLine
		{
		public:
			enum class CharFlags : uint32_t
			{
				None = 0u,
				Tag = 1u,
				Newline = Tag<<1u
			};

			TextLine(const std::string &line="");
			TextLength GetLength() const;
			// Returns length including new-line character
			TextLength GetAbsLength() const;
			const util::Utf8String &GetText() const;
			int32_t At(CharOffset offset) const;
			std::optional<int32_t> GetChar(CharOffset offset) const;
			void Reserve(TextLength len);
			util::Utf8StringView Substr(CharOffset offset,TextLength len=UNTIL_THE_END) const;

			TextLine &operator=(const util::Utf8String &line);
            TextLine &operator=(const std::string &line) {return operator=(util::Utf8String{line});}
			bool operator==(const util::Utf8StringView &line);
			bool operator!=(const util::Utf8StringView &line);
			operator const util::Utf8String&() const;
			operator const char*() const;

#ifdef ENABLE_FORMATTED_TEXT_UNIT_TESTS
			bool Validate(std::stringstream &msg) const;
#endif
		protected:
			std::optional<TextLength> Erase(CharOffset startOffset,TextLength len=UNTIL_THE_END,util::Utf8String *outErasedString=nullptr);
			bool CanErase(CharOffset startOffset,TextLength len=UNTIL_THE_END) const;
			void Clear();
			void AppendCharacter(int32_t c);
			bool InsertString(const util::Utf8StringView &str,CharOffset charOffset);
			friend FormattedTextLine;
			friend FormattedText;
		private:
			util::Utf8String m_line = "";
			std::vector<CharFlags> m_charFlags = {};
		};
		using PTextLine = std::shared_ptr<TextLine>;
	};
};

#endif
