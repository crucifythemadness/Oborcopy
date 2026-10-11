#pragma once

#include <QWidget>

class QLabel;
class QLineEdit;
class QTimer;

namespace obor {

// Folder input with browse button, drag & drop and a drive info line
// ("exFAT · 412 GB free of 1.8 TB").
class PathPicker : public QWidget
{
    Q_OBJECT

public:
    PathPicker(const QString &caption, QWidget *parent = nullptr);

    QString path() const;
    void setPath(const QString &path);
    void setPlaceholder(const QString &text);

signals:
    void pathChanged(const QString &path);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void browse();
    void updateInfo();

    QString m_caption;
    QLineEdit *m_edit;
    QLabel *m_info;
    QTimer *m_infoTimer;
};

} // namespace obor
