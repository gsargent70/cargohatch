#include "namematch.h"

#include "renamedelegate.h"

NameMatch::Stripped NameMatch::strip(const QString &name, bool withoutAccents) {
    Stripped s;
    bool plain = true;
    for (const QChar c : name)
        plain &= c.unicode() < 0x80;
    if (!withoutAccents || plain) {
        s.text = name;
        return s;
    }
    for (int i = 0; i < name.size();) {
        char32_t c = name.at(i).unicode();
        int length = 1;
        if (name.at(i).isHighSurrogate() && i + 1 < name.size() && name.at(i + 1).isLowSurrogate()) {
            c = QChar::surrogateToUcs4(name.at(i), name.at(i + 1));
            length = 2;
        }
        // An accented letter comes apart into the letter and its accents;
        // the accents are left out, here and where a name spells them as
        // separate characters already.
        QString parts = QChar::decompositionTag(c) == QChar::Canonical ? QChar::decomposition(c) : QString();
        if (parts.isEmpty())
            parts = name.mid(i, length);
        for (const QChar part : std::as_const(parts)) {
            if (part.category() == QChar::Mark_NonSpacing)
                continue;
            s.text += part;
            s.at << i;
        }
        i += length;
    }
    s.at << int(name.size());
    return s;
}

int NameMatch::lengthWithoutExtension(const QString &name, bool isDir) {
    const int dot = isDir ? -1 : RenameDelegate::extensionStart(name);
    return dot < 0 ? int(name.size()) : dot;
}
