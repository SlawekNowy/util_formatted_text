/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __UTIL_FORMATTED_TEXT_HPP__
#define __UTIL_FORMATTED_TEXT_HPP__

#include "util_formatted_text_config.hpp"
#include "util_formatted_text_line.hpp"
#include <sharedutils/util_shared_handle.hpp>
#include <vector>

#ifdef ENABLE_FORMATTED_TEXT_UNIT_TESTS
	#include <sstream>
#endif

namespace util
{
	namespace text
	{
		class AnchorPoint;
		class TextTag;
		class TextTagComponent;
		class FormattedText
			: public std::enable_shared_from_this<FormattedText>
		{
		public:
			static std::shared_ptr<FormattedText> Create(const std::string &text="");
			void AppendText(const std::string &text);
			bool InsertText(const std::string &text,LineIndex lineIdx,CharOffset charOffset=LAST_CHAR);
			void RemoveLine(LineIndex lineIdx);
			bool RemoveText(LineIndex lineIdx,CharOffset charOffset=0,TextLength len=UNTIL_THE_END);
			bool MoveText(LineIndex lineIdx,CharOffset startOffset,TextLength len,LineIndex targetLineIdx,CharOffset targetCharOffset=LAST_CHAR);
			void SetText(const std::string &text);
			std::string Substr(TextOffset startOffset,TextLength len) const;
			void Clear();
			util::TSharedHandle<AnchorPoint> CreateAnchorPoint(LineIndex lineIdx,CharOffset charOffset,bool allowOutOfBounds=false);

			std::string GetUnformattedText() const;
			std::string GetFormattedText() const;

			std::optional<TextOffset> GetTextCharOffset(LineIndex lineIdx,CharOffset charOffset) const;
			std::optional<std::pair<LineIndex,CharOffset>> GetRelativeCharOffset(TextOffset absCharOffset) const;
			std::optional<char> GetChar(LineIndex lineIdx,CharOffset charOffset) const;
			std::optional<char> GetChar(TextOffset absCharOffset) const;

			const std::vector<util::TSharedHandle<TextTag>> &GetTags() const;
			void SetTagsEnabled(bool tagsEnabled);
			bool AreTagsEnabled() const;
			void SetPreserveTagsOnLineRemoval(bool preserveTags);
			bool ShouldPreserveTagsOnLineRemoval() const;

#ifdef ENABLE_FORMATTED_TEXT_UNIT_TESTS
			void UnitTest();
			bool Validate(std::stringstream &msg) const;
			void DebugPrint(bool printTags=false) const;
#endif
		private:
			enum class StateFlags : uint8_t
			{
				None = 0u,
				TagsEnabled = 1u,
				PreserveTagsOnLineRemoval = TagsEnabled<<1u
			};
			FormattedText()=default;
			LineIndex InsertLine(FormattedTextLine &line,LineIndex lineIdx=LAST_LINE);

			void ParseTags(LineIndex lineIdx,CharOffset offset=0,TextLength len=UNTIL_THE_END);

			void ParseText(const std::string &text,std::vector<PFormattedTextLine> &outLines);
			std::vector<PFormattedTextLine> m_textLines = {};
			std::vector<util::TSharedHandle<TextTag>> m_tags = {};
			StateFlags m_stateFlags = static_cast<StateFlags>(
				static_cast<std::underlying_type_t<StateFlags>>(StateFlags::TagsEnabled) | static_cast<std::underlying_type_t<StateFlags>>(StateFlags::PreserveTagsOnLineRemoval)
			);
		};
	};
};

#endif
