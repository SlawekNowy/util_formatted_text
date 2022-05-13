/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __UTIL_FORMATTED_TEXT_TAG_HPP__
#define __UTIL_FORMATTED_TEXT_TAG_HPP__

#include "util_formatted_text_anchor_point.hpp"
#include <sharedutils/util_shared_handle.hpp>
#include <sharedutils/util_utf8.hpp>
#include <optional>
#include <string>

namespace util
{
	namespace text
	{
		class FormattedText;
		class TextTagComponent
		{
		public:
			TextTagComponent(const std::string &tagName,const util::TSharedHandle<AnchorPoint> &startAnchor,const util::TSharedHandle<AnchorPoint> &endAnchor);
			TextTagComponent(const TextTagComponent&)=delete;
			TextTagComponent &operator=(const TextTagComponent&)=delete;

			virtual bool operator==(const TextTagComponent &other) const;
			bool operator!=(const TextTagComponent &other) const;
			bool operator<(const TextTagComponent &other) const;
			bool operator>(const TextTagComponent &other) const;
			bool operator<=(const TextTagComponent&) const=delete;
			bool operator>=(const TextTagComponent&) const=delete;

			util::Utf8String GetTagString(const FormattedText &text) const;
			const AnchorPoint *GetStartAnchorPoint() const;
			AnchorPoint *GetStartAnchorPoint();
			const AnchorPoint *GetEndAnchorPoint() const;
			AnchorPoint *GetEndAnchorPoint();
			bool IsClosingTag() const;
			virtual bool IsOpeningTag() const;
			bool IsValid() const;
			TextLength GetLength() const;
			const std::string &GetTagName() const;
		private:
			std::string m_tagName = "";
			util::TSharedHandle<AnchorPoint> m_startAnchor = {};
			util::TSharedHandle<AnchorPoint> m_endAnchor = {};
		};

		class TextOpeningTagComponent
			: public TextTagComponent
		{
		public:
			TextOpeningTagComponent(
				const std::string &tagName,const std::string &label,const std::vector<std::string> &attributes,
				const util::TSharedHandle<AnchorPoint> &startAnchor,const util::TSharedHandle<AnchorPoint> &endAnchor
			);
			const std::string &GetLabel() const;
			const std::vector<std::string> &GetTagAttributes() const;
			virtual bool IsOpeningTag() const override;
			virtual bool operator==(const TextTagComponent &other) const override;
			bool operator==(const TextOpeningTagComponent &other) const;
		private:
			std::string m_label = "";
			std::vector<std::string> m_attributes = {};
		};

		class TextTag
		{
		public:
			static const std::string TAG_PREFIX;
			static const std::string TAG_POSTFIX;
			TextTag(const FormattedText &text,const util::TSharedHandle<TextTagComponent> &openingTag,const util::TSharedHandle<TextTagComponent> &closingTag={});
			TextTag(const TextTag&)=delete;
			TextTag &operator=(const TextTag&)=delete;

			bool IsValid() const;
			bool IsClosed() const;
			std::optional<std::pair<TextOffset,TextLength>> GetInnerRange() const;
			std::optional<std::pair<TextOffset,TextLength>> GetOuterRange() const;
			util::Utf8String GetTagContents() const;
			util::Utf8String GetTagString() const;
			util::Utf8String GetOpeningTag() const;
			util::Utf8String GetClosingTag() const;
			void SetClosingTagComponent(const util::TSharedHandle<TextTagComponent> &closingTag);
			const TextOpeningTagComponent *GetOpeningTagComponent() const;
			TextOpeningTagComponent *GetOpeningTagComponent();
			const TextTagComponent *GetClosingTagComponent() const;
			TextTagComponent *GetClosingTagComponent();
		private:
			util::TSharedHandle<TextTagComponent> m_openingTag = {};
			util::TSharedHandle<TextTagComponent> m_closingTag = {};
			std::weak_ptr<const FormattedText> m_wpText = {};
			friend FormattedText;
		};
		using PTextTag = std::shared_ptr<TextTag>;
	};
};

#endif
