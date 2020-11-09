#pragma once

#include <widgets/tgscrollarea.h>

#include <QPointer>
#include <QModelIndex>
#include <QAbstractItemModel>

namespace Tg {
class ListView : public ScrollArea
{
    Q_OBJECT

    Q_PROPERTY(bool wrapRows READ wrapRows WRITE setWrapRows NOTIFY wrapRowsChanged)
    Q_PROPERTY(bool alternatingRowColors READ alternatingRowColors WRITE setAlternatingRowColors NOTIFY alternatingRowColorsChanged)
    Q_PROPERTY(Tg::Color alternativeBackgroundColor READ alternativeBackgroundColor WRITE setAlternativeBackgroundColor NOTIFY alternativeBackgroundColorChanged)
    Q_PROPERTY(QAbstractItemModel* model READ model WRITE setModel NOTIFY modelChanged)

public:
    ListView(Widget *parent);
    ListView(Screen *screen);

    QString drawAreaContents(const QPoint &pixel) const override;

    QAbstractItemModel *model() const;
    void setModel(QAbstractItemModel *model);

    bool wrapRows() const;
    bool alternatingRowColors() const;
    Tg::Color alternativeBackgroundColor() const;

public slots:
    void setWrapRows(const bool wrapRows);
    void setAlternatingRowColors(const bool alternatingRowColors);
    void setAlternativeBackgroundColor(const Tg::Color &alternativeBackgroundColor);

signals:
    void wrapRowsChanged(const bool wrapRows) const;
    void alternatingRowColorsChanged(const bool alternatingRowColors) const;
    void modelChanged(QAbstractItemModel* model) const;
    void alternativeBackgroundColorChanged(const Tg::Color &alternativeBackgroundColor) const;

protected:
    void init() override;
    void consumeKeyboardBuffer(const QString &keyboardBuffer) override;

    QString getLine(const int y) const;

protected slots:
    void updateChildrenDimensions() override;

private:
    QPointer<QAbstractItemModel> _model;
    QModelIndex _currentIndex;
    bool _wrapRows = false;
    bool _alternatingRowColors = false;
    Tg::Color _alternativeBackgroundColor = Tg::Color::Predefined::Empty;
};
}