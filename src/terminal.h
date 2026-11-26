#pragma once

#include <QString>

#include <functional>

class QObject;

// Opens the user's terminal in a folder (IDEA-10). xdg-terminal-exec picks the
// terminal, as the rest of the desktop does; asked first for the command it
// would run, it can say what is wrong instead of failing out of sight.
namespace Terminal {

// What is asked for the command; for tests.
void setProgram(const QString &program);
// Opens a terminal in `folder`. If it can't, `failed` is told why, in a
// sentence for the user.
void open(const QString &folder, QObject *context, std::function<void(const QString &)> failed);

} // namespace Terminal
