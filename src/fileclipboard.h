#pragma once

#include <QObject>
#include <QSet>
#include <QStringList>

class QMimeData;

// Files on the desktop's clipboard (CLIP-01), in the formats other Linux
// file managers read and write, so cutting here and pasting there works
// both ways.
class FileClipboard : public QObject {
    Q_OBJECT

public:
    static FileClipboard *instance();

    struct Files {
        QStringList paths;
        bool cut = false;
        // Addresses that aren't local files, which can't be pasted yet.
        int remote = 0;
        bool operator==(const Files &o) const { return paths == o.paths && cut == o.cut; }
    };
    Files files() const;
    // The files in `mime`, which may have been dragged rather than copied.
    static Files read(const QMimeData *mime);
    void setFiles(const QStringList &paths, bool cut);
    void setText(const QString &text);
    void clear();

    // Whether `path` has been cut and not pasted yet, to show it dimmed.
    bool isCut(const QString &path) const { return m_cut.contains(path); }

    // After `pasted` cut files have been moved: takes the ones that moved
    // off the clipboard, unless something else is on it by now.
    void movedAway(const Files &pasted);

signals:
    // The set of cut files changed.
    void cutChanged();

private:
    explicit FileClipboard(QObject *parent);
    void update();

    QSet<QString> m_cut;
};
