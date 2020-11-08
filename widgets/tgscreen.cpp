#include "tgscreen.h"
#include "widgets/tgwidget.h"
#include "widgets/tgbutton.h"
#include "styles/tgstyle.h"

#include <tgterminal.h>

#include <QPoint>
#include <QRect>
#include <QDebug>

Tg::Screen::Screen(QObject *parent) : QObject(parent)
{
    _style = StylePointer::create();

    _terminal = new Terminal(this);
    setSize(_terminal->size());

    CHECK(connect(_terminal, &Terminal::sizeChanged,
                  this, &Screen::setSize));

    _keyboardTimer.setInterval(100);
    _keyboardTimer.setSingleShot(false);

    CHECK(connect(&_keyboardTimer, &QTimer::timeout,
                  this, &Screen::checkKeyboard));

    _redrawTimer.setInterval(32);
    _redrawTimer.setSingleShot(true);

    CHECK(connect(&_redrawTimer, &QTimer::timeout,
                  this, &Screen::redrawImmediately));
}

Tg::Screen::~Screen()
{
}

QSize Tg::Screen::size() const
{
    return _size;
}

void Tg::Screen::registerWidget(Tg::Widget *widget)
{
    _widgets.append(widget);
    widget->setStyle(_style, true);

    CHECK(connect(widget, &Widget::moveFocusToPreviousWidget,
                  this, &Screen::moveFocusToPreviousWidget));
    CHECK(connect(widget, &Widget::moveFocusToNextWidget,
                  this, &Screen::moveFocusToNextWidget));

    if (widget->acceptsFocus() && _activeFocusWidget.isNull()) {
        _activeFocusWidget = widget;
        widget->setHasFocus(true);

        if (_keyboardTimer.isActive() == false) {
            _keyboardTimer.start();
        }
    }
}

void Tg::Screen::deregisterWidget(Tg::Widget *widget)
{
    _widgets.removeOne(widget);
}

Tg::StylePointer Tg::Screen::style() const
{
    return _style;
}

void Tg::Screen::onNeedsRedraw(const RedrawType type, const Widget *widget)
{
    updateRedrawRegions(type, widget);
    compressRedraws();
}

void Tg::Screen::moveFocusToPreviousWidget()
{
    QListIterator<WidgetPointer> iterator(_widgets);
    const bool result = iterator.findNext(_activeFocusWidget);
    if (result == false) {
        clearActiveFocusWidget();
        return;
    }

    // Search from current focus widget backward
    while (iterator.hasPrevious()) {
        const WidgetPointer widget = iterator.previous();
        if (widget != _activeFocusWidget && widget->acceptsFocus()) {
            setActiveFocusWidget(widget);
            return;
        }
    }

    // Search from back up to current focus widget
    iterator.toBack();

    while (iterator.hasPrevious()) {
        const WidgetPointer widget = iterator.previous();
        if (widget == _activeFocusWidget) {
            return;
        } else if (widget->acceptsFocus()) {
            setActiveFocusWidget(widget);
            return;
        }
    }
}

void Tg::Screen::moveFocusToNextWidget()
{
    QListIterator<WidgetPointer> iterator(_widgets);
    const bool result = iterator.findNext(_activeFocusWidget);
    if (result == false) {
        clearActiveFocusWidget();
        return;
    }

    // Search from current focus widget forward
    while (iterator.hasNext()) {
        const WidgetPointer widget = iterator.next();
        if (widget != _activeFocusWidget && widget->acceptsFocus()) {
            setActiveFocusWidget(widget);
            return;
        }
    }

    // Search from root up to current focus widget
    iterator.toFront();

    while (iterator.hasNext()) {
        const WidgetPointer widget = iterator.next();
        if (widget == _activeFocusWidget) {
            return;
        } else if (widget->acceptsFocus()) {
            setActiveFocusWidget(widget);
            return;
        }
    }
}

void Tg::Screen::redrawImmediately()
{
    QTextStream stream(stdout);
#if QT_VERSION_MAJOR < 6
    stream.setCodec("UTF-8");
#else
    stream.setEncoding(QStringConverter::Utf8);
#endif
    stream.setAutoDetectUnicode(true);

    QVector<QPoint> points;
    for (const QRect &region : qAsConst(_redrawRegions)) {
        for (int y = region.y(); y < (region.y() + region.height()); ++y) {
            for (int x = region.x(); x < (region.x() + region.width()); ++x) {
                const QPoint pixel(x, y);

                if (points.contains(pixel)) {
                    continue;
                }

                // TODO: sort by Z value...
                QList<WidgetPointer> affectedWidgets;
                for (const WidgetPointer &widget : qAsConst(_widgets)) {
                    // Only draw direct children
                    if (widget->parentWidget() != nullptr) {
                        // TODO: get top-level parent and append it to
                        // affectedWidgets
                        continue;
                    }

                    if (widget->visible()
                            && widget->globalBoundingRectangle().contains(pixel))
                    {
                        affectedWidgets.append(widget);
                    }
                }

                // TODO: consider using Terminal::currentPosition() to
                // prevent move operation if it's not needed. This could
                // speed things up (or slow them down...)
                stream << Tg::Command::moveToPosition(x, y);

                bool drawn = false;
                // TODO: properly handle Z value...
                if (affectedWidgets.isEmpty() == false) {
                    WidgetPointer widget = affectedWidgets.last();
                    if (widget.isNull() == false) {
                        const QPoint localPixel(widget->mapFromGlobal(pixel));
                        stream << widget->drawPixel(localPixel);
                        drawn = true;
                    }
                }

                if (drawn == false) {
                    stream << style()->screenBackground;
                    stream << Tg::Color::end();
                }

                points.append(pixel);
            }
        }
    }

    // Reset cursor to bottom-right corner
    stream << Tg::Color::end();
    stream << Tg::Command::moveToPosition(size().width(), size().height());
    _redrawRegions.clear();
}

void Tg::Screen::checkKeyboard()
{
    const int bufferSize = Terminal::keyboardBufferSize();
    if (bufferSize <= 0) {
        return;
    }

    QString characters;
    for (int i = 0; i < bufferSize; ++i) {
        characters.append(Terminal::getChar());
    }

    if (characters.contains(Command::mouseEventBegin)) {
        const int clickIndex = characters.indexOf(Command::mouseClick);
        const int clickEnd = characters.indexOf(Command::mouseReleaseSuffix, clickIndex);
        if (clickIndex != -1 && clickEnd != -1) {
            const int positionBeginning = clickIndex + Command::mouseClick.length();
            const int positionLength = clickEnd - positionBeginning;
            const QString positionString = characters.mid(
                        positionBeginning, positionLength);
            const QStringList strings = positionString.split(Command::separator);
            const QPoint click(strings.at(0).toInt(), strings.at(1).toInt());

            QListIterator<WidgetPointer> iterator(_widgets);
            while (iterator.hasNext()) {
                const WidgetPointer widget = iterator.next();
                if (widget->acceptsFocus()
                        && widget->globalBoundingRectangle().contains(click))
                {
                    auto button = qobject_cast<Button*>(widget);
                    if (button) {
                        button->click();
                    }

                    setActiveFocusWidget(widget);

                    return;
                }
            }
        }
    }

    if (_activeFocusWidget) {
        if (const QString command(Tg::Key::tab);
                characters.contains(command)) {
            // Move to next input
            moveFocusToNextWidget();
            characters.remove(command);
        }

        if (_activeFocusWidget->verticalArrowsMoveFocus()) {
            // Up Arrow
            if (const QString command(Tg::Key::up);
                    characters.contains(command)) {
                moveFocusToPreviousWidget();
                characters.remove(command);
            }

            // Down Arrow
            if (const QString command(Tg::Key::down);
                    characters.contains(command)) {
                moveFocusToNextWidget();
                characters.remove(command);
            }
        }


        _activeFocusWidget->consumeKeyboardBuffer(characters);
    }
}

void Tg::Screen::setSize(const QSize &size)
{
    if (_size != size) {
        _size = size;
        emit sizeChanged(size);
    }
}

void Tg::Screen::updateRedrawRegions(const RedrawType type,
                                     const Widget *widget)
{
    if (type == RedrawType::Full) {
        _redrawRegions.clear();
        const QRect region(QPoint(0, 0), size());
        _redrawRegions.append(region);
    } else {
        if (type == RedrawType::PreviousPosition) {
            updateRedrawRegion(widget->globalPreviousBoundingRectangle());
        }
        updateRedrawRegion(widget->globalBoundingRectangle());
    }
}

void Tg::Screen::updateRedrawRegion(const QRect &region)
{
    bool update = true;
    const auto originalRegions = _redrawRegions;
    for (const QRect &current : originalRegions) {
        if (current.contains(region, true)) {
            update = false;
            break;
        }

        // TODO: add more fine-grained control and optimisation of
        // intersecting draw regions
    }

    if (update) {
        _redrawRegions.append(region);
    }
}

void Tg::Screen::compressRedraws()
{
    if (_redrawTimer.isActive() == false) {
        _redrawTimer.start();
    }
}

void Tg::Screen::setActiveFocusWidget(const Tg::WidgetPointer &widget)
{
    _activeFocusWidget->setHasFocus(false);
    _activeFocusWidget = widget;
    _activeFocusWidget->setHasFocus(true);
}

void Tg::Screen::clearActiveFocusWidget()
{
    _activeFocusWidget->setHasFocus(false);
    _activeFocusWidget = nullptr;
}
