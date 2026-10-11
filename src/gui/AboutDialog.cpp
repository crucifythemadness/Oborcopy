#include "AboutDialog.h"

#include "AppInfo.h"
#include "GuiUtil.h"
#include "IconFactory.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace obor {

AboutDialog::AboutDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("About Oborcopy"));

    auto *logo = new QLabel(this);
    logo->setPixmap(IconFactory::appIcon().pixmap(96, 96));
    logo->setAlignment(Qt::AlignTop);

    auto *name = GuiUtil::makeLabel(AppInfo::name(), QStringLiteral("AppTitle"), this);
    auto *tagline = GuiUtil::makeLabel(tr("Reliable incremental copy, backup and mirror tool"),
                                       QStringLiteral("Muted"), this);

    auto *facts = new QFormLayout;
    facts->setHorizontalSpacing(18);
    facts->addRow(tr("Version:"), new QLabel(QStringLiteral("<b>%1</b>").arg(AppInfo::version())));
    facts->addRow(tr("Build date:"), new QLabel(AppInfo::buildDate()));
    facts->addRow(tr("Qt version:"), new QLabel(tr("%1 (built with %2)").arg(AppInfo::qtVersion(),
                                                                            QStringLiteral(QT_VERSION_STR))));
    facts->addRow(tr("Compiler:"), new QLabel(AppInfo::compiler()));
    facts->addRow(tr("System:"), new QLabel(AppInfo::platform()));

    auto *text = new QVBoxLayout;
    text->addWidget(name);
    text->addWidget(tagline);
    text->addSpacing(10);
    text->addLayout(facts);
    text->addSpacing(10);
    text->addWidget(GuiUtil::makeLabel(tr("Inspired by Robocopy and rsync. Built with C++ and Qt."),
                                       QStringLiteral("Muted"), this));

    auto *top = new QHBoxLayout;
    top->setSpacing(20);
    top->addWidget(logo);
    top->addLayout(text, 1);

    auto *close = new QPushButton(tr("Close"), this);
    close->setObjectName(QStringLiteral("Primary"));
    connect(close, &QPushButton::clicked, this, &QDialog::accept);
    auto *buttons = new QHBoxLayout;
    buttons->addStretch();
    buttons->addWidget(close);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 22, 24, 18);
    layout->addLayout(top);
    layout->addSpacing(12);
    layout->addLayout(buttons);
    setFixedSize(sizeHint());
}

} // namespace obor
