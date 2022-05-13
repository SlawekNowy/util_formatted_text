/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "util_text_line.hpp"
#include <algorithm>

using namespace util::text;

TextLine::TextLine(const std::string &line)
	: m_line{line}
{}

void TextLine::AppendCharacter(int32_t c) {m_line += c;}
bool TextLine::InsertString(const util::Utf8StringView &str,CharOffset charOffset)
{
	if(charOffset == LAST_CHAR)
		charOffset = m_line.length();
	if(charOffset > m_line.length())
		return false;
	m_line.insert(m_line.begin() +charOffset,str.to_str());
	return true;
}

TextLength TextLine::GetLength() const {return m_line.length();}
TextLength TextLine::GetAbsLength() const {return GetLength() +1;}
const util::Utf8String &TextLine::GetText() const {return m_line;}
int32_t TextLine::At(CharOffset offset) const {return m_line.at(offset);}
std::optional<int32_t> TextLine::GetChar(CharOffset offset) const
{
	if(offset >= m_line.length())
		return {};
	return m_line.at(offset);
}

void TextLine::Clear() {m_line.clear();}
void TextLine::Reserve(TextLength len) {/*m_line.reserve(len);*/}
util::Utf8StringView TextLine::Substr(CharOffset offset,TextLength len) const
{
	if(offset >= m_line.length())
		return {};
	return util::Utf8StringView{m_line}.substr(offset,len);
}
bool TextLine::CanErase(CharOffset startOffset,TextLength len) const
{
	return startOffset < m_line.size() && len > 0;
}
std::optional<TextLength> TextLine::Erase(CharOffset startOffset,TextLength len,util::Utf8String *outErasedString)
{
	if(outErasedString)
		*outErasedString = "";
	if(CanErase(startOffset,len) == false)
		return {};
	if(m_line.empty())
		return 0;
	auto endOffset = (len < UNTIL_THE_END) ? std::min(startOffset +len -1,m_line.size() -1) : (m_line.size() -1);
	if(outErasedString)
		*outErasedString = m_line.substr(startOffset,endOffset -startOffset +1);
	m_line.erase(m_line.begin() +startOffset,m_line.begin() +endOffset +1);
	return endOffset -startOffset +1;
}

TextLine &TextLine::operator=(const util::Utf8String &line) {m_line = line; return *this;}
bool TextLine::operator==(const util::Utf8StringView &line) {return m_line == line;}
bool TextLine::operator!=(const util::Utf8StringView &line) {return m_line != line;}
TextLine::operator const util::Utf8String&() const {return m_line;}
TextLine::operator const char*() const {return m_line.c_str();}

#ifdef ENABLE_FORMATTED_TEXT_UNIT_TESTS
bool TextLine::Validate(std::stringstream &msg) const
{
	auto it = std::find(m_line.begin(),m_line.end(),'\n');
	if(it != m_line.end())
	{
		msg<<"New-line character found within line! This is not allowed, new-line characters should split the line into multiple lines.";
		return false;
	}
	return true;
}
#endif
