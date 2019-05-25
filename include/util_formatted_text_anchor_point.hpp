/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __UTIL_FORMATTED_TEXT_ANCHOR_POINT_HPP__
#define __UTIL_FORMATTED_TEXT_ANCHOR_POINT_HPP__

#include "util_formatted_text_types.hpp"
#include <sharedutils/util_shared_handle.hpp>
#include <memory>
#include <vector>

namespace util
{
	namespace text
	{
		class FormattedTextLine;
		class AnchorPoint
		{
		public:
			template<class TAnchorPoint=AnchorPoint>
				static util::TSharedHandle<TAnchorPoint> Create(FormattedTextLine &line,bool allowOutOfBounds=false);
			virtual ~AnchorPoint()=default;
			LineIndex GetLineIndex() const;
			TextOffset GetTextCharOffset() const;
			FormattedTextLine &GetLine() const;
			bool IsValid() const;

			AnchorPoint &operator=(const AnchorPoint&)=delete;
			// TODO: Replace these with operator<=> once C++-20 is released
			bool operator==(const AnchorPoint &other) const;
			bool operator!=(const AnchorPoint &other) const;
			bool operator<(const AnchorPoint &other) const;
			bool operator>(const AnchorPoint &other) const;
			bool operator<=(const AnchorPoint &other) const;
			bool operator>=(const AnchorPoint &other) const;
			
			std::vector<util::TWeakSharedHandle<AnchorPoint>> &GetChildren();
			const std::vector<util::TWeakSharedHandle<AnchorPoint>> &GetChildren() const;
			void SetLine(FormattedTextLine &line);
			AnchorPoint *GetParent();
			void SetParent(AnchorPoint &parent);
			void ClearParent();
			void ClearLine();
			bool IsAttachedToLine(FormattedTextLine &line) const;

			bool IsInRange(TextOffset startOffset,TextLength len) const;
			bool ShouldAllowOutOfBounds() const;

			void ShiftByOffset(ShiftOffset offset);
			void ShiftToOffset(TextOffset offset);
			void SetOffset(TextOffset offset);

			util::TSharedHandle<AnchorPoint> GetHandle();
		protected:
			AnchorPoint(TextOffset charOffset=0,bool allowOutOfBounds=false);
			AnchorPoint(const AnchorPoint&)=delete;
			void RemoveChild(const AnchorPoint &anchorPoint);
		private:
			TextOffset m_charOffset = 0u;
			bool m_bAllowOutOfBounds = false;
			std::weak_ptr<FormattedTextLine> m_wpLine = {};
			std::vector<util::TWeakSharedHandle<AnchorPoint>> m_children = {};
			util::TWeakSharedHandle<AnchorPoint> m_parent = {};

			util::TWeakSharedHandle<AnchorPoint> m_handle = {};
		};

		class LineStartAnchorPoint
			: public AnchorPoint
		{
		public:
			void SetPreviousLineAnchorStartPoint(LineStartAnchorPoint &anchor);
			void SetNextLineAnchorStartPoint(LineStartAnchorPoint &anchor);
			void ClearPreviousLineAnchorStartPoint();
			void ClearNextLineAnchorStartPoint();

			LineStartAnchorPoint *GetPreviousLineAnchorStartPoint();
			LineStartAnchorPoint *GetNextLineAnchorStartPoint();
		protected:
			using AnchorPoint::AnchorPoint;
			using AnchorPoint::SetParent;
			friend AnchorPoint;
		private:
			util::TWeakSharedHandle<AnchorPoint> m_nextLineAnchorStartPoint = {};
		};
	};
};

template<class TAnchorPoint>
	util::TSharedHandle<TAnchorPoint> util::text::AnchorPoint::Create(FormattedTextLine &line,bool allowOutOfBounds)
{
	auto hAnchorPoint = util::TSharedHandle<TAnchorPoint>{new TAnchorPoint{allowOutOfBounds}};
	hAnchorPoint->m_handle = util::TWeakSharedHandle<util::text::AnchorPoint>{hAnchorPoint.GetInternalData()};
	hAnchorPoint->SetLine(line);
	return hAnchorPoint;
}

#endif
