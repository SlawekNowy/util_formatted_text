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
		class LineStartAnchorPoint;
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
			
			void SetLine(FormattedTextLine &line);
			LineStartAnchorPoint *GetParent();
			void SetParent(LineStartAnchorPoint &parent);
			void ClearParent();
			void ClearLine();
			bool IsAttachedToLine(FormattedTextLine &line) const;
			virtual bool IsLineStartAnchorPoint() const;
			bool IsInRange(TextOffset startOffset,TextLength len) const;
			bool ShouldAllowOutOfBounds() const;

			virtual void ShiftByOffset(ShiftOffset offset);
			void ShiftToOffset(TextOffset offset);
			void SetOffset(TextOffset offset);

			util::TSharedHandle<AnchorPoint> GetHandle();
		protected:
			AnchorPoint(TextOffset charOffset=0,bool allowOutOfBounds=false);
			AnchorPoint(const AnchorPoint&)=delete;
		private:
			TextOffset m_charOffset = 0u;
			bool m_bAllowOutOfBounds = false;
			std::weak_ptr<FormattedTextLine> m_wpLine = {};
			util::TWeakSharedHandle<AnchorPoint> m_parent = {};

			util::TWeakSharedHandle<AnchorPoint> m_handle = {};
		};

		class LineStartAnchorPoint
			: public AnchorPoint
		{
		public:
			void SetPreviousLineAnchorStartPoint(LineStartAnchorPoint &anchor);
			void ClearPreviousLineAnchorStartPoint();
			LineStartAnchorPoint *GetPreviousLineAnchorStartPoint();

			void SetNextLineAnchorStartPoint(LineStartAnchorPoint &anchor);
			void ClearNextLineAnchorStartPoint();
			LineStartAnchorPoint *GetNextLineAnchorStartPoint();

			virtual bool IsLineStartAnchorPoint() const override;
			virtual void ShiftByOffset(ShiftOffset offset) override;

			std::vector<util::TWeakSharedHandle<AnchorPoint>> &GetChildren();
			const std::vector<util::TWeakSharedHandle<AnchorPoint>> &GetChildren() const;
		protected:
			void RemoveChild(const AnchorPoint &anchorPoint);
			std::vector<util::TWeakSharedHandle<AnchorPoint>> m_children = {};
			using AnchorPoint::AnchorPoint;
			using AnchorPoint::SetParent;
			friend AnchorPoint;
		private:
			util::TWeakSharedHandle<AnchorPoint> m_prevLineAnchorStartPoint = {};
			util::TWeakSharedHandle<AnchorPoint> m_nextLineAnchorStartPoint = {};
		};
	};
};

template<class TAnchorPoint>
	util::TSharedHandle<TAnchorPoint> util::text::AnchorPoint::Create(FormattedTextLine &line,bool allowOutOfBounds)
{
	auto hAnchorPoint = util::TSharedHandle<TAnchorPoint>{new TAnchorPoint{allowOutOfBounds}};
	hAnchorPoint->m_handle = util::shared_handle_cast<TAnchorPoint,AnchorPoint>(hAnchorPoint);
	hAnchorPoint->SetLine(line);
	return hAnchorPoint;
}

#endif
