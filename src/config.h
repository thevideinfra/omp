#pragma once

#include <QStringList>

namespace Config {
QStringList parseFolders(const QByteArray &text);
// The value of a key starts a TOML array (is a "[" after any space).
bool startsArray(const QString &value);
// Whether this text closes an array: a "]" outside quotes and before any comment.
bool closesArray(const QString &text);
QStringList folders();
}
