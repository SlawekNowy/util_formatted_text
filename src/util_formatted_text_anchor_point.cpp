/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "util_formatted_text_anchor_point.hpp"
#include "util_formatted_text_line.hpp"
#include <algorithm>

using namespace util::text;

AnchorPoint::AnchorPoint(TextOffset charOffset,bool allowOutOfBounds)
	: m_wpLine{},m_charOffset{charOffset},m_bAllowOutOfBounds{allowOutOfBounds}
{}

util::TSharedHandle<AnchorPoint> AnchorPoint::GetHandle() {return m_handle;}
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
TextOffset AnchorPoint::GetTextCharOffset() const {return m_charOffset;}
FormattedTextLine &AnchorPoint::GetLine() const {return *m_wpLine.lock();}

const std::vector<util::TWeakSharedHandle<AnchorPoint>> &AnchorPoint::GetChildren() const {return const_cast<AnchorPoint*>(this)->GetChildren();}
std::vector<util::TWeakSharedHandle<AnchorPoint>> &AnchorPoint::GetChildren() {return m_children;}
void AnchorPoint::SetLine(FormattedTextLine &line)
{
	ClearLine();
	line.AttachAnchorPoint(*this);
	m_wpLine = line.shared_from_this();
}
AnchorPoint *AnchorPoint::GetParent()
{
	return m_parent.IsValid() ? m_parent.Get() : nullptr;
}
void AnchorPoint::SetParent(AnchorPoint &parent)
{
	if(&parent == this)
		throw std::logic_error("Anchor cannot be the parent of itself!");
	ClearParent();
	parent.m_children.push_back(m_handle);
	m_parent = parent.GetHandle();
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
	auto *parent = m_parent.Get();
	parent->RemoveChild(*this);
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

void AnchorPoint::RemoveChild(const AnchorPoint &anchorPoint)
{
	auto it = std::find_if(m_children.begin(),m_children.end(),[&anchorPoint](const util::TWeakSharedHandle<AnchorPoint> &hChild) {
		return hChild.IsValid() && hChild.Get() == &anchorPoint;
	});
	if(it == m_children.end())
		return;
	m_children.erase(it);
}

void AnchorPoint::ShiftToOffset(TextOffset offset)
{
	auto delta = static_cast<ShiftOffset>(offset) -static_cast<ShiftOffset>(m_charOffset);
	ShiftByOffset(delta);
}

void AnchorPoint::ShiftByOffset(ShiftOffset offset)
{
	// TODO: What if offset < 0 or > line size?
	m_charOffset += offset;
	for(auto it=m_children.begin();it!=m_children.end();)
	{
		auto &wpChild = *it;
		if(wpChild.IsExpired())
		{
			it = m_children.erase(it);
			continue;
		}
		auto &child = *wpChild.Get();
		child.ShiftByOffset(offset);
		++it;
	}
}
void AnchorPoint::SetOffset(TextOffset offset) {m_charOffset = offset;}

////////////

void LineStartAnchorPoint::SetPreviousLineAnchorStartPoint(LineStartAnchorPoint &anchor)
{
	if(&anchor == GetParent())
		return;
	SetParent(anchor);
	anchor.SetNextLineAnchorStartPoint(*this);
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
void LineStartAnchorPoint::ClearPreviousLineAnchorStartPoint()
{
	ClearParent();
}
void LineStartAnchorPoint::ClearNextLineAnchorStartPoint()
{
	if(m_nextLineAnchorStartPoint.IsExpired())
		return;
	RemoveChild(*m_nextLineAnchorStartPoint);
}
LineStartAnchorPoint *LineStartAnchorPoint::GetPreviousLineAnchorStartPoint() {return static_cast<LineStartAnchorPoint*>(GetParent());}
LineStartAnchorPoint *LineStartAnchorPoint::GetNextLineAnchorStartPoint()
{
	return m_nextLineAnchorStartPoint.IsExpired() ? nullptr : static_cast<LineStartAnchorPoint*>(m_nextLineAnchorStartPoint.Get());
}
