#include "tglabel.h"
#include "tgscreen.h"
#include "textstream.h"
#include "tghelpers.h"

#include <backend/backend.h>

#include <QRect>
#include <QDebug>

Tg::Label::Label(Tg::Widget *parent) : Tg::Widget(parent)
{
    init();
}

Tg::Label::Label(Tg::Screen *screen) : Tg::Widget(screen)
{
    init();
}

Tg::Label::Label(const QString &text, Widget *parent) : Tg::Widget(parent)
{
    setText(text);
    init();
}

Tg::Label::Label(const QString &text, Tg::Screen *screen) : Tg::Widget(screen)
{
    setText(text);
    init();
}

QString Tg::Label::text() const
{
    return _text;
}

std::string Tg::Label::drawPixel(const QPoint &pixel) const
{
    if (isBorder(pixel)) {
        return drawBorderPixel(pixel);
    }

    std::string result;
    result.append(Terminal::colorCode(textColor(), backgroundColor()));

    const QRect contents = contentsRectangle();
    const int charX = pixel.x() - contents.x();
    const int charY = pixel.y() - contents.y();

    const QStringList wrappedText(_laidOutTextCache);
    const QString line(wrappedText.at(charY));

    if (line.size() > charX) {
        result.push_back(line.at(charX).unicode());
    } else {
        result.push_back(' ');
    }

    return result;
}

void Tg::Label::setText(const QString &text, const bool expand)
{
    if (_text == text)
        return;

    _text = text;

    QSize current(size());
    if (expand && current.width() != _text.length()) {
        current.setWidth(text.length());
        current.setHeight(1);
        setSize(current);
    }

    emit textChanged(_text);
}

void Tg::Label::init()
{
    CHECK(connect(this, &Label::needsRedraw,
                  this, &Label::layoutText));
    CHECK(connect(this, &Label::textChanged,
                  this, &Label::scheduleRedraw));

    Widget::init();
}

void Tg::Label::layoutText()
{
    _laidOutTextCache.clear();

    const QRect contents = contentsRectangle();
    const int width = contents.width();
    const int height = contents.height();

    if (text().size() <= width) {
        QString txt = text();
        while (txt.length() < width) {
            // Fill with spaces
            txt.append(' ');
        }
        _laidOutTextCache.append(text());
        return;
    } else {
        int currentX = 0;
        int currentY = 0;
        QString currentString;
        for (const QChar &character : text()) {
            if (currentY > height) {
                break;
            }

            if (currentX < width) {
                currentString.append(character);
                currentX++;
            } else {
                _laidOutTextCache.append(currentString);
                currentString.clear();
                currentY++;
                currentString.append(character);
                // One because one character is already added, in line above
                currentX = 1;
            }
        }

        while (currentString.length() < width) {
            // Fill with spaces
            currentString.append(' ');
        }
        _laidOutTextCache.append(currentString);
    }
}
