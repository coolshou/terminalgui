#include "tgbutton.h"

Tg::Button::Button(Tg::Widget *parent) : Tg::Label(parent)
{
    init();
}

Tg::Button::Button(Tg::Screen *screen) : Tg::Label(screen)
{
    init();
}

Tg::Button::Button(const QString &text, Tg::Widget *parent) : Tg::Label(text, parent)
{
    init();
}

Tg::Button::Button(const QString &text, Tg::Screen *screen) : Tg::Label(text, screen)
{
    init();
}

void Tg::Button::init()
{
    setAcceptsFocus(true);
    Label::init();

    _buttonPressTimer.setInterval(500);
    _buttonPressTimer.setSingleShot(true);

    connect(this, &Button::clicked,
            &_buttonPressTimer, qOverload<>(&QTimer::start));
    connect(&_buttonPressTimer, &QTimer::timeout,
            this, &Button::onButtonPressTimeout);

    setBackgroundColor(Terminal::Color4Bit::Gray);
    setTextColor(Terminal::Color4Bit::White);
}

void Tg::Button::consumeKeyboardBuffer(const QByteArray &keyboardBuffer)
{
    if (keyboardBuffer.contains('\n') or keyboardBuffer.contains('\r')) {
        if (textColor() == Terminal::Color4Bit::Green) {
            setTextColor(Terminal::Color4Bit::Pink);
        } else {
            setTextColor(Terminal::Color4Bit::Green);
        }

        setBackgroundColor(Terminal::Color4Bit::White);

        emit clicked();
    }
}

void Tg::Button::onButtonPressTimeout()
{
    setBackgroundColor(Terminal::Color4Bit::Gray);
    setTextColor(Terminal::Color4Bit::White);
}
