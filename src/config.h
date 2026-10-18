#pragma once

#include <QStringList>

namespace Config {
QStringList parseFolders(const QByteArray &text);
QStringList folders();
}
