/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "util_formatted_text.hpp"
#include "util_formatted_text_tag.hpp"
#include <algorithm>

using namespace util::text;

void FormattedText::ParseTags(LineIndex lineIdx,CharOffset offset,TextLength len)
{
	if(AreTagsEnabled() == false || lineIdx >= m_textLines.size())
		return;
	auto &line = *m_textLines.at(lineIdx);
	if(offset == LAST_CHAR)
		offset = line.GetLength();
	auto absOffset = line.GetStartOffset() +offset;
	len = (len == UNTIL_THE_END) ? line.GetAbsLength() : len;
	len = std::min(len,line.GetAbsLength() -offset);
	auto absEndOffset = absOffset +len -1;
	
	auto &tagComponents = line.GetTagComponents();
	for(auto it=tagComponents.begin();it!=tagComponents.end();)
	{
		auto &hTagComponent = *it;
		if(hTagComponent->IsValid() == false)
		{
			it = tagComponents.erase(it);
			continue;
		}
		auto startOffset = hTagComponent->GetStartAnchorPoint()->GetTextCharOffset();
		auto endOffset = hTagComponent->GetEndAnchorPoint()->GetTextCharOffset();
		if(
			(startOffset >= absOffset && startOffset <= absEndOffset) ||
			(endOffset >= absOffset && endOffset <= absEndOffset)
		)
		{
			// Component is touching input range

			// Increase range to encompass entire tag component
			if(startOffset < absOffset)
				absOffset = startOffset;
			if(endOffset > absEndOffset)
				absEndOffset = endOffset;

			// Remove the tag component. It will be re-created further below.
			// This is required in case any changes were made to the component contents.
			hTagComponent.Remove();
			it = tagComponents.erase(it);
			continue;
		}
		++it;
	}

	// Range may have changed, update it!
	offset = std::min(absOffset -line.GetStartOffset(),line.GetAbsLength() -1);
	len = absEndOffset -absOffset +1;
	auto clampedEndOffset = std::min(offset +len -1,line.GetAbsLength() -1);
	len = clampedEndOffset -offset +1;

	std::vector<util::TSharedHandle<TextTagComponent>> newTagComponents {}; // Contains all new tag components in sequential (by character offset) order
	if(len > 0)
	{
		// Parse text range and determine tag components with it
		auto text = line.Substr(offset,len);
		auto endOffset = offset +len -1;
		for(auto i=offset;i<=endOffset;)
		{
			auto substr = text.substr(i -offset);
			auto tagComponent = line.ParseTagComponent(i,substr);
			if(tagComponent.IsValid())
			{
				newTagComponents.insert(std::find_if(newTagComponents.begin(),newTagComponents.end(),[&tagComponent](util::TSharedHandle<TextTagComponent> &hTagComponent) {
					return hTagComponent->GetStartAnchorPoint()->GetTextCharOffset() > tagComponent->GetStartAnchorPoint()->GetTextCharOffset();
				}),tagComponent);

				auto offset = tagComponent->GetStartAnchorPoint()->GetTextCharOffset();
				tagComponents.insert(std::find_if(tagComponents.begin(),tagComponents.end(),[offset](util::TSharedHandle<TextTagComponent> &hTagComponent) {
					return hTagComponent->GetStartAnchorPoint()->GetTextCharOffset() > offset;
				}),tagComponent);
				i += tagComponent->GetLength();
				continue;
			}
			++i;
		}
	}

	for(auto it=m_tags.begin();it!=m_tags.end();)
	{
		auto &hTag = *it;
		if(hTag.IsExpired())
		{
			it = m_tags.erase(it);
			continue;
		}
		// TODO: What if tag invalid? (e.g. has closing tag but no opening tag)
		auto &tag = *hTag;
		auto removeTag = tag.IsValid() == false;
		if(removeTag == false)
		{
			auto tagRange = tag.GetOuterRange();
			removeTag = tagRange.has_value() == false;
			if(tagRange.has_value())
			{
				auto tagOffset = tagRange->first;
				auto tagLen = tagRange->second;
				if(
					(tag.IsClosed() == true && (tagOffset +tagLen -1) < absOffset) || // Tag has been closed before the input range, it remains unchanged and can be ignored
					tagOffset > absEndOffset // Tag has been opened after the input range, it remains unchanged and can be ignored
				)
				{
					++it;
					continue;
				}
				removeTag = true;
			}
		}
		if(removeTag)
		{
			// Tag intersects input range (or closing tag has been moved in front of opening tag),
			// move its components to the list of new components and remove the tag. It will be
			// re-created in the next step if its components are still in place.
			newTagComponents.reserve(newTagComponents.size() +2);
				
			// Mark opening and closing components as 'new' components
			auto *pOpeningTagComponent = tag.GetOpeningTagComponent();
			auto itInsert = newTagComponents.begin();
			if(pOpeningTagComponent && pOpeningTagComponent->IsValid())
			{
				itInsert = std::find_if(newTagComponents.begin(),newTagComponents.end(),[pOpeningTagComponent](const util::TSharedHandle<TextTagComponent> &tagComponent) {
					return tagComponent->GetStartAnchorPoint()->GetTextCharOffset() > pOpeningTagComponent->GetStartAnchorPoint()->GetTextCharOffset();
				});
				itInsert = newTagComponents.insert(itInsert,tag.m_openingTag);
				if(itInsert != newTagComponents.end())
					++itInsert;
			}

			auto *pClosingTagComponent = tag.GetClosingTagComponent();
			if(pClosingTagComponent && pClosingTagComponent->IsValid())
			{
				itInsert = std::find_if(itInsert,newTagComponents.end(),[pClosingTagComponent](const util::TSharedHandle<TextTagComponent> &tagComponent) {
					return tagComponent->GetStartAnchorPoint()->GetTextCharOffset() > pClosingTagComponent->GetStartAnchorPoint()->GetTextCharOffset();
				});
				newTagComponents.insert(itInsert,tag.m_closingTag);
			}

			hTag.Remove();
			it = m_tags.erase(it);
			continue;
		}
		++it;
	}

	std::vector<util::TSharedHandle<TextTag>> newOpenTags {};
	for(auto &hTagComponent : newTagComponents)
	{
		if(hTagComponent->IsValid() == false)
			continue;
		if(hTagComponent->IsOpeningTag())
		{
			auto offset = hTagComponent->GetStartAnchorPoint()->GetTextCharOffset();
			auto itInsert = std::find_if(m_tags.begin(),m_tags.end(),[offset](const util::TSharedHandle<TextTag> &tag) {
				return tag->IsValid() && tag->GetOpeningTagComponent()->GetStartAnchorPoint()->GetTextCharOffset() > offset;
			});
			auto tag = util::TSharedHandle<TextTag>{new TextTag{*this,hTagComponent}};
			m_tags.insert(itInsert,tag);
			newOpenTags.push_back(tag);
		}
		else
		{
			auto &tagName = hTagComponent->GetTagName();
			auto it = std::find_if(newOpenTags.rbegin(),newOpenTags.rend(),[&tagName](const util::TSharedHandle<TextTag> &hTag) {
				return hTag.IsValid() && hTag->IsClosed() == false && hTag->GetOpeningTagComponent()->GetTagName() == tagName;
			});
			if(it != newOpenTags.rend())
				(*it)->SetClosingTagComponent(hTagComponent);
		}
	}
}
