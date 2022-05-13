/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "util_formatted_text_tag.hpp"
#include "util_formatted_text.hpp"

using namespace util::text;
#pragma optimize("",off)
const decltype(TextTag::TAG_PREFIX) TextTag::TAG_PREFIX = "{[";
const decltype(TextTag::TAG_POSTFIX) TextTag::TAG_POSTFIX = "]}";

TextTagComponent::TextTagComponent(const std::string &tagName,const util::TSharedHandle<AnchorPoint> &startAnchor,const util::TSharedHandle<AnchorPoint> &endAnchor)
	: m_tagName{tagName},m_startAnchor{startAnchor},m_endAnchor{endAnchor}
{}
bool TextTagComponent::operator==(const TextTagComponent &other) const
{
	return IsValid() && other.IsValid() &&
		*m_startAnchor == *other.GetStartAnchorPoint() &&
		*m_endAnchor == *other.GetEndAnchorPoint() &&
		m_tagName == other.m_tagName;
}
bool TextTagComponent::operator!=(const TextTagComponent &other) const {return operator==(other) == false;}
bool TextTagComponent::operator<(const TextTagComponent &other) const
{
	return IsValid() && *m_endAnchor < *other.m_startAnchor;
}
bool TextTagComponent::operator>(const TextTagComponent &other) const
{
	return IsValid() && *m_startAnchor > *other.m_endAnchor;
}
util::Utf8String TextTagComponent::GetTagString(const FormattedText &text) const
{
	if(m_startAnchor.IsExpired() || m_endAnchor.IsExpired())
		return {};
	auto startOffset = m_startAnchor->GetTextCharOffset();
	auto endOffset = m_endAnchor->GetTextCharOffset();
	if(endOffset <= startOffset)
		return {};
	auto len = endOffset -startOffset +1;
	return text.Substr(startOffset,len);
}
const AnchorPoint *TextTagComponent::GetStartAnchorPoint() const {return const_cast<TextTagComponent*>(this)->GetStartAnchorPoint();}
AnchorPoint *TextTagComponent::GetStartAnchorPoint() {return m_startAnchor.Get();}
const AnchorPoint *TextTagComponent::GetEndAnchorPoint() const {return const_cast<TextTagComponent*>(this)->GetEndAnchorPoint();}
AnchorPoint *TextTagComponent::GetEndAnchorPoint() {return m_endAnchor.Get();}
bool TextTagComponent::IsClosingTag() const {return IsOpeningTag() == false;}
bool TextTagComponent::IsOpeningTag() const {return false;}
bool TextTagComponent::IsValid() const
{
	return m_startAnchor.IsValid() && m_endAnchor.IsValid() &&
		m_endAnchor->GetTextCharOffset() > m_startAnchor->GetTextCharOffset();
}
TextLength TextTagComponent::GetLength() const
{
	return m_endAnchor->GetTextCharOffset() -m_startAnchor->GetTextCharOffset() +1;
}
const std::string &TextTagComponent::GetTagName() const {return m_tagName;}

//////////////

TextOpeningTagComponent::TextOpeningTagComponent(
	const std::string &tagName,const std::string &label,const std::vector<std::string> &attributes,
	const util::TSharedHandle<AnchorPoint> &startAnchor,const util::TSharedHandle<AnchorPoint> &endAnchor
)
	: TextTagComponent{tagName,startAnchor,endAnchor},m_label{label},m_attributes{attributes}
{}
const std::string &TextOpeningTagComponent::GetLabel() const {return m_label;}
const std::vector<std::string> &TextOpeningTagComponent::GetTagAttributes() const {return m_attributes;}
bool TextOpeningTagComponent::IsOpeningTag() const {return true;}

bool TextOpeningTagComponent::operator==(const TextTagComponent &other) const
{
	return typeid(other) == typeid(TextOpeningTagComponent) && operator==(static_cast<const TextOpeningTagComponent&>(other));
}
bool TextOpeningTagComponent::operator==(const TextOpeningTagComponent &other) const
{
	return TextTagComponent::operator==(static_cast<const TextTagComponent&>(other)) && m_label == other.m_label && m_attributes == other.m_attributes;
}

//////////////

TextTag::TextTag(const FormattedText &text,const util::TSharedHandle<TextTagComponent> &openingTag,const util::TSharedHandle<TextTagComponent> &closingTag)
	: m_wpText{text.shared_from_this()},m_openingTag{openingTag},m_closingTag{closingTag}
{}
bool TextTag::IsValid() const
{
	return m_wpText.expired() == false && m_openingTag.IsValid() && m_openingTag->IsValid() &&
		(m_openingTag.IsExpired() || m_openingTag->IsValid());
}
bool TextTag::IsClosed() const
{
	return m_closingTag.IsValid() && m_closingTag->IsValid();
}
std::optional<std::pair<TextOffset,TextLength>> TextTag::GetInnerRange() const
{
	if(IsValid() == false)
		return {};
	auto startOffset = m_openingTag->GetEndAnchorPoint()->GetTextCharOffset() +1;
	if(IsClosed() == false)
		return {{startOffset,UNTIL_THE_END}};
	auto endOffset = m_closingTag->GetStartAnchorPoint()->GetTextCharOffset();
	if(endOffset == startOffset)
		return {}; // There is nothing between the opening and closing tags
	auto len = endOffset -startOffset;
	return {{startOffset,len}};
}
std::optional<std::pair<TextOffset,TextLength>> TextTag::GetOuterRange() const
{
	if(IsValid() == false)
		return {};
	auto startOffset = m_openingTag->GetStartAnchorPoint()->GetTextCharOffset();
	if(IsClosed() == false)
		return {{startOffset,UNTIL_THE_END}};
	auto endOffset = m_closingTag->GetEndAnchorPoint()->GetTextCharOffset();
	auto len = endOffset -startOffset +1;
	return {{startOffset,len}};
}
util::Utf8String TextTag::GetTagContents() const
{
	if(IsValid() == false)
		return {};
	auto range = GetInnerRange();
	return m_wpText.lock()->Substr(range->first,range->second);
}
util::Utf8String TextTag::GetTagString() const
{
	if(IsValid() == false)
		return {};
	auto range = GetOuterRange();
	return m_wpText.lock()->Substr(range->first,range->second);
}
util::Utf8String TextTag::GetOpeningTag() const
{
	if(m_wpText.expired())
		return "";
	return m_openingTag->GetTagString(*m_wpText.lock());
}
util::Utf8String TextTag::GetClosingTag() const
{
	if(m_wpText.expired())
		return "";
	return m_closingTag->GetTagString(*m_wpText.lock());
}
void TextTag::SetClosingTagComponent(const util::TSharedHandle<TextTagComponent> &closingTag) {m_closingTag = closingTag;}
const TextOpeningTagComponent *TextTag::GetOpeningTagComponent() const {return const_cast<TextTag*>(this)->GetOpeningTagComponent();}
TextOpeningTagComponent *TextTag::GetOpeningTagComponent() {return static_cast<TextOpeningTagComponent*>(m_openingTag.Get());}
const TextTagComponent *TextTag::GetClosingTagComponent() const {return const_cast<TextTag*>(this)->GetClosingTagComponent();}
TextTagComponent *TextTag::GetClosingTagComponent() {return m_closingTag.Get();}
#pragma optimize("",on)
