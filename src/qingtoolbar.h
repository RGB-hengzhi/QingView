#ifndef QINGTOOLBAR_H
#define QINGTOOLBAR_H

#include <QWidget>

class QAction;
class QLabel;
class QHBoxLayout;

// A child overlay: it never reserves space in the image viewport.
class QingToolbar : public QWidget
{
    Q_OBJECT
public:
    enum class Glyph { Open, Previous, Next, Minus, Plus, Fit, Actual, Rotate, Fullscreen };

    explicit QingToolbar(QWidget *parent = nullptr);
    ~QingToolbar() override;
    void addControl(QAction *action, const QString &key, const QString &caption, Glyph glyph);
    void setImageInfo(const QString &fileName, int index, int count, int zoomPercent);
    void setFullScreen(bool fullScreen);

signals:
    void actionRequested(QAction *action);
    void actionsAboutToBeDestroyed(const QList<QAction *> &actions);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void updateFilename();
    QHBoxLayout *controlLayout;
    QLabel *filenameLabel;
    QLabel *positionLabel;
    QLabel *zoomLabel;
    QString fullFilename;
};

#endif
