#include "qingtoolbar.h"

#include <QAction>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
class QingToolButton : public QToolButton
{
public:
    QingToolButton(QingToolbar::Glyph glyph, QWidget *parent) : QToolButton(parent), glyph(glyph)
    {
        setFixedSize(28, 32);
        setFocusPolicy(Qt::NoFocus);
        setAutoRaise(true);
        setMouseTracking(true);
        setCursor(Qt::PointingHandCursor);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        if (isEnabled() && (underMouse() || isDown()))
        {
            p.setPen(Qt::NoPen);
            p.setBrush(isDown() ? QColor(255, 255, 255, 40) : QColor(255, 255, 255, 23));
            p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 7, 7);
        }
        p.translate(5, 7);
        if (!isEnabled())
            p.setOpacity(0.28);
        p.setPen(QPen(QColor(236, 238, 242), 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);
        switch (glyph)
        {
        case QingToolbar::Glyph::Open: {
            QPainterPath path;
            path.moveTo(1, 5); path.lineTo(1, 15); path.lineTo(17, 15);
            path.lineTo(17, 5); path.lineTo(8, 5); path.lineTo(6, 3); path.lineTo(1, 3); path.closeSubpath();
            p.drawPath(path);
            break;
        }
        case QingToolbar::Glyph::Previous:
            p.drawLine(QPointF(11, 3), QPointF(5, 9));
            p.drawLine(QPointF(5, 9), QPointF(11, 15));
            break;
        case QingToolbar::Glyph::Next:
            p.drawLine(QPointF(7, 3), QPointF(13, 9));
            p.drawLine(QPointF(13, 9), QPointF(7, 15));
            break;
        case QingToolbar::Glyph::Minus:
        case QingToolbar::Glyph::Plus:
            p.drawEllipse(QRectF(1, 1, 12, 12));
            p.drawLine(QPointF(12, 12), QPointF(17, 17));
            p.drawLine(QPointF(4, 7), QPointF(10, 7));
            if (glyph == QingToolbar::Glyph::Plus)
                p.drawLine(QPointF(7, 4), QPointF(7, 10));
            break;
        case QingToolbar::Glyph::Fit:
            p.drawRoundedRect(QRectF(1, 2, 16, 14), 2, 2);
            p.drawLine(QPointF(5, 6), QPointF(13, 6));
            p.drawLine(QPointF(5, 12), QPointF(13, 12));
            break;
        case QingToolbar::Glyph::Actual: {
            QFont f = font(); f.setPixelSize(10); f.setBold(true); p.setFont(f);
            p.drawText(QRectF(-2, 0, 22, 18), Qt::AlignCenter, QStringLiteral("1:1"));
            break;
        }
        case QingToolbar::Glyph::Rotate:
            p.drawArc(QRectF(3, 3, 12, 12), 20 * 16, 290 * 16);
            p.drawLine(QPointF(14, 1), QPointF(16, 6));
            p.drawLine(QPointF(16, 6), QPointF(11, 6));
            break;
        case QingToolbar::Glyph::Fullscreen:
            p.drawLine(QPointF(1, 6), QPointF(1, 1)); p.drawLine(QPointF(1, 1), QPointF(6, 1));
            p.drawLine(QPointF(12, 1), QPointF(17, 1)); p.drawLine(QPointF(17, 1), QPointF(17, 6));
            p.drawLine(QPointF(17, 12), QPointF(17, 17)); p.drawLine(QPointF(17, 17), QPointF(12, 17));
            p.drawLine(QPointF(6, 17), QPointF(1, 17)); p.drawLine(QPointF(1, 17), QPointF(1, 12));
            break;
        }
    }

private:
    QingToolbar::Glyph glyph;
};
}

QingToolbar::QingToolbar(QWidget *parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("qingToolbar"));
    setAttribute(Qt::WA_TranslucentBackground);
    setFocusPolicy(Qt::NoFocus);
    setMouseTracking(true);
    setFixedHeight(76);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(13, 7, 13, 8);
    layout->setSpacing(3);
    controlLayout = new QHBoxLayout();
    controlLayout->setSpacing(3);
    layout->addLayout(controlLayout);

    auto *detailsLayout = new QHBoxLayout();
    detailsLayout->setSpacing(8);
    filenameLabel = new QLabel(this);
    filenameLabel->setObjectName(QStringLiteral("qingFilename"));
    filenameLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    positionLabel = new QLabel(this);
    positionLabel->setObjectName(QStringLiteral("qingPosition"));
    zoomLabel = new QLabel(this);
    zoomLabel->setObjectName(QStringLiteral("qingZoom"));
    zoomLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    for (auto *label : {filenameLabel, positionLabel, zoomLabel})
    {
        label->setStyleSheet(QStringLiteral("color: #c2c5cd; font-size: 11px; background: transparent;"));
        label->setAttribute(Qt::WA_TransparentForMouseEvents);
    }
    detailsLayout->addWidget(filenameLabel, 1);
    detailsLayout->addWidget(positionLabel);
    detailsLayout->addWidget(zoomLabel);
    layout->addLayout(detailsLayout);
}

QingToolbar::~QingToolbar()
{
    // Emitted before QWidget destroys the child QActions. A receiver context
    // lets Qt disconnect this cleanup if the application is already gone.
    emit actionsAboutToBeDestroyed(actions());
}

void QingToolbar::addControl(QAction *action, const QString &key, const QString &caption, Glyph glyph)
{
    if (!action)
        return;
    auto *button = new QingToolButton(glyph, this);
    button->setObjectName(QStringLiteral("qingToolbar_") + key);
    button->setDefaultAction(action);
    // Keyboard shortcuts live on MainWindow's existing virtual menu. These
    // clones are only clickable controls, so they cannot make a shortcut ambiguous.
    action->setShortcuts({});
    action->setShortcutContext(Qt::WidgetShortcut);
    button->setToolTip(caption);
    button->setAccessibleName(caption);
    connect(action, &QAction::changed, button, [button, caption]() {
        button->setToolTip(caption);
        button->setAccessibleName(caption);
    });
    connect(action, &QAction::triggered, this, [this, action]() { emit actionRequested(action); });
    controlLayout->addWidget(button, 0, Qt::AlignHCenter);
}

void QingToolbar::setImageInfo(const QString &fileName, int index, int count, int zoomPercent)
{
    fullFilename = fileName;
    filenameLabel->setToolTip(fileName);
    positionLabel->setText(count > 0 ? QStringLiteral("%1 / %2").arg(index + 1).arg(count) : QString());
    zoomLabel->setText(zoomPercent > 0 ? QStringLiteral("%1%").arg(zoomPercent) : QString());
    layout()->activate();
    updateFilename();
}

void QingToolbar::setFullScreen(bool fullScreen)
{
    if (auto *button = findChild<QToolButton *>(QStringLiteral("qingToolbar_fullscreen")))
    {
        const QString caption = fullScreen ? QStringLiteral("退出全屏") : QStringLiteral("全屏");
        button->setToolTip(caption);
        button->setAccessibleName(caption);
    }
}

void QingToolbar::updateFilename()
{
    filenameLabel->setText(filenameLabel->fontMetrics().elidedText(fullFilename, Qt::ElideMiddle, qMax(0, filenameLabel->width())));
}

void QingToolbar::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateFilename();
}

void QingToolbar::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(QColor(42, 44, 49, 243));
    p.setPen(QPen(QColor(255, 255, 255, 28), 1));
    p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 13, 13);
}
