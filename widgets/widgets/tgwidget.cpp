#include "tgwidget.h"
#include "tgscreen.h"
#include "layouts/tgchildfillsparentlayout.h"
#include "layouts/tgcolumnlayout.h"
#include "layouts/tgrowlayout.h"
#include "layouts/tggridlayout.h"
#include "styles/tgstyle.h"

#include <QRect>
#include <QDebug>

Tg::Widget::Widget(Widget *parent)
    : QObject(parent),
      _screen(parent->screen()),
      _parentWidget(parent)
{
    init();
}

Tg::Widget::Widget(Tg::Screen *parentScreen)
    : QObject(parentScreen),
      _screen(parentScreen),
      _parentWidget(nullptr)
{
    init();
}

Tg::Widget::~Widget()
{
    if (_screen) {
        _screen->deregisterWidget(this);
    }
}

QPoint Tg::Widget::position() const
{
    return _position;
}

QPoint Tg::Widget::previousGlobalPosition() const
{
    return _previousGlobalPosition;
}

QSize Tg::Widget::size() const
{
    return _size;
}

QSize Tg::Widget::previousSize() const
{
    return _previousSize;
}

QRect Tg::Widget::boundingRectangle() const
{
    return QRect(position(), size());
}

QRect Tg::Widget::globalBoundingRectangle() const
{
    if (parentWidget()) {
        const int border = parentWidget()->effectiveBorderWidth();
        return QRect(mapToGlobal(QPoint(border, border)), size());
    } else {
        return QRect(mapToGlobal(QPoint(0, 0)), size());
    }
}

QRect Tg::Widget::globalPreviousBoundingRectangle() const
{
    if (parentWidget()) {
        const int border = parentWidget()->effectiveBorderWidth();
        return QRect(previousGlobalPosition() + QPoint(border, border),
                     previousSize());
    } else {
        return QRect(previousGlobalPosition(), previousSize());
    }
}

QRect Tg::Widget::contentsRectangle() const
{
    const int borderWidth = effectiveBorderWidth();
    const QPoint pos(borderWidth, borderWidth);

    QSize siz = size();
    const int twiceBorderWidth = 2 * borderWidth;
    siz.setWidth(siz.width() - twiceBorderWidth);
    siz.setHeight(siz.height() - twiceBorderWidth);
    return QRect(pos, siz);
}

Terminal::Color Tg::Widget::backgroundColor() const
{
    if (_backgroundColor.isEmpty()) {
        return style()->backgroundColor;
    } else {
        return _backgroundColor;
    }
}

QChar Tg::Widget::backgroundCharacter() const
{
    if (_backgroundCharacter.isNull()) {
        return style()->backgroundCharacter;
    } else {
        return _backgroundCharacter;
    }
}

Terminal::Color Tg::Widget::textColor() const
{
    if (_textColor.isEmpty()) {
        return style()->textColor;
    } else {
        return _textColor;
    }
}

Terminal::Color Tg::Widget::borderTextColor() const
{
    if (_borderTextColor.isEmpty()) {
        return style()->border->textColor;
    } else {
        return _borderTextColor;
    }
}

Terminal::Color Tg::Widget::borderBackgroundColor() const
{
    if (_borderBackgroundColor.isEmpty()) {
        return style()->border->backgroundColor;
    } else {
        return _borderBackgroundColor;
    }
}

bool Tg::Widget::visible() const
{
    return _visible;
}

bool Tg::Widget::borderVisible() const
{
    return _borderVisible;
}

bool Tg::Widget::acceptsFocus() const
{
    return _acceptsFocus;
}

bool Tg::Widget::hasFocus() const
{
    return _hasFocus;
}

void Tg::Widget::setAcceptsFocus(const bool acceptsFocus)
{
    if (_acceptsFocus != acceptsFocus) {
        _acceptsFocus = acceptsFocus;
        emit acceptsFocusChanged(acceptsFocus);
    }
}

void Tg::Widget::setHasFocus(const bool hasFocus)
{
    if (_hasFocus != hasFocus) {
        _hasFocus = hasFocus;
        emit hasFocusChanged(hasFocus);
    }
}

Tg::Screen *Tg::Widget::screen() const
{
    return _screen;
}

Tg::Widget *Tg::Widget::parentWidget() const
{
    return _parentWidget;
}

QString Tg::Widget::drawBorderPixel(const QPoint &pixel) const
{
    QString result;

    const auto color = Terminal::Color::code(borderTextColor(), borderBackgroundColor());
    // TODO: add property to make overshoot color customizable
    const auto overshootColor = Terminal::Color::code(style()->border->overshootTextColor,
                                                      style()->border->overshootBackgroundColor);

    const QRect rect(QPoint(0, 0), size());
    if (pixel == rect.topLeft()) {
        result.append(color);
        result.append(style()->border->topLeft);
    } else if (pixel == rect.topRight()) {
        result.append(color);
        result.append(style()->border->topRight);
    } else if (pixel == rect.bottomLeft()) {
        result.append(color);
        result.append(style()->border->bottomLeft);
    } else if (pixel == rect.bottomRight()) {
        result.append(color);
        result.append(style()->border->bottomRight);
    } else if (pixel.y() == rect.top()) {
        result.append(color);
        result.append(style()->border->horizontal);
    } else if (pixel.y() == rect.bottom()) {
        if (widgetOvershoot().testFlag(Overshoot::Vertical)
                || layoutOvershoot().testFlag(Overshoot::Vertical)) {
            result.append(overshootColor);
            result.append(style()->border->horizontalOvershoot);
        } else {
            result.append(color);
            result.append(style()->border->horizontal);
        }
    } else if (pixel.x() == rect.left()) {
        result.append(color);
        result.append(style()->border->vertical);
    } else if (pixel.x() == rect.right()) {
        if (widgetOvershoot().testFlag(Overshoot::Horizontal)
                || layoutOvershoot().testFlag(Overshoot::Horizontal)) {
            result.append(overshootColor);
            result.append(style()->border->verticalOvershoot);
        } else {
            result.append(color);
            result.append(style()->border->vertical);
        }
    } else {
        result.append(Terminal::Key::space);
    }

    return result;
}

QString Tg::Widget::drawPixel(const QPoint &pixel) const
{
    QString result;

    if (isBorder(pixel)) {
        return drawBorderPixel(pixel);
    } else {
        const auto children = findChildren<Widget *>();
        if (children.isEmpty() == false) {
            const QPoint contentsPixel(pixel - contentsRectangle().topLeft());

            // TODO: sort by Z value...
            QList<WidgetPointer> affectedWidgets;
            for (const WidgetPointer &widget : qAsConst(children)) {
                // Only draw direct children
                if (widget->parentWidget() != this) {
                    continue;
                }

                if (widget->visible()
                        && widget->boundingRectangle().contains(contentsPixel))
                {
                    affectedWidgets.append(widget);
                }
            }

            // TODO: properly handle Z value...
            if (affectedWidgets.isEmpty() == false) {
                WidgetPointer widget = affectedWidgets.last();
                if (widget.isNull() == false) {
                    const QPoint childPixel(mapToChild(widget, pixel));
                    result.append(widget->drawPixel(childPixel));
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

bool Tg::Widget::isBorder(const QPoint &pixel) const
{
    if (borderVisible() == false) {
        return false;
    }

    const QRect rect(QPoint(0, 0), size());
    if (pixel.x() == rect.left() || pixel.x() == rect.right()) {
        return true;
    }

    if (pixel.y() == rect.top() || pixel.y() == rect.bottom()) {
        return true;
    }

    return false;
}

bool Tg::Widget::verticalArrowsMoveFocus() const
{
    return _verticalArrowsMoveFocus;
}

QPoint Tg::Widget::mapFromGlobal(const QPoint &position) const
{
    const QPoint result(position - this->position());

    if (parentWidget()) {
        const int border = parentWidget()->effectiveBorderWidth();
        return parentWidget()->mapFromGlobal(result) -
            QPoint(border, border);
    }

    return result;
}

QPoint Tg::Widget::mapToGlobal(const QPoint &position) const
{
    const QPoint result(position + this->position());

    if (parentWidget()) {
        return parentWidget()->mapToGlobal(result);
    }

    return result;
}

QPoint Tg::Widget::mapToChild(const Tg::WidgetPointer &child,
                              const QPoint &position) const
{
    const QPoint childPos(child->position());
    const QPoint topLeft(contentsRectangle().topLeft());
    const QPoint result(position - childPos - topLeft);
    return result;
}

bool Tg::Widget::propagatesStyle() const
{
    return _propagatesStyle;
}

void Tg::Widget::setStyle(const Tg::StylePointer &style, const bool propagate)
{
    if (style == _style && propagatesStyle() == propagate) {
        return;
    }

    _style = style;
    setPropagatesStyle(propagate);

    if (propagate) {
        for (auto child : children()) {
            auto widget = qobject_cast<Widget*>(child);
            if (widget) {
                propagateStyleToChild(widget);
            }
        }
    }

    emit styleChanged();
}

Tg::Layout::Type Tg::Widget::layoutType() const
{
    return _layout->type;
}

void Tg::Widget::setLayoutType(const Tg::Layout::Type type)
{
    if (_layout) {
        if (_layout->type == type) {
            return;
        } else {
            delete _layout;
        }
    }

    switch (type) {
    case Layout::Type::None:
        _layout = new Layout;
        break;
    case Layout::Type::ChildFillsParent:
        _layout = new ChildFillsParentLayout;
        break;
    case Layout::Type::Column:
        _layout = new ColumnLayout;
        break;
    case Layout::Type::Row:
        _layout = new RowLayout;
        break;
    case Layout::Type::Grid:
        _layout = new GridLayout;
        break;
    }

    _layout->parent = this;
    doLayout();
}

void Tg::Widget::doLayout()
{
    if (_layout) {
        _layout->doLayout();

        const auto overshoot = _layout->overshoot();
        if (overshoot != _layoutOvershoot) {
            _layoutOvershoot = overshoot;
            _widgetOvershoot = overshoot;
            emit layoutOvershootChanged(_layoutOvershoot);
            emit widgetOvershootChanged(_widgetOvershoot);
        }
    }
}

Tg::SizeOvershoot Tg::Widget::layoutOvershoot() const
{
    return _layoutOvershoot;
}

Tg::SizeOvershoot Tg::Widget::widgetOvershoot() const
{
    return _widgetOvershoot;
}

void Tg::Widget::setPosition(const QPoint &position)
{
    if (_position == position)
        return;

    if (parentWidget() == nullptr) {
        if (position.x() == 0) {
            qWarning() << "Minimal x coordinate on terminal is 1, not 0";
        }

        if (position.y() == 0) {
            qWarning() << "Minimal y coordinate on terminal is 1, not 0";
        }
    }

    updatePreviousBoundingRect();
    _position = position;
    emit positionChanged(_position);
}

void Tg::Widget::setSize(const QSize &size)
{
    if (_size == size)
        return;

    updatePreviousBoundingRect();
    _size = size;
    emit sizeChanged(_size);

    doLayout();
}

void Tg::Widget::setBackgroundColor(const Terminal::Color &backgroundColor)
{
    if (_backgroundColor == backgroundColor)
        return;

    _backgroundColor = backgroundColor;
    emit backgroundColorChanged(_backgroundColor);
}

void Tg::Widget::setBackgroundCharacter(const QChar &backgroundCharacter)
{
    if (_backgroundCharacter == backgroundCharacter)
        return;

    _backgroundCharacter = backgroundCharacter;
    emit backgroundCharacterChanged(_backgroundCharacter);
}

void Tg::Widget::setTextColor(const Terminal::Color &textColor)
{
    if (_textColor == textColor)
        return;

    _textColor = textColor;
    emit textColorChanged(_textColor);
}

void Tg::Widget::setBorderTextColor(const Terminal::Color &borderColor)
{
    if (_borderTextColor == borderColor)
        return;

    _borderTextColor = borderColor;
    emit borderTextColorChanged(_borderTextColor);
}

void Tg::Widget::setBorderBackgroundColor(const Terminal::Color &borderBackgroundColor)
{
    if (_borderBackgroundColor == borderBackgroundColor)
        return;

    _borderBackgroundColor = borderBackgroundColor;
    emit borderBackgroundColorChanged(_borderBackgroundColor);
}

void Tg::Widget::setVisible(const bool visible)
{
    if (_visible == visible)
        return;

    _visible = visible;

    emit visibleChanged(_visible);

    if (visible && parentWidget()) {
        parentWidget()->doLayout();

        if (parentWidget()->propagatesStyle()) {
            parentWidget()->propagateStyleToChild(this);
        }
    }
}

void Tg::Widget::show()
{
    setVisible(true);
}

void Tg::Widget::hide()
{
    setVisible(false);
}

void Tg::Widget::setBorderVisible(const bool borderVisible)
{
    if (_borderVisible == borderVisible)
        return;

    _borderVisible = borderVisible;
    emit borderVisibleChanged(_borderVisible);
}

int Tg::Widget::effectiveBorderWidth() const
{
    if (borderVisible()) {
        return _borderWidth;
    }

    return 0;
}

void Tg::Widget::init()
{
    CHECK(connect(this, &Widget::positionChanged,
                  this, &Widget::schedulePreviousPositionRedraw));
    CHECK(connect(this, &Widget::sizeChanged,
                  this, &Widget::schedulePreviousPositionRedraw));

    CHECK(connect(this, &Widget::backgroundColorChanged,
                  this, &Widget::schedulePartialRedraw));
    CHECK(connect(this, &Widget::backgroundCharacterChanged,
                  this, &Widget::schedulePartialRedraw));
    CHECK(connect(this, &Widget::textColorChanged,
                  this, &Widget::schedulePartialRedraw));
    CHECK(connect(this, &Widget::borderTextColorChanged,
                  this, &Widget::schedulePartialRedraw));
    CHECK(connect(this, &Widget::visibleChanged,
                  this, &Widget::schedulePartialRedraw));
    CHECK(connect(this, &Widget::borderVisibleChanged,
                  this, &Widget::schedulePartialRedraw));
    CHECK(connect(this, &Widget::hasFocusChanged,
                  this, &Widget::schedulePartialRedraw));
    CHECK(connect(this, &Widget::styleChanged,
                  this, &Widget::schedulePartialRedraw));
    CHECK(connect(this, &Widget::layoutOvershootChanged,
                  this, &Widget::schedulePartialRedraw));
    CHECK(connect(this, &Widget::widgetOvershootChanged,
                  this, &Widget::schedulePartialRedraw));

    if (_screen) {
        CHECK(connect(this, &Widget::needsRedraw,
                      _screen, &Screen::onNeedsRedraw));
        _screen->registerWidget(this);
    } else {
        qCritical() << "Screen is missing, can't draw the widget!" << this;
    }

    if (_parentWidget) {
        setBorderVisible(false);

        _parentWidget->doLayout();
        if (_parentWidget->propagatesStyle()) {
            _parentWidget->propagateStyleToChild(this);
        }
    }

    setLayoutType(Layout::Type::None);
}

void Tg::Widget::consumeKeyboardBuffer(const QString &keyboardBuffer)
{
    Q_UNUSED(keyboardBuffer)
}

void Tg::Widget::setVerticalArrowsMoveFocus(const bool verticalArrowsMoveFocus)
{
    _verticalArrowsMoveFocus = verticalArrowsMoveFocus;
}

void Tg::Widget::setPropagatesStyle(const bool propagatesStyle)
{
    if (propagatesStyle != _propagatesStyle) {
        _propagatesStyle = propagatesStyle;
        emit propagatesStyleChanged(propagatesStyle);
    }
}

void Tg::Widget::propagateStyleToChild(Widget *child) const
{
    if (child) {
        child->setStyle(style(), true);
    }
}

Tg::StylePointer Tg::Widget::style() const
{
    return _style;
}

void Tg::Widget::setWidgetOvershoot(const SizeOvershoot overshoot)
{
    if (_widgetOvershoot != overshoot) {
        _widgetOvershoot = overshoot;
        emit widgetOvershootChanged(overshoot);
    }
}

void Tg::Widget::setupPressTimer(QTimer *timer) const
{
    timer->setInterval(500);
    timer->setSingleShot(true);
}

void Tg::Widget::scheduleFullRedraw() const
{
    emit needsRedraw(RedrawType::Full, this);
}

void Tg::Widget::schedulePartialRedraw() const
{
    emit needsRedraw(RedrawType::Partial, this);
}

void Tg::Widget::schedulePreviousPositionRedraw() const
{
    emit needsRedraw(RedrawType::PreviousPosition, this);
}

bool Tg::Widget::canRedraw() const
{
    if (visible() == true) {
        return true;
    }

    return false;
}

void Tg::Widget::updatePreviousBoundingRect()
{
    _previousGlobalPosition = _position;
    _previousSize = _size;
}
