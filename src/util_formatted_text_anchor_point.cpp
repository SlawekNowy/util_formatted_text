/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "util_formatted_text_anchor_point.hpp"
#include "util_formatted_text_line.hpp"
#include "util_formatted_text.hpp"
#include <algorithm>

using namespace util::text;

AnchorPoint::AnchorPoint(TextOffset charOffset,bool allowOutOfBounds)
	: m_wpLine{},m_charOffset{charOffset},m_bAllowOutOfBounds{allowOutOfBounds}
{}

util::TSharedHandle<AnchorPoint> AnchorPoint::GetHandle() {return util::claim_shared_handle_ownership(m_handle);}
bool AnchorPoint::IsValid() const {return m_wpLine.expired() == false;}

bool AnchorPoint::operator==(const AnchorPoint &other) const
{
	return IsValid() && other.IsValid() && m_charOffset == other.m_charOffset && m_bAllowOutOfBounds == other.m_bAllowOutOfBounds;
}
bool AnchorPoint::operator!=(const AnchorPoint &other) const {return operator==(other) == false;}
bool AnchorPoint::operator<(const AnchorPoint &other) const {return m_charOffset < other.m_charOffset;}
bool AnchorPoint::operator>(const AnchorPoint &other) const {return m_charOffset > other.m_charOffset;}
bool AnchorPoint::operator<=(const AnchorPoint &other) const {return m_charOffset <= other.m_charOffset;}
bool AnchorPoint::operator>=(const AnchorPoint &other) const {return m_charOffset >= other.m_charOffset;}

LineIndex AnchorPoint::GetLineIndex() const {return GetLine().GetIndex();}
FormattedTextLine &AnchorPoint::GetLine() const {return *m_wpLine.lock();}

void AnchorPoint::SetLine(FormattedTextLine &line)
{
	ClearLine();
	line.AttachAnchorPoint(*this);
	m_wpLine = line.shared_from_this();
}
LineStartAnchorPoint *AnchorPoint::GetParent()
{
	return m_parent.IsValid() ? static_cast<LineStartAnchorPoint*>(m_parent.Get()) : nullptr;
}
void AnchorPoint::SetParent(LineStartAnchorPoint &parent)
{
	if(&parent == this)
		throw std::logic_error("Anchor cannot be the parent of itself!");
	ClearParent();

	auto offset = GetTextCharOffset();
	parent.m_children.push_back(m_handle);
	m_parent = parent.GetHandle();
	SetOffset(offset); // Re-apply offset
}
bool AnchorPoint::ShouldAllowOutOfBounds() const {return m_bAllowOutOfBounds;}
bool AnchorPoint::IsInRange(TextOffset startOffset,TextLength len) const
{
	if(len == 0)
		return false;
	return m_charOffset >= startOffset && (len == UNTIL_THE_END || m_charOffset < startOffset +len);
}
void AnchorPoint::ClearParent()
{
	if(m_parent.IsExpired())
		return;
	auto offset = GetTextCharOffset();
	auto *parent = static_cast<LineStartAnchorPoint*>(m_parent.Get());
	parent->RemoveChild(*this);
	m_parent = decltype(m_parent){};
	SetOffset(offset); // Re-apply offset
}
void AnchorPoint::ClearLine()
{
	if(m_wpLine.expired() == false)
		m_wpLine.lock()->DetachAnchorPoint(*this);
	m_wpLine = {};
}
bool AnchorPoint::IsAttachedToLine(FormattedTextLine &line) const
{
	return (m_wpLine.expired() == false) && m_wpLine.lock().get() == &line;
}
bool AnchorPoint::IsLineStartAnchorPoint() const {return false;}
TextOffset AnchorPoint::GetTextCharOffset() const
{
	auto offset = m_charOffset;
	if(m_parent.IsValid())
		offset += m_parent->GetTextCharOffset();
	return offset;
}
void AnchorPoint::ShiftToOffset(TextOffset offset)
{
	auto delta = static_cast<ShiftOffset>(offset) -static_cast<ShiftOffset>(GetTextCharOffset());
	ShiftByOffset(delta);
}

void AnchorPoint::ShiftByOffset(ShiftOffset offset) {SetOffset(GetTextCharOffset() +offset);}
void AnchorPoint::SetOffset(TextOffset offset)
{
	TextOffset baseOffset = 0;
	if(m_parent.IsValid())
		baseOffset = m_parent->GetTextCharOffset();
	m_charOffset = offset -baseOffset;
	if(m_wpLine.expired() || ShouldAllowOutOfBounds())
		return;
	auto &text = m_wpLine.lock()->GetTargetText();
	auto relOffset = text.GetRelativeCharOffset(offset);
	m_wpLine = relOffset.has_value() ? text.GetLine(relOffset->first)->shared_from_this() : std::weak_ptr<FormattedTextLine>{};
}

////////////

const std::vector<util::TWeakSharedHandle<AnchorPoint>> &LineStartAnchorPoint::GetChildren() const {return const_cast<LineStartAnchorPoint*>(this)->GetChildren();}
std::vector<util::TWeakSharedHandle<AnchorPoint>> &LineStartAnchorPoint::GetChildren() {return m_children;}
void LineStartAnchorPoint::RemoveChild(const AnchorPoint &anchorPoint)
{
	auto it = std::find_if(m_children.begin(),m_children.end(),[&anchorPoint](const util::TWeakSharedHandle<AnchorPoint> &hChild) {
		return hChild.IsValid() && hChild.Get() == &anchorPoint;
	});
	if(it == m_children.end())
		return;
	m_children.erase(it);

}
void LineStartAnchorPoint::ShiftByOffset(ShiftOffset offset)
{
	// Faster than recursion
	auto *pNextLineAnchorStartPoint = this;
	while(pNextLineAnchorStartPoint)
	{
		pNextLineAnchorStartPoint->SetOffset(pNextLineAnchorStartPoint->GetTextCharOffset() +offset);
		pNextLineAnchorStartPoint = pNextLineAnchorStartPoint->GetNextLineAnchorStartPoint();
	}
}
void LineStartAnchorPoint::SetNextLineAnchorStartPoint(LineStartAnchorPoint &anchor)
{
	if(&anchor == this)
		throw std::logic_error("Next anchor cannot be itself!");
	if(&anchor == GetNextLineAnchorStartPoint())
		return;
	ClearNextLineAnchorStartPoint();
	m_nextLineAnchorStartPoint = anchor.GetHandle();
	anchor.SetPreviousLineAnchorStartPoint(*this);
}
bool LineStartAnchorPoint::IsLineStartAnchorPoint() const {return true;}
void LineStartAnchorPoint::ClearNextLineAnchorStartPoint()
{
	m_nextLineAnchorStartPoint = decltype(m_nextLineAnchorStartPoint){};
}
LineStartAnchorPoint *LineStartAnchorPoint::GetNextLineAnchorStartPoint()
{
	return m_nextLineAnchorStartPoint.IsExpired() ? nullptr : static_cast<LineStartAnchorPoint*>(m_nextLineAnchorStartPoint.Get());
}
void LineStartAnchorPoint::ClearPreviousLineAnchorStartPoint()
{
	m_prevLineAnchorStartPoint = decltype(m_nextLineAnchorStartPoint){};
}
void LineStartAnchorPoint::SetPreviousLineAnchorStartPoint(LineStartAnchorPoint &anchor)
{
	if(&anchor == this)
		throw std::logic_error("Previous anchor cannot be itself!");
	if(&anchor == GetPreviousLineAnchorStartPoint())
		return;
	ClearPreviousLineAnchorStartPoint();
	m_prevLineAnchorStartPoint = anchor.GetHandle();
	anchor.SetNextLineAnchorStartPoint(*this);
}
LineStartAnchorPoint *LineStartAnchorPoint::GetPreviousLineAnchorStartPoint() {return static_cast<LineStartAnchorPoint*>(m_prevLineAnchorStartPoint.Get());}
