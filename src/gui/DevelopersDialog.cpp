#include "DevelopersDialog.h"

#include "GuiUtil.h"
#include "IconFactory.h"
#include "Theme.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>

namespace obor {

namespace {

constexpr int kLogoSize = 88;
constexpr int kDialogWidth = 560;

QPixmap logoPixmap(const QString &resource, qreal devicePixelRatio)
{
    const int size = qRound(kLogoSize * devicePixelRatio);
    QPixmap pixmap = QPixmap(resource).scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    pixmap.setDevicePixelRatio(devicePixelRatio);
    return pixmap;
}

} // namespace

DevelopersDialog::DevelopersDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("About Developers"));

    auto *close = new QPushButton(tr("Close"), this);
    close->setObjectName(QStringLiteral("Primary"));
    connect(close, &QPushButton::clicked, this, &QDialog::accept);
    auto *buttons = new QHBoxLayout;
    buttons->addStretch();
    buttons->addWidget(close);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 22, 24, 18);
    layout->setSpacing(12);
    layout->addWidget(GuiUtil::makeLabel(tr("About the developers"), QStringLiteral("Title"), this));
    layout->addWidget(GuiUtil::makeLabel(tr("The people and tools behind Oborcopy"), QStringLiteral("Muted"), this));
    layout->addSpacing(4);
    layout->addWidget(makeDeveloperCard(QStringLiteral(":/icons/Claude_Logo.png"), QStringLiteral("Claude"),
                                        {QStringLiteral("https://claude.ai/")}));
    layout->addWidget(makeDeveloperCard(QStringLiteral(":/icons/CrucifyTheMadness_Logo.png"),
                                        QStringLiteral("CrucifyTheMadness"),
                                        {QStringLiteral("https://github.com/crucifythemadness"),
                                         QStringLiteral("https://m.twitch.tv/crucifythemadness/home"),
                                         QStringLiteral("https://bandcamp.com/crucifythemadness")}));
    layout->addSpacing(6);
    layout->addLayout(buttons);

    setFixedSize(QSize(kDialogWidth, sizeHint().height()));
}

QFrame *DevelopersDialog::makeDeveloperCard(const QString &logo, const QString &name, const QStringList &links)
{
    auto *card = GuiUtil::makeCard(this);
    auto *picture = new QLabel(card);
    picture->setPixmap(logoPixmap(logo, devicePixelRatioF()));
    picture->setFixedSize(kLogoSize, kLogoSize);

    auto *text = new QVBoxLayout;
    text->setSpacing(6);
    text->addStretch();
    text->addWidget(GuiUtil::makeLabel(name, QStringLiteral("SectionTitle"), card));
    const QColor accent = Theme::colors().accent;
    for (const QString &url : links) {
        auto *icon = new QLabel(card);
        icon->setPixmap(IconFactory::icon(IconKind::Arrow, accent).pixmap(14, 14));
        auto *link = new QLabel(QStringLiteral("<a href=\"%1\" style=\"color:%2; text-decoration:none;\">%1</a>")
                                    .arg(url.toHtmlEscaped(), accent.name()),
                                card);
        link->setTextFormat(Qt::RichText);
        link->setTextInteractionFlags(Qt::TextBrowserInteraction);
        link->setOpenExternalLinks(true);
        link->setCursor(Qt::PointingHandCursor);
        link->setToolTip(tr("Open %1 in the browser").arg(url));
        auto *row = new QHBoxLayout;
        row->setSpacing(8);
        row->addWidget(icon);
        row->addWidget(link, 1);
        text->addLayout(row);
    }
    text->addStretch();

    auto *layout = new QHBoxLayout(card);
    layout->setContentsMargins(18, 14, 18, 14);
    layout->setSpacing(22);
    layout->addWidget(picture, 0, Qt::AlignVCenter);
    layout->addLayout(text, 1);
    return card;
}

} // namespace obor
