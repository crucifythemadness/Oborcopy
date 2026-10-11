#pragma once

#include <QDialog>

namespace obor {

// "About Oborcopy": version, build date and build environment.
class AboutDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AboutDialog(QWidget *parent = nullptr);
};

} // namespace obor
