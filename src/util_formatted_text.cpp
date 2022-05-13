/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "util_formatted_text.hpp"
#include "util_formatted_text_anchor_point.hpp"
#include "util_formatted_text_tag.hpp"
#include <sstream>
#include <cstring>
#include <cassert>
#include <algorithm>
#ifdef ENABLE_FORMATTED_TEXT_UNIT_TESTS
	#include <functional>
	#include <iostream>
	#include <unordered_set>
#endif

using namespace util::text;
#pragma optimize("",off)
std::shared_ptr<FormattedText> FormattedText::Create(const util::Utf8StringView &text)
{
	auto ftext = std::shared_ptr<FormattedText>{new FormattedText{}};
	ftext->AppendText(text);
	return ftext;
}

void FormattedText::SetText(const util::Utf8StringView &text)
{
	Clear();
	AppendText(text);
}
util::Utf8String FormattedText::Substr(TextOffset startOffset,TextLength len) const
{
	auto relOffset = GetRelativeCharOffset(startOffset);
	if(relOffset.has_value() == false)
		return "";
	util::Utf8String result = "";
	// result.reserve(len +m_textLines.size());
	auto lineIndex = relOffset->first;
	auto relCharOffset = relOffset->second;
	auto first = true;
	while(lineIndex < m_textLines.size() && len > 0)
	{
		if(first)
			first = false;
		else
		{
			--len;
			result += '\n';
		}
		auto &line = *m_textLines.at(lineIndex);

		auto substr = line.Substr(startOffset,len);
		result += substr;

		len -= substr.length();
		startOffset = 0;
		++lineIndex;
	}
	return result;
}
void FormattedText::Clear()
{
	m_textLines.clear();
	m_tags.clear();
	m_unformattedOffsetToLineIndex.clear();
	m_formattedOffsetToLineIndex.clear();
	m_bDirty = true;
	if(m_callbacks.onTextCleared)
		m_callbacks.onTextCleared();
	if(m_callbacks.onTagsCleared)
		m_callbacks.onTagsCleared();
}
bool FormattedText::operator==(const util::Utf8StringView &text) const
{
	TextOffset offset = 0;
	auto it = text.begin();
	for(auto lineIdx=decltype(m_textLines.size()){0u};lineIdx<m_textLines.size();++lineIdx)
	{
		auto &line = m_textLines.at(lineIdx);
		auto &lineText = line->GetUnformattedLine().GetText();
		if(offset >= text.length() || !util::utf8_strncmp(text.c_str() +offset,lineText.c_str(),lineText.length()))
			return false;
		offset += line->GetAbsLength();
		it += line->GetAbsLength();
		if(lineIdx == m_textLines.size() -1)
		{
			if((offset -1) != text.length())
				return false;
		}
	}
	return text.empty();
}
bool FormattedText::operator!=(const util::Utf8StringView &text) const {return operator==(text) == false;}
FormattedText::operator util::Utf8String() const {return GetUnformattedText();}
util::TSharedHandle<AnchorPoint> FormattedText::CreateAnchorPoint(LineIndex lineIdx,CharOffset charOffset,bool allowOutOfBounds)
{
	if(lineIdx >= m_textLines.size())
		return {};
	return m_textLines.at(lineIdx)->CreateAnchorPoint(charOffset,allowOutOfBounds);
}
util::TSharedHandle<AnchorPoint> FormattedText::CreateAnchorPoint(TextOffset offset,bool allowOutOfBounds)
{
	auto relOffset = GetRelativeCharOffset(offset);
	if(relOffset.has_value() == false)
		return nullptr;
	return CreateAnchorPoint(relOffset->first,relOffset->second,allowOutOfBounds);
}

std::optional<TextOffset> FormattedText::GetFormattedTextOffset(TextOffset offset) const
{
	/*if(offset >= m_unformattedOffsetToLineIndex.size())
		return {};
	auto lineIndex = m_unformattedOffsetToLineIndex.at(offset);
	auto &pLine = m_textLines.at(lineIndex);
	return pLine->GetFormattedCharOffset(offset -pLine->GetFormattedStartOffset());*/
	auto relOffset = GetRelativeCharOffset(offset);
	if(relOffset.has_value() == false)
		return {};
	auto &line = m_textLines.at(relOffset->first);
	return line->GetFormattedCharOffset(relOffset->second);
}
std::optional<TextOffset> FormattedText::GetUnformattedTextOffset(TextOffset offset) const
{
	/*if(offset >= m_formattedOffsetToLineIndex.size())
		return {};
	auto lineIndex = m_formattedOffsetToLineIndex.at(offset);
	auto &pLine = m_textLines.at(lineIndex);
	return pLine->GetUnformattedCharOffset(offset -pLine->GetStartOffset());*/
	for(auto &line : m_textLines)
	{
		auto len = line->GetAbsFormattedLength();
		if(offset >= len)
		{
			offset -= len;
			continue;
		}
		return line->GetStartOffset() +line->GetUnformattedCharOffset(offset);
	}
	return {};
}

const util::Utf8String &FormattedText::GetUnformattedText() const
{
	UpdateTextInfo();
	return m_textInfo.unformattedText;
}

const util::Utf8String &FormattedText::GetFormattedText() const
{
	UpdateTextInfo();
	return m_textInfo.formattedText;
}

void FormattedText::UpdateTextOffsets(LineIndex lineStartIdx)
{
	auto &lines = GetLines();
	TextOffset formattedOffset = 0;
	TextOffset unformattedOffset = 0;
	if(lineStartIdx > 0)
	{
		auto &prevLine = m_textLines.at(lineStartIdx -1);
		unformattedOffset = prevLine->GetStartOffset() +prevLine->GetAbsLength();
		formattedOffset = prevLine->GetFormattedStartOffset() +prevLine->GetAbsFormattedLength();
	}
	for(auto lineIndex=lineStartIdx;lineIndex<m_textLines.size();++lineIndex)
	{
		auto &line = m_textLines.at(lineIndex);
		line->m_formattedStartOffset = formattedOffset;
		auto len = line->GetAbsFormattedLength();
		auto end = formattedOffset +len;
		if(m_formattedOffsetToLineIndex.size() < end)
			m_formattedOffsetToLineIndex.resize(std::max(end,formattedOffset +500));
		std::fill(m_formattedOffsetToLineIndex.begin() +formattedOffset,m_formattedOffsetToLineIndex.begin() +formattedOffset +len,lineIndex);
		formattedOffset += len;

		auto startOffset = line->GetStartOffset();
		len = line->GetAbsLength();
		end = startOffset +len;
		if(m_unformattedOffsetToLineIndex.size() < end)
			m_unformattedOffsetToLineIndex.resize(std::max(end,startOffset +500));
		std::fill(m_unformattedOffsetToLineIndex.begin() +startOffset,m_unformattedOffsetToLineIndex.begin() +startOffset +len,lineIndex);
		unformattedOffset += len;
	}
	m_formattedOffsetToLineIndex.resize(formattedOffset);
	m_unformattedOffsetToLineIndex.resize(unformattedOffset);
}

void FormattedText::UpdateTextInfo() const
{
	if(m_bDirty == false)
		return;
	m_bDirty = false;
	m_textInfo.lineCount = 0u;
	m_textInfo.charCount = 0u;
	m_textInfo.unformattedText.clear();
	m_textInfo.formattedText.clear();
	auto &lines = GetLines();
	for(auto &line : lines)
	{
		++m_textInfo.lineCount;
		m_textInfo.charCount += line->GetAbsLength();
		m_textInfo.unformattedText += line->GetUnformattedLine().GetText();
		m_textInfo.formattedText += line->GetFormattedLine().GetText();
		if(m_textInfo.lineCount != lines.size())
		{
			m_textInfo.unformattedText += '\n';
			m_textInfo.formattedText += '\n';
		}
	}
}

std::optional<std::pair<LineIndex,CharOffset>> FormattedText::GetRelativeCharOffset(TextOffset absCharOffset) const
{
	if(absCharOffset >= m_unformattedOffsetToLineIndex.size())
		return {};
	auto lineIdx = m_unformattedOffsetToLineIndex.at(absCharOffset);
	auto &pLine = m_textLines.at(lineIdx);
	return {{lineIdx,absCharOffset -pLine->GetStartOffset()}};
}
std::optional<TextOffset> FormattedText::GetTextCharOffset(LineIndex lineIdx,CharOffset charOffset) const
{
	if(lineIdx >= m_textLines.size())
		return {};
	auto &line = m_textLines.at(lineIdx);
	if(charOffset >= line->GetAbsEndOffset())
		return {};
	return line->GetStartOffset() +charOffset;
}
std::optional<char> FormattedText::GetChar(LineIndex lineIdx,CharOffset charOffset) const
{
	if(lineIdx >= m_textLines.size())
		return {};
	auto &line = m_textLines.at(lineIdx);
	return line->GetChar(charOffset);
}
std::optional<char> FormattedText::GetChar(TextOffset absCharOffset) const
{
	auto relCharOffset = GetRelativeCharOffset(absCharOffset);
	if(relCharOffset.has_value() == false)
		return {};
	return GetChar(relCharOffset->first,relCharOffset->second);
}
const std::vector<PFormattedTextLine> &FormattedText::GetLines() const {return m_textLines;}
FormattedTextLine *FormattedText::GetLine(LineIndex lineIdx) const
{
	if(lineIdx >= m_textLines.size())
		return nullptr;
	return m_textLines.at(lineIdx).get();
}
uint32_t FormattedText::GetLineCount() const
{
	UpdateTextInfo();
	return m_textInfo.lineCount;
}
uint32_t FormattedText::GetCharCount() const
{
	UpdateTextInfo();
	return m_textInfo.charCount;
}
const std::vector<util::TSharedHandle<TextTag>> &FormattedText::GetTags() const {return const_cast<FormattedText*>(this)->GetTags();}
std::vector<util::TSharedHandle<TextTag>> &FormattedText::GetTags() {return m_tags;}
void FormattedText::SetTagsEnabled(bool tagsEnabled)
{
	if(tagsEnabled)
		m_stateFlags = static_cast<StateFlags>(static_cast<std::underlying_type_t<StateFlags>>(m_stateFlags) | static_cast<std::underlying_type_t<StateFlags>>(StateFlags::TagsEnabled));
	else
		m_stateFlags = static_cast<StateFlags>(static_cast<std::underlying_type_t<StateFlags>>(m_stateFlags) & ~static_cast<std::underlying_type_t<StateFlags>>(StateFlags::TagsEnabled));
}
bool FormattedText::AreTagsEnabled() const
{
	return (static_cast<std::underlying_type_t<StateFlags>>(m_stateFlags) &static_cast<std::underlying_type_t<StateFlags>>(StateFlags::TagsEnabled)) != 0;
}
void FormattedText::SetPreserveTagsOnLineRemoval(bool preserveTags)
{
	if(preserveTags)
		m_stateFlags = static_cast<StateFlags>(static_cast<std::underlying_type_t<StateFlags>>(m_stateFlags) | static_cast<std::underlying_type_t<StateFlags>>(StateFlags::PreserveTagsOnLineRemoval));
	else
		m_stateFlags = static_cast<StateFlags>(static_cast<std::underlying_type_t<StateFlags>>(m_stateFlags) & ~static_cast<std::underlying_type_t<StateFlags>>(StateFlags::PreserveTagsOnLineRemoval));
}
bool FormattedText::ShouldPreserveTagsOnLineRemoval() const
{
	return (static_cast<std::underlying_type_t<StateFlags>>(m_stateFlags) &static_cast<std::underlying_type_t<StateFlags>>(StateFlags::PreserveTagsOnLineRemoval)) != 0;
}

void FormattedText::RemoveLine(LineIndex lineIdx,bool preserveTags)
{
	if(lineIdx >= m_textLines.size())
		return;
	auto &line = *m_textLines.at(lineIdx);
	FormattedTextLine *nextLine = nullptr;
	if(lineIdx < m_textLines.size() -1)
		nextLine = m_textLines.at(lineIdx +1).get();

	if(lineIdx > 1)
	{
		// Link start anchor points of previous and next line
		auto &prevLine = *m_textLines.at(lineIdx -1);
		auto &prevLineAnchorStartPoint = prevLine.GetStartAnchorPoint();
		prevLineAnchorStartPoint.ClearPreviousLineAnchorStartPoint();
		if(nextLine)
			prevLineAnchorStartPoint.SetPreviousLineAnchorStartPoint(nextLine->GetStartAnchorPoint());
	}
	// If this line is the first line, the next line's start anchor point's parent will be
	// cleared automatically, so we don't have to do anything about it

	// Find tags located in removed line, these will have to be moved into the next
	// or previous line
	util::Utf8String strTagComponents {};
	if(preserveTags == true && ShouldPreserveTagsOnLineRemoval())
	{
		for(auto &tagComponent : line.GetTagComponents())
			strTagComponents += tagComponent->GetTagString(*this);

		while(!strTagComponents.empty() && strTagComponents.front() == '\n')
		{
			// TODO: This should never happen under normal circumstances, but due to a bug where the tag anchors can refer
			// to incorrect positions, it does happen sometimes, which can cause an infinite loop.
			// This is a temporary work-around until that bug is fixed.
			strTagComponents.erase(strTagComponents.begin());
		}
	}

	auto pline = m_textLines.at(lineIdx);
	m_textLines.erase(m_textLines.begin() +lineIdx);
	for(auto i=lineIdx;i<m_textLines.size();++i)
	{
		auto &line = *m_textLines.at(i);
		line.SetIndex(line.GetIndex() -1);
	}

	// The anchor points for the subsequent lines mustn't be updated before the line
	// has actually been removed, otherwise line references may be incorrect
	if(nextLine)
	{
		auto &nextLineAnchorStartPoint = nextLine->GetStartAnchorPoint();
		nextLineAnchorStartPoint.ShiftByOffset(-static_cast<ShiftOffset>(line.GetAbsLength()));
		auto prevLine = (lineIdx > 0) ? m_textLines.at(lineIdx -1) : nullptr;
		if(prevLine)
			nextLineAnchorStartPoint.SetPreviousLineAnchorStartPoint(prevLine->GetStartAnchorPoint());
	}
	
	UpdateTextOffsets(lineIdx);
	m_bDirty = true;
	OnLineRemoved(*pline);
	pline = nullptr; // Line has to be completely destroyed before tags are parsed again

	// ParseTags(lineIdx,0,1);

	// Move removed tags to next or previous line
	if(strTagComponents.empty())
		return;
	if(nextLine)
	{
		// Move to beginning of next line
		InsertText(strTagComponents,lineIdx,0);
		RemoveEmptyTags(lineIdx);
		return;
	}
	if(lineIdx == 0)
	{
		// There is no text left so there is no point in keeping
		// the tags around either.
		return;
	}
	auto prevLineIdx = lineIdx -1;
	// Move to end of previous line
	InsertText(strTagComponents,prevLineIdx);
	RemoveEmptyTags(prevLineIdx,true);
}

void FormattedText::RemoveLine(LineIndex lineIdx) {RemoveLine(lineIdx,true);}

TextOffset FormattedText::FindFirstVisibleChar(util::text::LineIndex lineIndex,bool fromEnd) const
{
	auto &line = m_textLines.at(lineIndex);
	// Find first visible character
	auto lineStartOffset = line->GetStartOffset();
	auto &tagComponents = line->GetTagComponents();
	auto &unformattedLine = line->GetUnformattedLine();
	auto len = unformattedLine.GetLength();
	if(fromEnd == false)
	{
		auto curTagIdx = 0u;
		TextOffset offset = 0u;
		while(offset < len)
		{
			if(curTagIdx < tagComponents.size())
			{
				auto &tagComponent = tagComponents.at(curTagIdx);
				auto startOffset = tagComponent->GetStartAnchorPoint()->GetTextCharOffset() -lineStartOffset;
				if(offset == startOffset)
				{
					offset += tagComponent->GetLength();
					++curTagIdx;
					continue;
				}
			}
			break; // Visible character found!
		}
		return offset;
	}
	auto curTagIdx = tagComponents.empty() ? 0 : (tagComponents.size() -1);
	TextOffset offset = (len > 0) ? (len -1) : 0;
	while(offset >= 0)
	{
		if(curTagIdx < tagComponents.size())
		{
			auto &tagComponent = tagComponents.at(curTagIdx);
			auto endOffset = tagComponent->GetEndAnchorPoint()->GetTextCharOffset() -lineStartOffset;
			if(offset == endOffset)
			{
				offset -= tagComponent->GetLength();
				--curTagIdx;
				continue;
			}
		}
		break; // Visible character found!
	}
	return offset;
}

void FormattedText::RemoveEmptyTags(util::text::LineIndex lineIndex,bool fromEnd)
{
	static auto skip = false;
	if(skip)
		return;
	// Remove all empty tags
	for(auto it=m_tags.begin();it!=m_tags.end();)
	{
		auto &hTag = *it;
		if(hTag->IsValid() == false)
		{
			if(m_callbacks.onTagRemoved)
				m_callbacks.onTagRemoved(*hTag);
			it = m_tags.erase(it);
			continue;
		}
		if(hTag->IsClosed() == false)
		{
			++it;
			continue;
		}
		auto innerRange = hTag->GetInnerRange();
		if(innerRange.has_value() == false)
		{
			// Tag is completely empty, we can just remove it in one step
			auto outerRange = hTag->GetOuterRange();
			auto result = outerRange.has_value();
			if(result == true)
			{
				assert(outerRange->second >= outerRange->first);
				if(outerRange->second >= outerRange->first)
				{
					skip = true;
					result = RemoveText(outerRange->first,outerRange->second -outerRange->first);
					skip = false;
				}
			}
			if(result)
				RemoveEmptyTags(lineIndex); // Restart in case m_tags array has been changed
			return;
		}
		else
		{
			auto &line = m_textLines.at(lineIndex);
			auto offset = FindFirstVisibleChar(lineIndex,fromEnd);
			auto rangeEndOffset = innerRange->first +innerRange->second -1;

			// Check if there are any visible characters within the tag
			if(offset <= rangeEndOffset)
			{
				// There are visible characters within the tag, which means we'll have to
				// keep the tag
				++it;
				continue;
			}
			if(offset > rangeEndOffset || line->GetFormattedLength() == 0)
			{
				// There are no visible characters within the tag.

				// Opening and closing tags have to be removed separately, because there may be
				// other tags located between them! The closing tag will be removed first, because
				// otherwise the tag would become invalid.
				auto hTagCpy = hTag;
				auto &closingTag = *hTag->GetClosingTagComponent();
				auto closingTagStartOffset = closingTag.GetStartAnchorPoint()->GetTextCharOffset();
				auto closingTagEndOffset = closingTag.GetEndAnchorPoint()->GetTextCharOffset();

				auto &openingTag = *hTag->GetOpeningTagComponent();
				auto openingTagStartOffset = openingTag.GetStartAnchorPoint()->GetTextCharOffset();
				auto openingTagEndOffset = openingTag.GetEndAnchorPoint()->GetTextCharOffset();

				skip = true;
				// Closing tag has to be removed first to make sure the offsets don't become invalid
				auto result = RemoveText(closingTagStartOffset,closingTagEndOffset -closingTagStartOffset +1) == true && RemoveText(openingTagStartOffset,openingTagEndOffset -openingTagStartOffset +1);
				skip = false;
				if(result)
					RemoveEmptyTags(lineIndex); // Restart in case m_tags array has been changed
				return;
			}
		}
		++it;
	}
}

bool FormattedText::RemoveText(TextOffset offset,TextLength len)
{
	if(len == 0)
		return true;
	auto endOffset = offset +len -1;
	auto relStartOffset = GetRelativeCharOffset(offset);
	auto relEndOffset = GetRelativeCharOffset(endOffset);
	if(relStartOffset.has_value() == false || relEndOffset.has_value() == false)
		return false;
	auto endLineIdx = relEndOffset->first;
	for(auto lineIdx=relStartOffset->first +1;lineIdx<relEndOffset->first;++lineIdx)
	{
		RemoveLine(lineIdx,false);
		--endLineIdx;
	}
	if(endLineIdx != relStartOffset->first && RemoveText(endLineIdx,0,relEndOffset->second +1) == false)
		return false;
	return RemoveText(relStartOffset->first,relStartOffset->second,len);
}

bool FormattedText::RemoveText(LineIndex lineIdx,CharOffset charOffset,TextLength len)
{
	if(lineIdx >= m_textLines.size())
		return false;
	auto &line = *m_textLines.at(lineIdx);
	auto &unformattedLine = line.GetUnformattedLine();
	if(charOffset >= unformattedLine.GetLength() || unformattedLine.CanErase(charOffset,len) == false)
		return false;
	len = (len == UNTIL_THE_END) ? (line.GetAbsLength() -charOffset) : len;
	auto removedTextEndOffset = charOffset +len -1;
	if((removedTextEndOffset +1) >= line.GetAbsLength())
	{
		// Deleted text includes new-line character. If the start offset of the deleted range is 0, the entire
		// line will have to be deleted, otherwise the next line will have to be appened to this one.
		if(charOffset == 0)
		{
			// Range covers entire line, just delete it completely
			RemoveLine(lineIdx,false);
			return true;
		}
		auto lineLen = line.GetLength();
		if(charOffset < lineLen)
		{
			// Delete text range BEFORE new-line character
			if(RemoveText(lineIdx,charOffset,lineLen -charOffset) == false)
				return false;
		}
		auto nextLineIdx = lineIdx +1;
		if(nextLineIdx >= m_textLines.size())
			return true; // There is no next line, so there is nothing left to do
		auto &nextLine = *m_textLines.at(nextLineIdx);
		// Move next line into this one
		return MoveText(nextLineIdx,0,nextLine.GetAbsLength(),lineIdx,charOffset);
	}

	auto numErased = line.Erase(charOffset,len);
	if(numErased.has_value() == false)
		throw std::logic_error{"Discrepancy: Erasing failed, but 'CanErase' returned true."};
	UpdateTextOffsets(lineIdx);
	ParseTags(lineIdx,charOffset,1);
	m_bDirty = true;
	OnLineChanged(line);
	return numErased.has_value();
}

bool FormattedText::MoveText(LineIndex lineIdx,CharOffset startOffset,TextLength len,LineIndex targetLineIdx,CharOffset targetCharOffset)
{
	if(len == 0)
		return true;
	if(lineIdx == targetLineIdx && targetCharOffset > startOffset && targetCharOffset <= startOffset +len -1)
		return false; // Target index is located within the range that should be moved
	if(lineIdx >= m_textLines.size() || targetLineIdx >= m_textLines.size())
		return false;
	// Create temporary anchor to keep track of the target line (in case the line is changed)
	auto tgtAnchor = CreateAnchorPoint(targetLineIdx,0,true);
	if(tgtAnchor.IsExpired())
		return false;
	auto &lineSrc = *m_textLines.at(lineIdx);

	auto absStartOffset = *GetTextCharOffset(lineIdx,startOffset);
	auto srcAnchorPoints = lineSrc.DetachAnchorPoints(startOffset,len);
	std::vector<CharOffset> anchorPointOffsets {};
	anchorPointOffsets.reserve(srcAnchorPoints.size());
	for(auto &hAnchorPoint : srcAnchorPoints)
		anchorPointOffsets.push_back(hAnchorPoint->GetTextCharOffset() -absStartOffset);

	auto text = lineSrc.Substr(startOffset,len).to_str();
	auto tgtAnchorOffset = tgtAnchor->GetTextCharOffset();

	if(RemoveText(lineIdx,startOffset,len) == false || tgtAnchor.IsExpired())
		return false;

	if(targetLineIdx == lineIdx && targetCharOffset > startOffset)
		targetCharOffset -= len; // Target offset has changed if the target line is the same as the source line
	auto *lineTgt = &tgtAnchor->GetLine();
	targetLineIdx = lineTgt->GetIndex();

	if(InsertText(text,targetLineIdx,targetCharOffset) == false || tgtAnchor.IsExpired())
		return false;
	lineTgt = &tgtAnchor->GetLine();

	auto newTgtAnchorOffset = tgtAnchor->GetTextCharOffset();
	auto shiftAmount = static_cast<ShiftOffset>(newTgtAnchorOffset) -static_cast<ShiftOffset>(tgtAnchorOffset);
	lineTgt->AttachAnchorPoints(srcAnchorPoints,shiftAmount);

	auto newAbsStartOffset = *GetTextCharOffset(targetLineIdx,targetCharOffset);
	for(auto i=0;i<srcAnchorPoints.size();++i)
		srcAnchorPoints.at(i)->SetOffset(newAbsStartOffset +anchorPointOffsets.at(i));
	return true;
}

void FormattedText::SetCallbacks(const Callbacks &callbacks) {m_callbacks = callbacks;}
void FormattedText::OnLineAdded(FormattedTextLine &line)
{
	if(m_callbacks.onLineAdded)
		m_callbacks.onLineAdded(line);
}
void FormattedText::OnLineRemoved(FormattedTextLine &line)
{
	if(m_callbacks.onLineRemoved)
		m_callbacks.onLineRemoved(line);
}
void FormattedText::OnLineChanged(FormattedTextLine &line)
{
	if(m_callbacks.onLineChanged)
		m_callbacks.onLineChanged(line);
}

LineIndex FormattedText::InsertLine(FormattedTextLine &line,LineIndex lineIdx)
{
	if(lineIdx > m_textLines.size())
		lineIdx = m_textLines.size();
	line.Initialize(lineIdx);
	auto &startAnchorPoint = line.GetStartAnchorPoint();

	// Set parent anchor point
	if(lineIdx > 0)
	{
		auto &prevLine = *m_textLines.at(lineIdx -1);
		startAnchorPoint.SetPreviousLineAnchorStartPoint(prevLine.GetStartAnchorPoint());
		startAnchorPoint.SetOffset(prevLine.GetAbsEndOffset() +1);
	}
	else
	{
		startAnchorPoint.ClearParent();
		startAnchorPoint.SetOffset(0);
	}
	// Usually SetOffset would already assign the correct line, however
	// this does not work here because the line hasn't been added to m_textLines yet,
	// so we have to assign the line manually.
	startAnchorPoint.SetLine(line);
	
	// Set child anchor point
	if(lineIdx < m_textLines.size())
	{
		auto &nextLine = *m_textLines.at(lineIdx);
		auto &nextAnchorPoint = nextLine.GetStartAnchorPoint();
		nextAnchorPoint.SetPreviousLineAnchorStartPoint(line.GetStartAnchorPoint());
		nextAnchorPoint.ShiftToOffset(line.GetAbsEndOffset() +1);
	}
	m_textLines.insert(m_textLines.begin() +lineIdx,line.shared_from_this());
	for(auto it=m_textLines.begin() +lineIdx +1;it!=m_textLines.end();++it)
	{
		auto &line = **it;
		line.SetIndex(line.GetIndex() +1);
	}
	UpdateTextOffsets(lineIdx);
	ParseTags(lineIdx);
	m_bDirty = true;
	OnLineAdded(*m_textLines.at(lineIdx));
	return lineIdx;
}
void FormattedText::AppendText(const util::Utf8StringView &text) {InsertText(text,m_textLines.empty() ? LAST_LINE : (m_textLines.size() -1),LAST_CHAR);}
void FormattedText::AppendLine(const util::Utf8StringView &line)
{
	auto strLine = line.to_str();
	if(m_textLines.empty() == false)
		strLine = util::Utf8String{"\n"} +strLine;
	InsertText(strLine,m_textLines.size());
}
void FormattedText::PopFrontLine() {RemoveLine(0);}
void FormattedText::PopBackLine()
{
	if(m_textLines.empty())
		return;
	RemoveLine(m_textLines.size() -1);
}

bool FormattedText::InsertText(const util::Utf8StringView &text,LineIndex lineIdx,CharOffset charOffset)
{
	if(text.empty())
		return true;
	std::vector<PFormattedTextLine> lines {};
	ParseText(text,lines);
	if(lines.empty())
		return true;
	while(m_textLines.size() +lines.size() > m_maxLineCount)
		PopFrontLine(); // TODO: This may cause an infinite loop in some cases?
	if(lineIdx == LAST_LINE)
		lineIdx = m_textLines.size();
	if(lineIdx > m_textLines.size() || (lineIdx == m_textLines.size() && charOffset != LAST_CHAR))
		return false;
	if(lineIdx == m_textLines.size())
	{
		auto newLine = FormattedTextLine::Create(*this);
		InsertLine(*newLine,LAST_LINE);
	}
	auto &firstLineToInsert = lines.front();
	
	auto postfix = m_textLines.at(lineIdx)->Substr(charOffset).to_str();
	auto &targetLineToInsert = m_textLines.at(lineIdx);
	auto anchorPointsInMoveRange = targetLineToInsert->DetachAnchorPoints(charOffset,UNTIL_THE_END);

	auto numErased = targetLineToInsert->Erase(charOffset);
	m_bDirty = true;
	//ParseTags(lineIdx,charOffset,numErased.has_value() ? *numErased : 0);

	auto textToInsert = firstLineToInsert->GetUnformattedLine().GetText();
	auto insertedCharOffset = targetLineToInsert->InsertString(textToInsert,charOffset);
	if(insertedCharOffset.has_value() == false)
		return false;

	UpdateTextOffsets(lineIdx);
	ParseTags(lineIdx);//,*insertedCharOffset,textToInsert.length());

	OnLineChanged(*targetLineToInsert);

	m_textLines.reserve(m_textLines.size() +lines.size() -1);
	auto lineIndexOffset = lineIdx +1;
	for(auto it=lines.begin() +1;it!=lines.end();++it)
		InsertLine(**it,lineIndexOffset++);

	auto lastInsertedLineIdx = lineIdx +lines.size() -1;
	auto &lastInsertedLine = m_textLines.at(lastInsertedLineIdx);
	auto insertOffset = lastInsertedLine->AppendString(postfix);
	lastInsertedLine->AttachAnchorPoints(anchorPointsInMoveRange,text.length());
	if(postfix.empty() == false)
		UpdateTextOffsets(lastInsertedLineIdx);
	ParseTags(lastInsertedLineIdx,insertOffset);

	OnLineChanged(*lastInsertedLine);
	return true;
}

void FormattedText::ParseText(const util::Utf8StringView &text,std::vector<PFormattedTextLine> &outLines)
{
	if(text.empty())
		return;
	outLines.push_back(FormattedTextLine::Create(*this));
	auto pCurLine = outLines.back();
	TextOffset offset = 0;
	auto it = text.begin();
	while(offset < text.length())
	{
		auto c = *it;
		switch(c)
		{
			case '\n':
				outLines.push_back(FormattedTextLine::Create(*this));
				pCurLine = outLines.back();
				break;
			default:
				pCurLine->AppendCharacter(c);
				break;
		}
		++offset;
		++it;
	}
}

#ifdef ENABLE_FORMATTED_TEXT_UNIT_TESTS
void FormattedText::UnitTest()
{
	struct TagInfo
	{
		TagInfo(const std::string &tagName,const std::string &contents,const std::string &label="",const std::vector<std::string> &attributes={},bool isClosed=true)
			: tagName{tagName},contents{contents},label{label},attributes{attributes},isClosed{isClosed}
		{}
		std::string tagName;
		std::string label;
		std::vector<std::string> attributes;
		std::string contents;
		bool isClosed = true;
	};

	auto allSucceeded = true;
	util::Utf8StringView currentUnitTestName = {};

	const auto print_tags = [this]() {
		for(auto &tag : m_tags)
		{
			if(tag->IsValid() == false)
				continue;
			std::cout<<"--- Tag Info ---\n";
			std::cout<<"Tag String: "<<tag->GetTagString()<<std::endl;
			std::cout<<"Tag Contents: "<<tag->GetTagContents()<<std::endl;
			std::cout<<"Is closed: "<<(tag->IsClosed() ? "Yes" : "No")<<std::endl;
			auto *pOpeningTag = tag->GetOpeningTagComponent();
			std::cout<<"Opening tag name: "<<pOpeningTag->GetTagName()<<std::endl;
			std::cout<<"Opening tag label: "<<pOpeningTag->GetLabel()<<std::endl;
			std::cout<<"Opening tag string: "<<pOpeningTag->GetTagString(*this)<<std::endl;
			auto *pClosingTag = tag->GetClosingTagComponent();
			if(pClosingTag)
			{
				std::cout<<"Closing tag name: "<<pClosingTag->GetTagName()<<std::endl;
				std::cout<<"Closing tag string: "<<pClosingTag->GetTagString(*this)<<std::endl;
			}
			std::cout<<"----------------\n";
		}
	};

	const auto validate = [this,&allSucceeded,&currentUnitTestName]() -> bool {
		std::stringstream msg {};
		if(Validate(msg) == true)
			return true;
		allSucceeded = false;
		std::cout<<"Validation for unit test '"<<currentUnitTestName<<"' has failed:\n"<<msg.str()<<std::endl;
		// DebugPrint();
		return false;
	};
	const auto validate_tags = [this,print_tags](const std::vector<TagInfo> &tagInfos,std::stringstream &msg) -> bool {
		auto tagIdx = 0u;
		if(m_tags.empty() && tagInfos.empty() == false)
		{
			msg<<"Expected number of tags does not match actual number of tags!";
			return false;
		}
		for(auto &tag : m_tags)
		{
			if(tag.IsExpired())
				continue;
			auto *pOpeningTagComponent = tag->GetOpeningTagComponent();
			auto *pClosingTagComponent = tag->GetClosingTagComponent();
			if(tagIdx >= tagInfos.size())
			{
				msg<<"Expected number of tags does not match actual number of tags!";
				return false;
			}
			auto &tagInfo = tagInfos.at(tagIdx);
			if(tagInfo.tagName != pOpeningTagComponent->GetTagName())
			{
				msg<<"Unexpected tag of type '"<<tagInfo.tagName<<"'!";
				return false;
			}
			if(tagInfo.isClosed && pClosingTagComponent == nullptr)
			{
				msg<<"Expected tag '"<<tagInfo.tagName<<"' to be closed, but it's not!";
				return false;
			}
			if(tagInfo.attributes != pOpeningTagComponent->GetTagAttributes())
			{
				msg<<"Expected tag attributes do not match actual attributes!";
				return false;
			}
			if(tagInfo.label != pOpeningTagComponent->GetLabel())
			{
				msg<<"Expected tag label '"<<tagInfo.label<<"' does not match actual label '"<<pOpeningTagComponent->GetLabel()<<"'!";
				return false;
			}
			if(tagInfo.contents != tag->GetTagContents())
			{
				msg<<"Tag contents do not match expected contents!";
				return false;
			}
			++tagIdx;
		}
		return true;
	};
	const auto unit_test = [this,&allSucceeded,&validate,&currentUnitTestName,&print_tags](const std::string &unitTestName,const std::function<bool(std::stringstream&)> &test) {
		Clear();
		currentUnitTestName = unitTestName;
		std::stringstream msg {};
		if(test(msg) == false)
		{
			allSucceeded = false;
			std::cout<<"Unit test '"<<unitTestName<<"' has failed:\n"<<msg.str()<<std::endl;
			DebugPrint(true);
			std::cout<<"Tags:"<<std::endl;
			print_tags();
			return;
		}
		validate();
	};

	const auto assert_anchor_point = [this](std::stringstream &msg,const TSharedHandle<AnchorPoint> &pAnchorPoint,LineIndex lineIndex,CharOffset charOffset) -> bool {
		if(pAnchorPoint.IsExpired())
		{
			msg<<"Expected anchor point to be in line "<<lineIndex<<", character "<<charOffset<<", but is expired!";
			return false;
		}
		if(pAnchorPoint->IsValid() == false)
		{
			msg<<"Expected anchor point to be in line "<<lineIndex<<", character "<<charOffset<<", but is invalid!";
			return false;
		}
		auto &anchorPoint = *pAnchorPoint;
		if(anchorPoint.GetLineIndex() != lineIndex)
		{
			msg<<"Expected anchor point to be in line "<<lineIndex<<", but is in line "<<anchorPoint.GetLineIndex();
			return false;
		}
		auto textOffset = GetTextCharOffset(lineIndex,charOffset);
		if(textOffset.has_value() && anchorPoint.GetTextCharOffset() == *textOffset)
			return true;
		msg<<"Expected anchor point to be at character "<<charOffset<<" of line "<<lineIndex<<", but is at ";
		auto &line = anchorPoint.GetLine();
		auto lineOffset = line.GetRelativeOffset(anchorPoint.GetTextCharOffset());
		if(lineOffset.has_value())
			msg<<"character "<<*lineOffset;
		else
			msg<<"out-of-range character (absolute offset: "<<anchorPoint.GetTextCharOffset()<<")";
		msg<<" of line "<<line.GetIndex()<<"!";
		return false;
	};
	const auto assert_invalid_anchor_point = [this](std::stringstream &msg,const TSharedHandle<AnchorPoint> &pAnchorPoint) -> bool {
		if(pAnchorPoint.IsExpired())
			return true;
		msg<<"Expected anchor point to be invalid, but is valid!";
		return false;
	};

	unit_test("LineAppend",[this,&validate](std::stringstream &msg) -> bool {
		AppendText("Hello\n");
		if(validate() == false) return false;
		AppendText("World");
		auto text = GetUnformattedText();
		auto *expected = "Hello\nWorld";
		if(text != expected)
		{
			msg<<"Expected '"<<expected<<"', got: '"<<text<<"'!\n";
			return false;
		}
		return true;
	});

	unit_test("TextInsert",[this,&validate](std::stringstream &msg) -> bool {
		AppendText("Hello");
		if(validate() == false) return false;
		AppendText("World");
		auto text = GetUnformattedText();
		auto *expected = "HelloWorld";
		if(text != expected)
		{
			msg<<"Expected '"<<expected<<"', got: '"<<text<<"'!\n";
			return false;
		}
		return true;
	});

	unit_test("TextInsertAtLineEnd",[this,&validate](std::stringstream &msg) -> bool {
		AppendText("Hello\n");
		if(validate() == false) return false;
		AppendText("World");
		if(validate() == false) return false;
		InsertText("Ab\ncd",1);
		auto text = GetUnformattedText();
		auto *expected = "Hello\nWorldAb\ncd";
		if(text != expected)
		{
			msg<<"Expected '"<<expected<<"', got: '"<<text<<"'!\n";
			return false;
		}
		return true;
	});

	unit_test("TextInsertAtLineStart",[this,&assert_anchor_point,&validate](std::stringstream &msg) -> bool {
		AppendText("ABC\n");
		if(validate() == false) return false;
		AppendText("JKL");
		auto refPoint = CreateAnchorPoint(1,2u);
		if(validate() == false) return false;
		InsertText("DEF\nGHI",1,0);
		if(validate() == false) return false;
		auto text = GetUnformattedText();
		auto *expected = "ABC\nDEF\nGHIJKL";
		if(text != expected)
		{
			msg<<"Expected '"<<expected<<"', got: '"<<text<<"'!\n";
			return false;
		}
		return assert_anchor_point(msg,refPoint,2,5);
	});

	unit_test("TextInsertAtLineMiddle",[this,&assert_anchor_point,&validate](std::stringstream &msg) -> bool {
		AppendText("ABC\n");
		if(validate() == false) return false;
		AppendText("JKLMNO");
		auto refPoint0 = CreateAnchorPoint(1,1u);
		auto refPoint1 = CreateAnchorPoint(1,4u);
		if(validate() == false) return false;
		InsertText("DEF\nGHI",1,3);
		auto text = GetUnformattedText();
		auto *expected = "ABC\nJKLDEF\nGHIMNO";
		if(text != expected)
		{
			msg<<"Expected '"<<expected<<"', got: '"<<text<<"'!\n";
			return false;
		}
		return assert_anchor_point(msg,refPoint0,1,1) && assert_anchor_point(msg,refPoint1,2,4);
	});

	unit_test("LineRemoveFirst",[this,&assert_anchor_point,&assert_invalid_anchor_point,&validate](std::stringstream &msg) -> bool {
		AppendText("Abc\n");
		auto refPoint0 = CreateAnchorPoint(0,1u);
		if(validate() == false) return false;
		AppendText("Def\n");
		auto refPoint1 = CreateAnchorPoint(1,2u);
		if(validate() == false) return false;
		AppendText("Ghi");
		auto refPoint2 = CreateAnchorPoint(2,3u);
		if(validate() == false) return false;
		RemoveLine(0);
		auto text = GetUnformattedText();
		auto *expected = "Def\nGhi";
		if(text != expected)
		{
			msg<<"Expected '"<<expected<<"', got: '"<<text<<"'!\n";
			return false;
		}
		return assert_invalid_anchor_point(msg,refPoint0) && assert_anchor_point(msg,refPoint1,0,2) && assert_invalid_anchor_point(msg,refPoint2);
	});

	unit_test("LineRemoveMiddle",[this,&assert_anchor_point,&assert_invalid_anchor_point,&validate](std::stringstream &msg) -> bool {
		AppendText("Abc\n");
		auto refPoint0 = CreateAnchorPoint(0,0u);
		if(validate() == false) return false;
		AppendText("Def\n");
		auto refPoint1 = CreateAnchorPoint(1,0u);
		if(validate() == false) return false;
		AppendText("Ghi");
		auto refPoint2 = CreateAnchorPoint(2,0u);
		if(validate() == false) return false;
		RemoveLine(1);
		if(validate() == false) return false;
		auto text = GetUnformattedText();
		auto *expected = "Abc\nGhi";
		if(text != expected)
		{
			msg<<"Expected '"<<expected<<"', got: '"<<text<<"'!\n";
			return false;
		}
		return assert_anchor_point(msg,refPoint0,0,0) && assert_invalid_anchor_point(msg,refPoint1) && assert_anchor_point(msg,refPoint2,1,0);
	});

	unit_test("LineRemoveLast",[this,&validate,&assert_anchor_point,&assert_invalid_anchor_point](std::stringstream &msg) -> bool {
		AppendText("Abc\n");
		auto refPoint0 = CreateAnchorPoint(0,1u);
		if(validate() == false) return false;
		AppendText("Def\n");
		auto refPoint1 = CreateAnchorPoint(1,1u);
		if(validate() == false) return false;
		AppendText("Ghi");
		auto refPoint2 = CreateAnchorPoint(2,1u);
		if(validate() == false) return false;
		RemoveLine(2);
		auto text = GetUnformattedText();
		auto *expected = "Abc\nDef";
		if(text != expected)
		{
			msg<<"Expected '"<<expected<<"', got: '"<<text<<"'!\n";
			return false;
		}
		return assert_anchor_point(msg,refPoint0,0,1) && assert_anchor_point(msg,refPoint1,1,1) && assert_invalid_anchor_point(msg,refPoint2);
	});

	unit_test("LineRemoveInvalid",[this,&validate](std::stringstream &msg) -> bool {
		AppendText("Abc\n");
		if(validate() == false) return false;
		AppendText("Def\n");
		if(validate() == false) return false;
		AppendText("Ghi");
		if(validate() == false) return false;
		RemoveLine(8);
		auto text = GetUnformattedText();
		auto *expected = "Abc\nDef\nGhi";
		if(text != expected)
		{
			msg<<"Expected '"<<expected<<"', got: '"<<text<<"'!\n";
			return false;
		}
		return true;
	});
	
	unit_test("ParseTag",[this](std::stringstream &msg) -> bool {
		AppendText("abc{[c]}def{[/d]}ghi");
		auto text = GetFormattedText();
		auto *expected = "abcdefghi";
		if(text != expected)
		{
			msg<<"Expected '"<<expected<<"', got: '"<<text<<"'!\n";
			return false;
		}
		return true;
	});
	
	unit_test("RemoveTextSameLine",[this,&validate](std::stringstream &msg) -> bool {
		AppendText("abc{[c]}def{[/d]}ghi");
		if(validate() == false) return false;
		RemoveText(0,4,6);
		auto unformattedText = GetUnformattedText();
		auto *expected = "abc{f{[/d]}ghi";
		if(unformattedText != expected)
		{
			msg<<"Expected '"<<expected<<"', got: '"<<unformattedText<<"'!\n";
			return false;
		}
		auto formattedText = GetFormattedText();
		expected = "abc{fghi";
		if(formattedText != expected)
		{
			msg<<"Expected '"<<expected<<"', got: '"<<formattedText<<"'!\n";
			return false;
		}
		return true;
	});
	
	unit_test("RemoveTextMiddle",[this,&validate,&assert_anchor_point,&assert_invalid_anchor_point](std::stringstream &msg) -> bool {
		AppendText("abcdef\nghijkl\nmnopqr");
		auto refPoint0 = CreateAnchorPoint(1,0u);
		auto refPoint1 = CreateAnchorPoint(1,1u);
		auto refPoint2 = CreateAnchorPoint(1,4u);
		if(validate() == false) return false;
		RemoveText(1,1,3);
		auto text = GetUnformattedText();
		auto *expected = "abcdef\ngkl\nmnopqr";
		if(text != expected)
		{
			msg<<"Expected '"<<expected<<"', got: '"<<text<<"'!\n";
			return false;
		}
		return assert_anchor_point(msg,refPoint0,1,0) && assert_invalid_anchor_point(msg,refPoint1) && assert_anchor_point(msg,refPoint2,1,1);
	});
	
	unit_test("RemoveTextNewline",[this,&validate,&assert_anchor_point,&assert_invalid_anchor_point](std::stringstream &msg) -> bool {
		AppendText("abcdef\nghijkl\nmnopqr");
		auto refPoint0 = CreateAnchorPoint(1,0u);
		auto refPoint1 = CreateAnchorPoint(1,1u);
		auto refPoint2 = CreateAnchorPoint(2,0u);
		if(validate() == false) return false;
		RemoveText(1,5,2);
		auto text = GetUnformattedText();
		auto *expected = "abcdef\nghijkmnopqr";
		if(text != expected)
		{
			msg<<"Expected '"<<expected<<"', got: '"<<text<<"'!\n";
			return false;
		}
		return assert_anchor_point(msg,refPoint0,1,0) && assert_anchor_point(msg,refPoint1,1,1) && assert_anchor_point(msg,refPoint2,1,5);
	});
	
	unit_test("RemoveTextMultipleNewline",[this,&validate,&assert_anchor_point,&assert_invalid_anchor_point](std::stringstream &msg) -> bool {
		AppendText("abcdef\nghijkl\nmnopqr");
		auto refPoint0 = CreateAnchorPoint(2,5u);
		if(validate() == false) return false;
		RemoveText(1,5,2);
		if(validate() == false) return false;
		RemoveText(0,0,7);
		auto text = GetUnformattedText();
		auto *expected = "ghijkmnopqr";
		if(text != expected)
		{
			msg<<"Expected '"<<expected<<"', got: '"<<text<<"'!\n";
			return false;
		}
		return assert_anchor_point(msg,refPoint0,0,10);
	});
	
	unit_test("MoveTextUp",[this,&validate,&assert_anchor_point,&assert_invalid_anchor_point](std::stringstream &msg) -> bool {
		AppendText("abcd\nefgh\nijkl\nmnop\nqrst");
		auto refPoint0 = CreateAnchorPoint(0,1u);
		auto refPoint1 = CreateAnchorPoint(1,2u);
		auto refPoint2 = CreateAnchorPoint(2,3u);
		auto refPoint3 = CreateAnchorPoint(3,0u);
		auto refPoint4 = CreateAnchorPoint(3,2u);
		auto refPoint5 = CreateAnchorPoint(4,3u);
		if(validate() == false) return false;
		RemoveText(3,1,2);
		if(validate() == false) return false;
		InsertText("lo",1,3);
		if(validate() == false) return false;
		MoveText(3,0,2,1,0);
		if(validate() == false) return false;
		auto text = GetUnformattedText();
		auto *expected = "abcd\nmpefgloh\nijkl\n\nqrst";
		if(text != expected)
		{
			msg<<"Expected '"<<expected<<"', got: '"<<text<<"'!\n";
			return false;
		}
		return assert_anchor_point(msg,refPoint0,0,1) && assert_anchor_point(msg,refPoint1,1,4) && assert_anchor_point(msg,refPoint2,2,3) &&
			assert_anchor_point(msg,refPoint3,1,0) && assert_invalid_anchor_point(msg,refPoint4) && assert_anchor_point(msg,refPoint5,4,3);

		//TODO:
//Test AnchorPoint at tag which is in removed line
//-> Use :Move to move tag?
/*
Move text up
Move text down
Move text with newline included
Move over several lines*/
	});
	
	unit_test("MoveSameLine",[this,&validate,&assert_anchor_point,&assert_invalid_anchor_point](std::stringstream &msg) -> bool {
		AppendText("abcdefghi");
		auto refPoint0 = CreateAnchorPoint(0,0u); // 'a'
		auto refPoint1 = CreateAnchorPoint(0,2u); // 'c'
		auto refPoint2 = CreateAnchorPoint(0,7u); // 'h'
		if(validate() == false) return false;
		MoveText(0,1,3,0,8);
		if(validate() == false) return false;
		MoveText(0,4,2,0,0);
		if(validate() == false) return false;
		auto text = GetUnformattedText();
		auto *expected = "hbaefgcdi";
		if(text != expected)
		{
			msg<<"Expected '"<<expected<<"', got: '"<<text<<"'!\n";
			return false;
		}
		return assert_anchor_point(msg,refPoint0,0,2) && assert_anchor_point(msg,refPoint1,0,6) && assert_anchor_point(msg,refPoint2,0,0);
	});
	
	unit_test("MoveTextDown",[this,&validate,&assert_anchor_point,&assert_invalid_anchor_point](std::stringstream &msg) -> bool {
		AppendText("ghsabcd\nefnk\nijrl\nmqop\nt");
		if(validate() == false) return false;
		MoveText(4,0,1,3,0); // -> ghsabcd\nefnk\nijrl\ntmqop\n
		std::cout<<GetUnformattedText()<<std::endl;
		if(validate() == false) return false;
		MoveText(3,0,5,0,0); // -> tmqopghsabcd\nefnk\nijrl\n\n
		if(validate() == false) return false;
		auto text = GetUnformattedText();
		auto *expected = "tmqopghsabcd\nefnk\nijrl\n\n";
		if(text != expected)
		{
			msg<<"Expected '"<<expected<<"', got: '"<<text<<"'!\n";
			return false;
		}
		return true;
	});

	// TODO: Test cases
	// Nested tags?
	unit_test("SimpleTag",[this,&validate,&assert_anchor_point,&assert_invalid_anchor_point,&validate_tags](std::stringstream &msg) -> bool {
		AppendText("{[abc]}12{[/abc]}3\n");

		/*ParseTags(0,0,UNTIL_THE_END);
		AppendText("qwe24{[x]}028{[/x]}3909rjerrt\n");
		ParseTags(1,0,UNTIL_THE_END);
		AppendText("456{[/abc]}");
		ParseTags(2,0,UNTIL_THE_END);
		if(validate() == false) return false;*/
		
		//if(validate() == false) return false;
		//RemoveText(0,10,UNTIL_THE_END);
		//if(validate() == false) return false;
		//ParseTags(0,0,UNTIL_THE_END);
		if(validate() == false) return false;
		auto text = GetUnformattedText();
		auto *expected = "tmqopghsabcd\nefnk\nijrl\n\n";

		// DebugPrint(true);

		return validate_tags({
			{"abc","12"}
		},msg);
	});
	unit_test("NestedTags",[this,&validate,&assert_anchor_point,&assert_invalid_anchor_point,&validate_tags](std::stringstream &msg) -> bool {
		AppendText("{[a#\"#t0:1,2,3\":4,5,6]}{[a#t1:,,a]}{[/a]}{[/a]}");
		if(validate() == false) return false;
		return validate_tags({
			{"a","{[a#t1:,,a]}{[/a]}","#t0:1,2,3",{"4","5","6"}},
			{"a","","t1",{"","","a"}}
		},msg);
	});
	unit_test("NestedTag2",[this,&validate,&assert_anchor_point,&assert_invalid_anchor_point,&validate_tags](std::stringstream &msg) -> bool {
		AppendText("{[a]}\n{[a]}\n{[/a]}\nabc{[/a]}");
		if(validate() == false) return false;
		return validate_tags({
			{"a","\n{[a]}\n{[/a]}\nabc","",{}},
			{"a","\n","",{}}
		},msg);
	});
	unit_test("RemoveInnerTag",[this,&validate,&assert_anchor_point,&assert_invalid_anchor_point,&validate_tags](std::stringstream &msg) -> bool {
		AppendText("{[a]}{[a]}x{[/a]}{[/a]}");
		RemoveText(0,5,1);
		if(validate() == false) return false;
		return validate_tags({
			{"a","[a]}x","",{}}
		},msg);
	});
	unit_test("InvalidTag",[this,&validate,&assert_anchor_point,&assert_invalid_anchor_point,&validate_tags](std::stringstream &msg) -> bool {
		AppendText("asdf{[/a]}");
		if(validate() == false) return false;
		return validate_tags({},msg);
	});
	unit_test("InvalidTag2",[this,&validate,&assert_anchor_point,&assert_invalid_anchor_point,&validate_tags](std::stringstream &msg) -> bool {
		AppendText("{[a]}asdf{[/a]}");
		RemoveText(0,0,2);
		if(validate() == false) return false;
		return validate_tags({},msg);
	});
	unit_test("TagLineRemove",[this,&validate,&assert_anchor_point,&assert_invalid_anchor_point,&validate_tags](std::stringstream &msg) -> bool {
		SetPreserveTagsOnLineRemoval(false);
		AppendText("{[a]}abc\ndef{[/a]}");
		if(validate() == false) return false;
		RemoveLine(0);
		if(validate() == false) return false;
		InsertText("{[a]}",0,0);
		// TODO: Move tag to next line
		//RemoveLine(0);

		if(validate() == false) return false;
		return validate_tags({
			{"a","def","",{}}
		},msg);
	});
	
	if(allSucceeded == true)
		std::cout<<"All unit tests have succeeded!"<<std::endl;
}

void FormattedText::DebugPrint(bool printTags) const
{
	std::cout<<"-------- DEBUG START --------"<<std::endl;
	TextOffset offset = 0;
	for(auto &line : m_textLines)
	{
		auto &strLine = line->GetUnformattedLine();
		auto endOffset = offset +strLine.GetAbsLength();
		std::cout<<strLine<<std::endl;
		std::string strAnchors(strLine.GetAbsLength() +1,' ');

		auto &startAnchor = line->GetStartAnchorPoint();
		auto startAnchorOffset = startAnchor.GetTextCharOffset() -offset;
		if(printTags == false)
		{
			strAnchors.at(startAnchorOffset) = '^';

			auto *pNextLineAnchor = startAnchor.GetNextLineAnchorStartPoint();
			for(auto &hChild : startAnchor.GetChildren())
			{
				if(hChild.IsExpired() || hChild.Get() == pNextLineAnchor)
					continue;
				auto anchorOffset = hChild->GetTextCharOffset() -offset;
				strAnchors.at(anchorOffset) = '^';
			}
		}
		else
		{
			auto tagLineIdx = 0u;
			for(auto &hTag : m_tags)
			{
				if(hTag.IsExpired())
					continue;
				++tagLineIdx;
				auto outerRange = hTag->GetOuterRange();
				if(outerRange.has_value() == false)
					continue;
				auto tagStartOffset = outerRange->first;
				auto tagEndOffset = (outerRange->second != UNTIL_THE_END) ? (tagStartOffset +outerRange->second -1) : endOffset;
				if(tagStartOffset > endOffset || tagEndOffset < tagStartOffset)
					continue;
				strAnchors.at(std::max(static_cast<int32_t>(tagStartOffset) -static_cast<int32_t>(offset),0)) = '>';
				strAnchors.at(std::min(tagEndOffset -offset,strAnchors.size() -1)) = '<';
				
				auto innerRange = hTag->GetInnerRange();
				if(innerRange.has_value())
				{
					auto innerTagStartOffset = innerRange->first;
					auto innerTagEndOffset = (innerRange->second != UNTIL_THE_END) ? (innerTagStartOffset +innerRange->second -1) : endOffset;
					strAnchors.at(std::clamp(static_cast<int32_t>(innerTagStartOffset) -static_cast<int32_t>(offset),0,static_cast<int32_t>(strAnchors.size()) -1)) = '^';
					strAnchors.at(std::min(innerTagEndOffset -offset,strAnchors.size() -1)) = 'v';
				}
			}
		}
		std::cout<<strAnchors<<std::endl;

		offset = endOffset;
	}
	std::cout<<"--------- DEBUG END ---------"<<std::endl;
}

bool FormattedText::Validate(std::stringstream &msg) const
{
	TextOffset unformattedOffset = 0;
	TextOffset formattedOffset = 0;
	LineIndex lineIndex = 0;
	auto first = true;
	LineStartAnchorPoint *pPrevLineStartAnchor = nullptr;
	LineStartAnchorPoint *pNextLineStartAnchor = nullptr;
	std::unordered_set<LineStartAnchorPoint*> iteratedStartPoints {};
	for(auto &line : m_textLines)
	{
		if(line->Validate(msg) == false)
			return false;
		if(line->GetIndex() == INVALID_LINE_INDEX)
		{
			msg<<"Line has invalid line index!";
			return false;
		}
		if(lineIndex != line->GetIndex())
		{
			msg<<"Line index "<<line->GetIndex()<<" does not match expected index "<<lineIndex<<"!";
			return false;
		}
		auto &anchorPoint = line->GetStartAnchorPoint();
		auto it = iteratedStartPoints.find(&anchorPoint);
		if(it != iteratedStartPoints.end())
		{
			msg<<"Same start anchor is used for more than one line!";
			return false;
		}
		if(anchorPoint.GetTextCharOffset() != unformattedOffset)
		{
			msg<<"Line offset of "<<anchorPoint.GetTextCharOffset()<<" of line "<<lineIndex<<" does not match expected offset of "<<unformattedOffset<<"!";
			return false;
		}


		for(auto &hAnchorPoint : line->GetAnchorPoints())
		{
			if(hAnchorPoint.IsExpired() || hAnchorPoint->IsValid() == false)
				continue;
			auto &line = hAnchorPoint->GetLine();
			if(line.IsInRange(hAnchorPoint->GetTextCharOffset()) == false)
			{
				msg<<"Anchor point is out of range of its line!";
				return false;
			}
		}
		TextOffset nextOffset = 0;
		for(auto &hTagComponent : line->GetTagComponents())
		{
			if(hTagComponent.IsExpired() || hTagComponent->IsValid() == false)
				continue;
			auto startOffset = hTagComponent->GetStartAnchorPoint()->GetTextCharOffset();
			auto endOffset = hTagComponent->GetEndAnchorPoint()->GetTextCharOffset();
			if(endOffset < startOffset)
			{
				msg<<"Tag component has invalid range!";
				return false;
			}
			if(startOffset < nextOffset)
			{
				msg<<"Tag components are not in sequential order!";
				return false;
			}
			nextOffset = endOffset +1;
		}

		if(first == false)
		{
			if(anchorPoint.GetPreviousLineAnchorStartPoint() == nullptr)
			{
				msg<<"Line has no previous anchor even though it is not the first line!";
				return false;
			}
			if(&anchorPoint != pNextLineStartAnchor)
			{
				msg<<"Anchor point differs from expected anchor point!";
				return false;
			}
			if(anchorPoint.GetPreviousLineAnchorStartPoint() != pPrevLineStartAnchor)
			{
				msg<<"Previous anchor point does not match expected anchor point!";
				return false;
			}
		}
		pPrevLineStartAnchor = &anchorPoint;
		pNextLineStartAnchor = anchorPoint.GetNextLineAnchorStartPoint();
		if(pNextLineStartAnchor)
		{
			auto it = iteratedStartPoints.find(pNextLineStartAnchor);
			if(it != iteratedStartPoints.end())
			{
				msg<<"Next line start anchor is already used by a previous line!";
				return false;
			}
		}

		unformattedOffset = line->GetAbsEndOffset() +1;
		++lineIndex;
		first = false;
	}
	TextOffset nextOffset = 0;
	std::unordered_set<const TextTagComponent*> usedComponents {};
	for(auto &hTag : m_tags)
	{
		if(hTag.IsExpired() || hTag->IsValid() == false)
			continue;
		auto &openingTag = *hTag->GetOpeningTagComponent();
		auto it = usedComponents.find(&openingTag);
		if(it != usedComponents.end())
		{
			msg<<"Opening tag component is being used by multiple tags!";
			return false;
		}
		else
			usedComponents.insert(&openingTag);
		if(openingTag.GetStartAnchorPoint()->GetTextCharOffset() < nextOffset)
		{
			msg<<"Tags are not in sequential order!";
			return false;
		}
		if(hTag->IsClosed())
		{
			auto &closingTag = *hTag->GetClosingTagComponent();
			auto it = usedComponents.find(&closingTag);
			if(it != usedComponents.end())
			{
				msg<<"Closing tag component is being used by multiple tags!";
				return false;
			}
			else
				usedComponents.insert(&closingTag);
			if(closingTag.GetStartAnchorPoint()->GetTextCharOffset() <= openingTag.GetEndAnchorPoint()->GetTextCharOffset())
			{
				msg<<"Tag has invalid range!";
				return false;
			}
			if(openingTag.GetTagName() != closingTag.GetTagName())
			{
				msg<<"Opening tag name does not match closing tag name";
				return false;
			}
		}
		nextOffset = openingTag.GetEndAnchorPoint()->GetTextCharOffset() +1;
	}
	return true;
}
#endif
#pragma optimize("",on)
