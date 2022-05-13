/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __UTIL_FORMATTED_TEXT_LINE_HPP__
#define __UTIL_FORMATTED_TEXT_LINE_HPP__

#include "util_formatted_text_config.hpp"
#include "util_text_line.hpp"
#include <sharedutils/util_shared_handle.hpp>
#include <sharedutils/util_utf8.hpp>
#include <optional>
#include <memory>

namespace util
{
	namespace text
	{
		class FormattedText;
		class TextTagComponent;
		class LineStartAnchorPoint;
		class AnchorPoint;
		class FormattedTextLine
			: public std::enable_shared_from_this<FormattedTextLine>
		{
		public:
			static PFormattedTextLine Create(FormattedText &text,const std::string &line="");
			FormattedTextLine(const FormattedTextLine&)=delete;
			FormattedTextLine &operator=(const FormattedTextLine&)=delete;
			~FormattedTextLine();
			const TextLine &GetFormattedLine() const;
			TextLine &GetFormattedLine();

			const TextLine &GetUnformattedLine() const;
			TextLine &GetUnformattedLine();

			CharOffset AppendString(const util::Utf8StringView &str);
			std::optional<CharOffset> InsertString(const util::Utf8StringView &str,CharOffset charOffset);
			util::Utf8StringView Substr(CharOffset offset,TextLength len=UNTIL_THE_END) const;
			std::optional<TextLength> Erase(CharOffset startOffset,TextLength len=UNTIL_THE_END,util::Utf8String *outErasedString=nullptr);
			bool Move(CharOffset startOffset,TextLength len,FormattedTextLine &moveTarget,CharOffset targetCharOffset=LAST_CHAR);
			
			std::vector<util::TSharedHandle<TextTagComponent>> &GetTagComponents();
			const std::vector<util::TSharedHandle<TextTagComponent>> &GetTagComponents() const;
			std::vector<util::TWeakSharedHandle<AnchorPoint>> &GetAnchorPoints();
			const std::vector<util::TWeakSharedHandle<AnchorPoint>> &GetAnchorPoints() const;
			const LineStartAnchorPoint &GetStartAnchorPoint() const;
			LineStartAnchorPoint &GetStartAnchorPoint();
			LineIndex GetIndex() const;

			bool IsEmpty() const;
			TextOffset GetStartOffset() const;
			TextOffset GetEndOffset() const;
			TextOffset GetFormattedStartOffset() const;
			// End-offset including the new-line character
			TextOffset GetAbsEndOffset() const;
			TextLength GetAbsLength() const;
			TextLength GetLength() const;
			TextLength GetAbsFormattedLength() const;
			TextLength GetFormattedLength() const;
			std::optional<CharOffset> GetRelativeOffset(TextOffset offset) const;
			bool IsInRange(TextOffset offset,TextLength len=1) const;
			std::optional<char> GetChar(CharOffset offset) const;
			CharOffset GetFormattedCharOffset(CharOffset offset) const;
			CharOffset GetUnformattedCharOffset(CharOffset offset) const;
			util::TSharedHandle<AnchorPoint> CreateAnchorPoint(CharOffset charOffset,bool allowOutOfBounds=false);

			void AppendCharacter(int32_t c);
			TextLine &Format();
			FormattedText &GetTargetText() const;

#ifdef ENABLE_FORMATTED_TEXT_UNIT_TESTS
			bool Validate(std::stringstream &msg) const;
#endif
		protected:
			FormattedTextLine(FormattedText &text,const std::string &line="");
			void Initialize(LineIndex lineIndex);
			void SetIndex(LineIndex lineIndex);
			void DetachAnchorPoint(const AnchorPoint &anchorPoint);
			void AttachAnchorPoint(AnchorPoint &anchorPoint);

			// oldLineLen = original length of this line before the modification that caused the shift
			// was applied
			void ShiftAnchors(CharOffset startOffset,TextLength len,ShiftOffset shiftAmount,TextLength oldLineLen);
			std::vector<TSharedHandle<AnchorPoint>> DetachAnchorPoints(CharOffset startOffset,TextLength len=UNTIL_THE_END);
			void AttachAnchorPoints(std::vector<TSharedHandle<AnchorPoint>> &anchorPoints,ShiftOffset shiftOffset=0);
			util::TSharedHandle<TextTagComponent> ParseTagComponent(CharOffset offset,const util::Utf8StringView &str);
			friend FormattedText;
			friend AnchorPoint;
		private:
			FormattedText &m_text;
			util::TSharedHandle<LineStartAnchorPoint> m_startAnchorPoint = nullptr;
			TextOffset m_formattedStartOffset = 0;
			TextLine m_formattedLine;
			TextLine m_unformattedLine;
			std::vector<CharOffset> m_unformattedCharIndexToFormatted = {};
			std::vector<CharOffset> m_formattedCharIndexToUnformatted = {};
			LineIndex m_lineIndex = INVALID_LINE_INDEX;
			std::vector<util::TSharedHandle<TextTagComponent>> m_tagComponents = {};
			std::vector<util::TWeakSharedHandle<AnchorPoint>> m_anchorPoints = {};
			
			bool m_bDirty = false;
		};
	};
};

#endif
