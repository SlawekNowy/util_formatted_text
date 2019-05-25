/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "util_formatted_text_line.hpp"
#include "util_formatted_text_tag.hpp"
#include "util_formatted_text_anchor_point.hpp"
#include <assert.h>
#include <vector>
#include <algorithm>

using namespace util::text;

PFormattedTextLine FormattedTextLine::Create(const std::string &line)
{
	return PFormattedTextLine{new FormattedTextLine{line}};
}
FormattedTextLine::~FormattedTextLine()
{
	for(auto &hAnchorPoint : m_anchorPoints)
	{
		if(hAnchorPoint.IsValid())
			hAnchorPoint.Remove();
	}
	m_anchorPoints.clear();
}
FormattedTextLine::FormattedTextLine(const std::string &line)
{
	if(line.empty())
		return;
	m_unformattedLine = line;
	m_bDirty = true;
}
void FormattedTextLine::Initialize(LineIndex lineIndex)
{
	m_lineIndex = lineIndex;
	m_startAnchorPoint = AnchorPoint::Create<LineStartAnchorPoint>(*this);
}
void FormattedTextLine::SetIndex(LineIndex lineIndex) {m_lineIndex = lineIndex;}
LineIndex FormattedTextLine::GetIndex() const {return m_lineIndex;}
void FormattedTextLine::DetachAnchorPoint(const AnchorPoint &anchorPoint)
{
	auto it = std::find_if(m_anchorPoints.begin(),m_anchorPoints.end(),[&anchorPoint](const util::TWeakSharedHandle<AnchorPoint> &hAnchorPoint) {
		return hAnchorPoint.Get() == &anchorPoint;
	});
	if(it == m_anchorPoints.end())
		return;
	m_anchorPoints.erase(it);
}
void FormattedTextLine::AttachAnchorPoint(AnchorPoint &anchorPoint)
{
	auto it = std::find_if(m_anchorPoints.begin(),m_anchorPoints.end(),[&anchorPoint](const util::TWeakSharedHandle<AnchorPoint> &hAnchorPoint) {
		return hAnchorPoint.Get() == &anchorPoint;
	});
	m_anchorPoints.push_back(anchorPoint.GetHandle());
}

const TextLine &FormattedTextLine::GetFormattedLine() const {return const_cast<FormattedTextLine*>(this)->GetFormattedLine();}
TextLine &FormattedTextLine::GetFormattedLine() {return m_formattedLine;}

const TextLine &FormattedTextLine::GetUnformattedLine() const {return const_cast<FormattedTextLine*>(this)->GetUnformattedLine();}
TextLine &FormattedTextLine::GetUnformattedLine() {return m_unformattedLine;}

const std::vector<util::TSharedHandle<TextTagComponent>> &FormattedTextLine::GetTagComponents() const {return const_cast<FormattedTextLine*>(this)->GetTagComponents();}
std::vector<util::TSharedHandle<TextTagComponent>> &FormattedTextLine::GetTagComponents() {return m_tagComponents;}
const std::vector<util::TWeakSharedHandle<AnchorPoint>> &FormattedTextLine::GetAnchorPoints() const {return const_cast<FormattedTextLine*>(this)->GetAnchorPoints();}
std::vector<util::TWeakSharedHandle<AnchorPoint>> &FormattedTextLine::GetAnchorPoints() {return m_anchorPoints;}
const LineStartAnchorPoint &FormattedTextLine::GetStartAnchorPoint() const {return const_cast<FormattedTextLine*>(this)->GetStartAnchorPoint();}
LineStartAnchorPoint &FormattedTextLine::GetStartAnchorPoint() {return static_cast<LineStartAnchorPoint&>(*m_startAnchorPoint);}

bool FormattedTextLine::IsEmpty() const {return m_unformattedLine.GetLength() == 0;}
TextOffset FormattedTextLine::GetStartOffset() const {return m_startAnchorPoint->GetTextCharOffset();}
TextOffset FormattedTextLine::GetEndOffset() const
{
	if(IsEmpty())
		throw std::logic_error("Empty line cannot have an end offset!");
	return GetStartOffset() +m_unformattedLine.GetLength() -1;
}
TextOffset FormattedTextLine::GetAbsEndOffset() const {return GetStartOffset() +(GetAbsLength() -1);}
TextLength FormattedTextLine::GetAbsLength() const {return m_unformattedLine.GetAbsLength();}
TextLength FormattedTextLine::GetLength() const {return m_unformattedLine.GetLength();}
std::optional<CharOffset> FormattedTextLine::GetRelativeOffset(TextOffset offset) const
{
	if(IsInRange(offset) == false)
		return {};
	return offset -GetStartOffset();
}
bool FormattedTextLine::IsInRange(TextOffset offset,TextLength len) const
{
	auto startOffset = GetStartOffset();
	auto endOffset = GetAbsEndOffset();
	return len > 0 && offset >= startOffset && (offset +len -1) <= endOffset;
}
std::optional<char> FormattedTextLine::GetChar(CharOffset offset) const {return m_unformattedLine.GetChar(offset);}

CharOffset FormattedTextLine::AppendString(const std::string_view &str)
{
	auto charOffset = GetLength();
	auto success = InsertString(str,charOffset);
	assert(success);
	return charOffset;
}
std::optional<CharOffset> FormattedTextLine::InsertString(const std::string_view &str,CharOffset charOffset)
{
	if(charOffset == LAST_CHAR)
		charOffset = GetLength();
	auto lenLine = GetAbsLength();
	auto result = m_unformattedLine.InsertString(str,charOffset);
	if(result == false)
		return {};
	ShiftAnchors(charOffset,UNTIL_THE_END,static_cast<ShiftOffset>(str.length()),lenLine);

	m_bDirty = true;
	return charOffset;
}

std::string_view FormattedTextLine::Substr(CharOffset offset,TextLength len) const
{
	return m_unformattedLine.Substr(offset,len);
}

void FormattedTextLine::ShiftAnchors(CharOffset startOffset,TextLength len,ShiftOffset shiftAmount,TextLength oldLineLen)
{
	if(startOffset >= oldLineLen)
		return;
	if(len == UNTIL_THE_END || (startOffset +len) > oldLineLen)
		len = oldLineLen -startOffset;
	
	auto &startAnchorPoint = GetStartAnchorPoint();
	startOffset += startAnchorPoint.GetTextCharOffset();
	auto endOffset = startOffset +len -1;
	auto endOffsetLine = startAnchorPoint.GetTextCharOffset() +oldLineLen -1;
	auto &childAnchors = startAnchorPoint.GetChildren();
	for(auto &hChild : childAnchors)
	{
		if(hChild.IsExpired() || hChild.Get() == m_startAnchorPoint.Get())
			continue;
		auto *childAnchor = hChild.Get();
		if(childAnchor->IsInRange(startOffset,len))
		{
			if(childAnchor->ShouldAllowOutOfBounds() == false)
				hChild.Remove();
			continue;
		}
		auto childAnchorOffset = childAnchor->GetTextCharOffset();
		if(childAnchorOffset > endOffset && childAnchorOffset <= endOffsetLine)
			childAnchor->ShiftByOffset(shiftAmount);
	}
	auto *nextLineAnchorPoint = startAnchorPoint.GetNextLineAnchorStartPoint();
	if(nextLineAnchorPoint)
		nextLineAnchorPoint->ShiftByOffset(shiftAmount);
}

std::optional<TextLength> FormattedTextLine::Erase(CharOffset startOffset,TextLength len,std::string *outErasedString)
{
	auto lenLine = GetAbsLength();
	auto numErased = m_unformattedLine.Erase(startOffset,len,outErasedString);
	if(numErased.has_value() == false)
		return false;
	// Update anchor offsets
	len = *numErased;
	ShiftAnchors(startOffset,len,-static_cast<ShiftOffset>(len),lenLine);

	m_bDirty = true;
	return len;
}

std::vector<util::TSharedHandle<util::text::AnchorPoint>> FormattedTextLine::DetachAnchorPoints(CharOffset startOffset,TextLength len)
{
	if(len == UNTIL_THE_END)
		len = GetAbsLength() -startOffset;
	std::vector<TSharedHandle<util::text::AnchorPoint>> anchorPointsInRange {};
	startOffset += GetStartOffset();
	for(auto &hChild : GetStartAnchorPoint().GetChildren())
	{
		if(hChild.IsValid() == false || hChild->IsValid() == false || hChild->IsInRange(startOffset,len) == false)
			continue;
		anchorPointsInRange.push_back(hChild);
		hChild->ClearLine();
		hChild->ClearParent();
	}
	return anchorPointsInRange;
}

void FormattedTextLine::AttachAnchorPoints(std::vector<util::TSharedHandle<util::text::AnchorPoint>> &anchorPoints,ShiftOffset shiftOffset)
{
	for(auto &pAnchorPoint : anchorPoints)
	{
		pAnchorPoint->SetParent(GetStartAnchorPoint());
		pAnchorPoint->SetLine(*this);
		pAnchorPoint->SetOffset(pAnchorPoint->GetTextCharOffset() +shiftOffset);
	}
}

bool FormattedTextLine::Move(CharOffset startOffset,TextLength len,FormattedTextLine &moveTarget,CharOffset targetCharOffset)
{
	// Temporarily remove all anchor points within the move range from this line
	auto anchorPointsInMoveRange = DetachAnchorPoints(startOffset,len);

	std::string erasedString;
	if(Erase(startOffset,len,&erasedString) == false || moveTarget.InsertString(erasedString,targetCharOffset) == false)
		return false;
	// Add the removed anchor points to the target line and update their offsets accordingly
	moveTarget.AttachAnchorPoints(anchorPointsInMoveRange,static_cast<ShiftOffset>(moveTarget.GetStartOffset() +targetCharOffset) -static_cast<ShiftOffset>(GetStartOffset() +startOffset));
	return true;
}

void FormattedTextLine::AppendCharacter(char c)
{
	auto len = GetLength();
	auto absLen = GetAbsLength();
	m_unformattedLine.AppendCharacter(c);
	if(m_startAnchorPoint.IsValid()) // The line may not be initialized at this point yet
		ShiftAnchors(len,0,1,absLen);
	m_bDirty = true;
}

TextLine &FormattedTextLine::Format()
{
	if(m_bDirty == false)
		return m_formattedLine;
	m_bDirty = false;
	m_formattedLine.Clear();
	auto len = m_unformattedLine.GetLength();
	m_formattedLine.Reserve(len);
	auto lineView = std::string_view{m_unformattedLine.GetText()};

	auto curTagIdx = 0u;
	TextOffset offset = 0u;
	while(offset < len)
	{
		if(curTagIdx < m_tagComponents.size())
		{
			auto &tagComponent = m_tagComponents.at(curTagIdx);
			auto startOffset = tagComponent->GetStartAnchorPoint()->GetTextCharOffset();
			if(offset == startOffset)
			{
				offset += tagComponent->GetLength();
				++curTagIdx;
				continue;
			}
		}
		auto c = m_unformattedLine.At(offset);
		m_formattedLine.AppendCharacter(c);
		++offset;
	}
	return m_formattedLine;
}

util::TSharedHandle<AnchorPoint> FormattedTextLine::CreateAnchorPoint(CharOffset charOffset,bool allowOutOfBounds)
{
	if(allowOutOfBounds == false && charOffset >= GetLength())
		return {};
	auto anchorPoint = AnchorPoint::Create(*this);
	anchorPoint->SetOffset(GetStartOffset() +charOffset);
	anchorPoint->SetParent(GetStartAnchorPoint());
	return anchorPoint;
}

util::TSharedHandle<TextTagComponent> FormattedTextLine::ParseTagComponent(CharOffset offset,const std::string_view &str)
{
	if(str.empty() || strncmp(str.data(),TextTag::TAG_PREFIX.data(),TextTag::TAG_PREFIX.length()) != 0 || str.length() < (TextTag::TAG_PREFIX.length() +TextTag::TAG_POSTFIX.length()))
		return {};
	auto isClosingTag = false;
	enum class Stage : uint8_t
	{
		TagName = 0u,
		Label,
		Arguments
	};
	auto stage = Stage::TagName;
	std::string tagName {};
	std::string label {};
	std::vector<std::string> args {};
	auto startOffset = TextTag::TAG_PREFIX.length();
	auto curOffset = startOffset;
	auto bStringInQuotes = false;
	while(curOffset < str.length())
	{
		auto token = str.at(curOffset);
		auto controlToken = token;
		if(bStringInQuotes && token != '\0' && token != '\"')
			controlToken = ' '; // Arbitrary token which must reach 'default' branch
		switch(controlToken)
		{
			case '\0':
				return {};
			case ':':
				stage = Stage::Arguments;
				break;
			case '#':
				if(stage == Stage::TagName)
					stage = Stage::Label;
				break;
			case ',':
				if(stage == Stage::Arguments)
				{
					if(args.empty())
						args.push_back(""); // First argument is empty
					args.push_back("");
				}
				break;
			case '/':
				if(curOffset == startOffset)
					isClosingTag = true;
				break;
			case '"':
				bStringInQuotes = !bStringInQuotes;
				break;
			default:
			{
				if(controlToken == TextTag::TAG_POSTFIX.front() && str.substr(curOffset,TextTag::TAG_POSTFIX.length()) == TextTag::TAG_POSTFIX)
				{
					auto endOffset = curOffset +TextTag::TAG_POSTFIX.length() -1;
					auto startAnchor = CreateAnchorPoint(offset);
					auto endAnchor = CreateAnchorPoint(offset +endOffset);
					if(isClosingTag == false)
						return util::TSharedHandle<TextTagComponent>{new TextOpeningTagComponent{tagName,label,args,startAnchor,endAnchor}};
					else
						return util::TSharedHandle<TextTagComponent>{new TextTagComponent{tagName,startAnchor,endAnchor}};
				}
				switch(stage)
				{
					case Stage::TagName:
						tagName += token;
						break;
					case Stage::Label:
						label += token;
						break;
					default:
						if(args.empty())
							args.push_back("");
						args.back() += token;
						break;
				}
				break;
			}
		}
		++curOffset;
	}
	return {};
}

#ifdef ENABLE_FORMATTED_TEXT_UNIT_TESTS
bool FormattedTextLine::Validate(std::stringstream &msg) const
{
	return m_unformattedLine.Validate(msg) && m_formattedLine.Validate(msg);
}
#endif
