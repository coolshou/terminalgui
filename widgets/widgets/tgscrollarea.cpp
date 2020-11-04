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

        if (result.isEmpty()) {
            result = drawAreaContents(pixel);
            if (result.isEmpty() == false) {
                return result;
            }
        }
    }

    // Draw default widget background
    result.append(Terminal::Color::code(Terminal::Color::Predefined::Empty,
                                        backgroundColor()));
    result.append(backgroundCharacter());
    return result;
}

QString Tg::ScrollArea::drawAreaContents(const QPoint &pixel) const
{
    Q_UNUSED(pixel);
    return {};
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
    updateScrollBarPositions();
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
    if (keyboardBuffer.contains(Terminal::Key::right)) {
        const int currentX = contentsPosition().x();
        const int hiddenLength = std::abs(currentX);
        const int contentsWidth = scrollableArea().width();
        const int childrenW = childrenWidth();
        if ((hiddenLength + contentsWidth) < childrenW) {
            QPoint pos = contentsPosition();
            pos.setX(currentX - 1);
            setContentsPosition(pos);
        }
    }

    if (keyboardBuffer.contains(Terminal::Key::left)) {
        const int currentX = contentsPosition().x();
        if (currentX < 0) {
            QPoint pos = contentsPosition();
            pos.setX(currentX + 1);
            setContentsPosition(pos);
        }
    }

    if (keyboardBuffer.contains(Terminal::Key::down)) {
        const int currentY = contentsPosition().y();
        const int hiddenLength = std::abs(currentY);
        const int contentsHeight = scrollableArea().height();
        const int childrenH = childrenHeight();
        if ((hiddenLength + contentsHeight) < childrenH) {
            QPoint pos = contentsPosition();
            pos.setY(currentY - 1);
            setContentsPosition(pos);
        }
    }

    if (keyboardBuffer.contains(Terminal::Key::up)) {
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
            const QRect rect = widget->boundingRectangle();
            _childrenWidth = std::max(_childrenWidth, rect.x() + rect.width());
            _childrenHeight = std::max(_childrenHeight, rect.y() + rect.height());
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

QRect Tg::ScrollArea::scrollableArea() const
{
    QRect result = contentsRectangle();
    if (_horizontalScrollBar->visible()) {
        result.setHeight(result.height() - _horizontalScrollBar->size().height());
    }

    if (_verticalScrollBar->visible()) {
        result.setWidth(result.width() - _verticalScrollBar->size().width());
    }

    return result;
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

    if (childrenWidth() >= (contents.width() - 1)
            && _horizontalScrollBarPolicy != ScrollBarPolicy::NeverShow) {
        _horizontalScrollBar->setVisible(true);
    }

    if (childrenHeight() >= (contents.height() - 1)
            && _verticalScrollBarPolicy != ScrollBarPolicy::NeverShow) {
        _verticalScrollBar->setVisible(true);
    }

    const QRect scrollable = scrollableArea();
    _horizontalScrollBar->setMinimum(0);
    _horizontalScrollBar->setMaximum(childrenWidth() - scrollable.width());
    _verticalScrollBar->setMinimum(0);
    _verticalScrollBar->setMaximum(childrenHeight() - scrollable.height());
    updateScrollBarPositions();
}

void Tg::ScrollArea::updateScrollBarPositions()
{
    _horizontalScrollBar->setSliderPosition(std::abs(contentsPosition().x()));
    _verticalScrollBar->setSliderPosition(std::abs(contentsPosition().y()));

}
