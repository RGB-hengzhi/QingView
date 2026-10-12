#include "qvrenamedialog.h"

#include <QDir>
#include <QMessageBox>

static void showRenameError(QWidget *parent, const QString &title, const QString &message)
{
    // The input dialog has already finished and may be deleted by a deferred
    // event. A separate heap dialog avoids nesting a modal loop inside it.
    auto *box = new QMessageBox(QMessageBox::Critical, title, message, QMessageBox::Ok, parent);
    box->setAttribute(Qt::WA_DeleteOnClose);
    box->open();
}

QVRenameDialog::QVRenameDialog(QWidget *parent, QFileInfo fileInfo) :
    QInputDialog(parent)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);

    this->fileInfo = fileInfo;

    setWindowTitle(tr("Rename..."));
    setLabelText(tr("File name:"));
    setTextValue(fileInfo.fileName());
    resize(350, height());

    connect(this, &QInputDialog::finished, this, &QVRenameDialog::onFinished);
}

void QVRenameDialog::onFinished(int result)
{
    if (!result)
        return;

    if (!fileInfo.isWritable())
    {
        showRenameError(parentWidget(), tr("Error"), tr("Could not rename %1:\nNo write permission or file is read-only.").arg(fileInfo.fileName()));
        return;
    }

    if (result)
    {
        const auto newFileName = textValue();
        const auto newFilePath = QDir::cleanPath(fileInfo.absolutePath() + QDir::separator() + newFileName);

        if (fileInfo.absoluteFilePath() != newFilePath)
        {
            emit readyToRenameFile();
            if (QFile::rename(fileInfo.absoluteFilePath(), newFilePath))
            {
                emit newFileToOpen(newFilePath);
            }
            else
            {
                emit newFileToOpen(fileInfo.absoluteFilePath());
                showRenameError(parentWidget(), tr("Error"), tr("Could not rename %1:\n(Check that all characters are valid)").arg(fileInfo.fileName()));
            }
        }
    }
}

void QVRenameDialog::showEvent(QShowEvent *event)
{
    QInputDialog::showEvent(event);

    QLineEdit *lineEdit = findChild<QLineEdit*>();
    const auto &lastDot = lineEdit->text().lastIndexOf(".");
    if (lastDot != -1)
        lineEdit->setSelection(0, lastDot);
}
