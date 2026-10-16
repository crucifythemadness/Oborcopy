#pragma once

#include <QDialog>

class QFrame;

namespace obor {

// "About Developers": who built Oborcopy, with logos and links.
class DevelopersDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DevelopersDialog(QWidget *parent = nullptr);

private:
    QFrame *makeDeveloperCard(const QString &logo, const QString &name, const QStringList &links);
};

} // namespace obor
