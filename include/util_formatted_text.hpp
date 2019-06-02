/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __UTIL_FORMATTED_TEXT_HPP__
#define __UTIL_FORMATTED_TEXT_HPP__

#include "util_formatted_text_config.hpp"
#include "util_formatted_text_line.hpp"
#include <sharedutils/util_shared_handle.hpp>
#include <vector>
#include <string_view>
#include <functional>

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
			struct Callbacks
			{
				Callbacks()=default;
				Callbacks(const std::function<void(FormattedTextLine&)> &onLineAdded,const std::function<void(FormattedTextLine&)> &onLineRemoved,const std::function<void(FormattedTextLine&)> &onLineChanged)
					: onLineAdded{onLineAdded},onLineRemoved{onLineRemoved},onLineChanged{onLineChanged}
				{}
				std::function<void(FormattedTextLine&)> onLineAdded = nullptr;
				std::function<void(FormattedTextLine&)> onLineRemoved = nullptr;
				std::function<void(FormattedTextLine&)> onLineChanged = nullptr;
				std::function<void()> onTextCleared = nullptr;

				std::function<void(TextTag&)> onTagAdded = nullptr;
				std::function<void(TextTag&)> onTagRemoved = nullptr;
				std::function<void()> onTagsCleared = nullptr;
			};

			static std::shared_ptr<FormattedText> Create(const std::string_view &text="");
			virtual ~FormattedText()=default;
			void AppendText(const std::string_view &text);
			bool InsertText(const std::string_view &text,LineIndex lineIdx,CharOffset charOffset=LAST_CHAR);
			void AppendLine(const std::string_view &line);
			void PopFrontLine();
			void PopBackLine();
			void RemoveLine(LineIndex lineIdx);
			bool RemoveText(LineIndex lineIdx,CharOffset charOffset,TextLength len);
			bool RemoveText(TextOffset offset,TextLength len);
			bool MoveText(LineIndex lineIdx,CharOffset startOffset,TextLength len,LineIndex targetLineIdx,CharOffset targetCharOffset=LAST_CHAR);
			void SetText(const std::string_view &text);
			std::string Substr(TextOffset startOffset,TextLength len) const;
			void Clear();
			util::TSharedHandle<AnchorPoint> CreateAnchorPoint(LineIndex lineIdx,CharOffset charOffset,bool allowOutOfBounds=false);
			util::TSharedHandle<AnchorPoint> CreateAnchorPoint(TextOffset offset,bool allowOutOfBounds=false);
			bool operator==(const std::string_view &text) const;
			bool operator!=(const std::string_view &text) const;
			operator std::string() const;

			std::optional<TextOffset> GetFormattedTextOffset(TextOffset offset) const;
			std::optional<TextOffset> GetUnformattedTextOffset(TextOffset offset) const;

			const std::string &GetUnformattedText() const;
			const std::string &GetFormattedText() const;

			void SetCallbacks(const Callbacks &callbacks);

			std::optional<TextOffset> GetTextCharOffset(LineIndex lineIdx,CharOffset charOffset) const;
			std::optional<std::pair<LineIndex,CharOffset>> GetRelativeCharOffset(TextOffset absCharOffset) const;
			std::optional<char> GetChar(LineIndex lineIdx,CharOffset charOffset) const;
			std::optional<char> GetChar(TextOffset absCharOffset) const;
			const std::vector<PFormattedTextLine> &GetLines() const;
			FormattedTextLine *GetLine(LineIndex lineIdx) const;
			uint32_t GetLineCount() const;
			uint32_t GetCharCount() const;

			const std::vector<util::TSharedHandle<TextTag>> &GetTags() const;
			std::vector<util::TSharedHandle<TextTag>> &GetTags();
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
			void RemoveLine(LineIndex lineIdx,bool preserveTags);
			void RemoveEmptyTags(util::text::LineIndex lineIndex,bool fromEnd=false);
			TextOffset FindFirstVisibleChar(util::text::LineIndex lineIndex,bool fromEnd=false) const;

			void OnLineAdded(FormattedTextLine &line);
			void OnLineRemoved(FormattedTextLine &line);
			void OnLineChanged(FormattedTextLine &line);
			Callbacks m_callbacks = {};

			void ParseTags(LineIndex lineIdx,CharOffset offset=0,TextLength len=UNTIL_THE_END);
			void ParseText(const std::string_view &text,std::vector<PFormattedTextLine> &outLines);
			void UpdateTextInfo() const;
			void UpdateTextOffsets(LineIndex lineStartIdx=0);
			struct {
				uint32_t lineCount = 0u;
				TextLength charCount = 0u;
				std::string unformattedText = "";
				std::string formattedText = "";
			} mutable m_textInfo = {};
			mutable bool m_bDirty = true;
			std::vector<PFormattedTextLine> m_textLines = {};
			std::vector<LineIndex> m_formattedOffsetToLineIndex = {};
			std::vector<LineIndex> m_unformattedOffsetToLineIndex = {};
			std::vector<util::TSharedHandle<TextTag>> m_tags = {};
			StateFlags m_stateFlags = static_cast<StateFlags>(
				static_cast<std::underlying_type_t<StateFlags>>(StateFlags::TagsEnabled) | static_cast<std::underlying_type_t<StateFlags>>(StateFlags::PreserveTagsOnLineRemoval)
			);
		};
	};
};

#endif
