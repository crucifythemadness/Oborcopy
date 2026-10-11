#include "Hash.h"

#include "SyncControl.h"

#include <QCryptographicHash>
#include <QFile>

namespace obor::Hash {

QByteArray fileSha256(const QString &path, SyncControl *control, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = file.errorString();
        return {};
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    QByteArray buffer(1 << 20, Qt::Uninitialized);
    while (true) {
        if (control && !control->checkpoint())
            return {};
        const qint64 n = file.read(buffer.data(), buffer.size());
        if (n < 0) {
            if (error)
                *error = file.errorString();
            return {};
        }
        if (n == 0)
            break;
        hash.addData(QByteArrayView(buffer.constData(), n));
    }
    return hash.result();
}

} // namespace obor::Hash
