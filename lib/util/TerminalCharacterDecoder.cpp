/*
 This file is part of Konsole, an X terminal.

 Copyright 2006-2008 by Robert Knight <robertknight@gmail.com>

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU Lesser General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU Lesser General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 02110-1301  USA.
*/

#include "TerminalCharacterDecoder.h"
#include "CharWidth.h"
#include <QTextStream>
#include <cwctype>

PlainTextDecoder::PlainTextDecoder()
    : _output(nullptr), _includeTrailingWhitespace(true),
    _recordLinePositions(false) {
}

void PlainTextDecoder::setTrailingWhitespace(bool enable) {
    _includeTrailingWhitespace = enable;
}

bool PlainTextDecoder::trailingWhitespace() const {
    return _includeTrailingWhitespace;
}

void PlainTextDecoder::begin(QTextStream *output) {
    _output = output;
    if (!_linePositions.isEmpty())
        _linePositions.clear();
}

void PlainTextDecoder::end() { 
    _output = nullptr; 
}

void PlainTextDecoder::setRecordLinePositions(bool record) {
    _recordLinePositions = record;
}

QList<int> PlainTextDecoder::linePositions() const { 
    return _linePositions; 
}

void PlainTextDecoder::decodeLine(const Character* const characters, int count, LineProperty /*properties*/) {
    Q_ASSERT(_output);

    if (_recordLinePositions && _output->string()) {
        int pos = _output->string()->size();
        _linePositions << pos;
    }

    if (characters == nullptr) {
        // TODO: So far, this has happened only under kwin_wayland, when the current
        // function is called by TerminalDisplay::inputMethodQuery(). The actual
        // issue should be found.
        return;
    }

    // TODO should we ignore or respect the LINE_WRAPPED line property?

    // note:  we build up a QString and send it to the text stream rather writing
    // into the text stream a character at a time because it is more efficient.
    //(since QTextStream always deals with QStrings internally anyway)
    std::wstring plainText;
    plainText.reserve(count);

    int outputCount = count;

    // if inclusion of trailing whitespace is disabled then find the end of the
    // line
    if (!_includeTrailingWhitespace) {
        for (int i = count - 1; i >= 0; i--) {
            if (!characters[i].isSpace())
                break;
            else
                outputCount--;
        }
    }

    for (int i = 0; i < outputCount;) {
        if (characters[i].rendition & RE_EXTENDED_CHAR) {
            ushort extendedCharLength = 0;
            const uint* chars = ExtendedCharTable::instance.lookupExtendedChar(characters[i].character, extendedCharLength);
            if (chars) {
                std::wstring str;
                for (ushort nchar = 0; nchar < extendedCharLength; nchar++) {
                    str.push_back(chars[nchar]);
                }
                plainText += str;
                i += qMax(1, CharWidth::string_unicode_width(str));
            } else {
                ++i;
            }
        } else {
            plainText.push_back(characters[i].character);
            i += qMax(1, CharWidth::unicode_width(characters[i].character));
        }
    }
    *_output << QString::fromStdWString(plainText);
}

HTMLDecoder::HTMLDecoder()
    : _output(nullptr)
    , _colorTable(base_color_table)
    , _innerSpanOpen(false)
    , _lastRendition(DEFAULT_RENDITION) {
}

void HTMLDecoder::begin(QTextStream *output) {
    _output = output;

    std::wstring text;

    // open monospace span
    openSpan(text, QLatin1String("font-family:monospace"));

    *output << QString::fromStdWString(text);
}

void HTMLDecoder::end() {
    Q_ASSERT(_output);

    std::wstring text;

    closeSpan(text);

    *_output << QString::fromStdWString(text);

    _output = nullptr;
}

// TODO: Support for LineProperty (mainly double width , double height)
void HTMLDecoder::decodeLine(const Character *const characters, int count, LineProperty /*properties*/) {
    Q_ASSERT(_output);

    std::wstring text;

    int spaceCount = 0;

    for (int i = 0; i < count; i++) {
        // check if appearance of character is different from previous char
        if (characters[i].rendition != _lastRendition ||
            characters[i].foregroundColor != _lastForeColor ||
            characters[i].backgroundColor != _lastBackColor) {
            if (_innerSpanOpen)
                closeSpan(text);

            _lastRendition = characters[i].rendition;
            _lastForeColor = characters[i].foregroundColor;
            _lastBackColor = characters[i].backgroundColor;

            // build up style string
            QString style;

            bool useBold;
            ColorEntry::FontWeight weight = characters[i].fontWeight(_colorTable);
            if (weight == ColorEntry::UseCurrentFormat)
                useBold = _lastRendition & RE_BOLD;
            else
                useBold = weight == ColorEntry::Bold;

            if (useBold)
                style.append(QLatin1String("font-weight:bold;"));

            if (_lastRendition & RE_UNDERLINE)
                style.append(QLatin1String("font-decoration:underline;"));

            // colours - a colour table must have been defined first
            if (_colorTable) {
                style.append(QString::fromLatin1("color:%1;")
                                .arg(_lastForeColor.color(_colorTable).name()));

                if (!characters[i].isTransparent(_colorTable)) {
                    style.append(QString::fromLatin1("background-color:%1;")
                                .arg(_lastBackColor.color(_colorTable).name()));
                }
            }

            // open the span with the current style
            openSpan(text, style);
            _innerSpanOpen = true;
        }

        // handle whitespace
        if (characters[i].isSpace())
            spaceCount++;
        else
            spaceCount = 0;

        // output current character
        if (spaceCount < 2) {
            if (characters[i].rendition & RE_EXTENDED_CHAR) {
                ushort extendedCharLength = 0;
                const uint* chars = ExtendedCharTable::instance.lookupExtendedChar(characters[i].character, extendedCharLength);
                if (chars) {
                    for (ushort nchar = 0; nchar < extendedCharLength; nchar++) {
                        text.push_back(chars[nchar]);
                    }
                }
            } else {
                //escape HTML tag characters and just display others as they are
                wchar_t ch(characters[i].character);
                if ( ch == '<' ) {
                    text.append(L"&lt;");
                } else if (ch == '>') {
                    text.append(L"&gt;");
                } else if (ch == '&') {
                    text.append(L"&amp;");
                } else {
                    text.push_back(ch);
                }
            }
        } else {
            // HTML truncates multiple spaces, so use a space marker instead
            // Use &#160 instead of &nbsp so xmllint will work.
            text.append(L"&#160;");
        }
    }

    // close any remaining open inner spans
    if (_innerSpanOpen)
        closeSpan(text);

    // start new line
    text.append(L"<br>");

    *_output << QString::fromStdWString(text);
}

void HTMLDecoder::openSpan(std::wstring &text, const QString &style) {
    text.append(QString(QLatin1String("<span style=\"%1\">")).arg(style).toStdWString());
}

void HTMLDecoder::closeSpan(std::wstring &text) { 
    text.append(L"</span>"); 
}

void HTMLDecoder::setColorTable(const ColorEntry *table) {
    _colorTable = table;
}
