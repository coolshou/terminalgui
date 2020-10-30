#include "tgscrollarea.h"
#include "tgscrollbar.h"

#include <QRect>

Tg::ScrollArea::ScrollArea(Tg::Widget *parent) : Tg::Widget(parent)
{
    init();
}

Tg::ScrollArea::ScrollArea(Tg::Screen *screen) : Tg::Widget(screen)
{
    init();
}

QString Tg::ScrollArea::drawPixel(const QPoint &pixel) const
{
    QString result;

    if (isBorder(pixel)) {
        return drawBorderPixel(pixel);
    } else {
        const auto children = findChildren<Widget *>();
        if (children.isEmpty() == false) {
            // TODO: sort by Z value...
            QList<WidgetPointer> affectedWidgets;
            for (const WidgetPointer &widget : qAsConst(children)) {
                if (widget == _verticalScrollBar
                        || widget == _horizontalScrollBar) {
                    continue;
                }

                // Only draw direct children
                if (widget->parentWidget() != this) {
                    continue;
                }

                const QPoint childPx(childPixel(pixel));
                const QRect boundingRect(widget->boundingRectangle());
                if (widget->visible() && boundingRect.contains(childPx))
                {
                    affectedWidgets.append(widget);
                }
            }

            const QRect contents = contentsRectangle();
            const bool isCorner = (pixel == contents.bottomRight());
            if (isCorner == false) {
                const int borderWidth = effectiveBorderWidth();
                const QPoint adjustedPixel(pixel - QPoint(borderWidth, borderWidth));
                if (_verticalScrollBar->visible() && pixel.x() == contents.right()) {
                    return _verticalScrollBar->drawPixel(adjustedPixel);
                }

                if (_horizontalScrollBar->visible() && pixel.y() == contents.bottom()) {
                    return _horizontalScrollBar->drawPixel(adjustedPixel);
                }
            }

            // TODO: properly handle Z value...
            if (affectedWidgets.isEmpty() == false) {
                WidgetPointer widget = affectedWidgets.last();
                if (widget.isNull() == false) {
                    const QPoint childPx(childPixel(pixel));
                    const QPoint childPos(widget->position());
                    result.append(widget->drawPixel(childPx - childPos));
                    return result;
                }
            }
        }
    }

    // Draw default widget background
    result.append(Terminal::Color::code(Terminal::Color::Predefined::Empty,
                                        backgroundColor()));
    result.append(backgroundCharacter());
    return result;
}

QPoint Tg::ScrollArea::contentsPosition() const
{
    return _contentsPosition;
}

void Tg::ScrollArea::setContentsPosition(const QPoint &contentsPosition)
{
    if (_contentsPosition == contentsPosition)
        return;

    _contentsPosition = contentsPosition;
    emit contentsPositionChanged(_contentsPosition);
}

void Tg::ScrollArea::init()
{
    _verticalScrollBar = new ScrollBar(this);
    _verticalScrollBar->setOrientation(Qt::Orientation::Vertical);
    _verticalScrollBar->setAcceptsFocus(false);

    _horizontalScrollBar = new ScrollBar(this);
    _horizontalScrollBar->setOrientation(Qt::Orientation::Horizontal);
    _horizontalScrollBar->setAcceptsFocus(false);

    setAcceptsFocus(true);

    Widget::init();

    CHECK(connect(this, &ScrollArea::contentsPositionChanged,
                  this, &ScrollArea::schedulePartialRedraw));

    CHECK(connect(this, &ScrollArea::childAdded,
                  this, &ScrollArea::connectChild));
    CHECK(connect(this, &ScrollArea::childAdded,
                  this, &ScrollArea::updateChildrenDimensions));
    CHECK(connect(this, &ScrollArea::childRemoved,
                  this, &ScrollArea::updateChildrenDimensions));

    updateChildrenDimensions();
}

void Tg::ScrollArea::consumeKeyboardBuffer(const QString &keyboardBuffer)
{
    if (keyboardBuffer.contains(Terminal::Key::left)) {
        const int currentX = contentsPosition().x();
        const int hiddenLength = std::abs(currentX);
        const int contentsWidth = contentsRectangle().width();
        const int childrenW = childrenWidth();
        if ((hiddenLength + contentsWidth) < childrenW) {
            QPoint pos = contentsPosition();
            pos.setX(currentX - 1);
            setContentsPosition(pos);
        }
    }

    if (keyboardBuffer.contains(Terminal::Key::right)) {
        const int currentX = contentsPosition().x();
        if (currentX < 0) {
            QPoint pos = contentsPosition();
            pos.setX(currentX + 1);
            setContentsPosition(pos);
        }
    }

    if (keyboardBuffer.contains(Terminal::Key::up)) {
        const int currentY = contentsPosition().y();
        const int hiddenLength = std::abs(currentY);
        const int contentsHeight = contentsRectangle().height();
        const int childrenH = childrenHeight();
        if ((hiddenLength + contentsHeight) < childrenH) {
            QPoint pos = contentsPosition();
            pos.setY(currentY - 1);
            setContentsPosition(pos);
        }
    }

    if (keyboardBuffer.contains(Terminal::Key::down)) {
        const int currentY = contentsPosition().y();
        if (currentY < 0) {
            QPoint pos = contentsPosition();
            pos.setY(currentY + 1);
            setContentsPosition(pos);
        }
    }
}

void Tg::ScrollArea::updateChildrenDimensions()
{
    const int oldWidth = _childrenWidth;
    const int oldHeight = _childrenHeight;

    _childrenWidth = 0;
    _childrenHeight = 0;

    const auto widgets = findChildren<Widget *>();
    for (const Widget *widget : widgets) {
        if (widget == _verticalScrollBar || widget == _horizontalScrollBar) {
            continue;
        }

        if (widget) {
            _childrenWidth = std::max(_childrenWidth,
                                      widget->boundingRectangle().right());
            _childrenHeight = std::max(_childrenHeight,
                                       widget->boundingRectangle().bottom());
        }
    }

    if (oldWidth != _childrenWidth || oldHeight != _childrenHeight) {
        updateScrollBarStates();
        schedulePartialRedraw();
    }
}

void Tg::ScrollArea::connectChild(Tg::Widget *child)
{
    CHECK(connect(child, &Widget::positionChanged,
                  this, &ScrollArea::updateChildrenDimensions));
    CHECK(connect(child, &Widget::sizeChanged,
                  this, &ScrollArea::updateChildrenDimensions));
}

QPoint Tg::ScrollArea::childPixel(const QPoint &pixel) const
{
    const int borderWidth = effectiveBorderWidth();
    const QPoint adjustedPixel(pixel - QPoint(borderWidth, borderWidth));
    const QPoint reversed(contentsPosition() * -1);
    const QPoint result(reversed + adjustedPixel);
    return result;
}

int Tg::ScrollArea::childrenWidth() const
{
    return _childrenWidth;
}

int Tg::ScrollArea::childrenHeight() const
{
    return _childrenHeight;
}

void Tg::ScrollArea::updateScrollBarStates()
{
    if (_verticalScrollBarPolicy == ScrollBarPolicy::NeverShow
            && _horizontalScrollBarPolicy == ScrollBarPolicy::NeverShow) {
        _verticalScrollBar->setVisible(false);
        _horizontalScrollBar->setVisible(false);
        return;
    }

    const QRect contents = contentsRectangle();
    _verticalScrollBar->setPosition(QPoint(contents.right() - 1, 0));
    _verticalScrollBar->setSize(QSize(1, contents.height() - 1));

    _horizontalScrollBar->setPosition(QPoint(0, contents.bottom() - 1));
    _horizontalScrollBar->setSize(QSize(contents.width() - 1, 1));

    if (_verticalScrollBarPolicy == ScrollBarPolicy::AlwaysShow) {
        _verticalScrollBar->setVisible(true);
    }

    if (_horizontalScrollBarPolicy == ScrollBarPolicy::AlwaysShow) {
        _horizontalScrollBar->setVisible(true);
    }

    if (_verticalScrollBarPolicy == ScrollBarPolicy::AlwaysShow
            && _horizontalScrollBarPolicy == ScrollBarPolicy::AlwaysShow) {
        return;
    }

    if (childrenWidth() > (contents.width() - 1)
            && _horizontalScrollBarPolicy != ScrollBarPolicy::NeverShow) {
        _horizontalScrollBar->setVisible(true);
        _horizontalScrollBar->setMinimum(0);
        _horizontalScrollBar->setMaximum(childrenWidth());
        //const qreal absolutePos = childrenWidth() / contentsPosition().x();
        //_horizontalScrollBar->setSliderPosition(
        //            absolutePos * _horizontalScrollBar->size().width());
    }

    if (childrenHeight() > (contents.height() - 1)
            && _verticalScrollBarPolicy != ScrollBarPolicy::NeverShow) {
        _verticalScrollBar->setVisible(true);
        _verticalScrollBar->setMinimum(0);
        _verticalScrollBar->setMaximum(childrenHeight());
        //const qreal absolutePos = childrenHeight() / contentsPosition().y();
        //_verticalScrollBar->setSliderPosition(
        //            absolutePos * _verticalScrollBar->size().height());
    }
}
