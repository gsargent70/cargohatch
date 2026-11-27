#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QImage>
#include <QSize>
#include <QString>

// What the viewer pane shows for one file or folder (VWR-01). Loading reads
// the disk, perhaps a slow network mount, so it runs on a worker thread and
// touches nothing that belongs to the GUI.
struct Preview {
    enum class Kind {
        None,      // nothing to show
        Error,     // `error` says why
        Folder,    // `items` and `hiddenItems`
        Image,     // `image`, perhaps scaled down from `imageSize`
        Animation, // `data` holds the whole file, for QMovie
        Text,      // `text`, the start of the file when `truncated`
        Hex,       // `data` holds the start of the file
        Pdf,       // `image` is the first page
        Media,     // played by Qt Multimedia; `video` or audio
        Info,      // only the details: an empty file, a PDF with no pdftoppm
    };

    // How much is read, at most.
    struct Limits {
        qint64 textBytes = 512 * 1024;
        qint64 hexBytes = 64 * 1024;
        qint64 animationBytes = 64 * 1024 * 1024;
        // Larger images are scaled down as they're read.
        int imageSide = 4096;
        int folderItems = 10000;
        int pdfSide = 1600;
    };

    QString path;
    Kind kind = Kind::None;
    QString error;
    // A line for under the content, such as why only details show.
    QString note;

    // Details, for every kind but None and Error.
    QString name;
    QString mimeName;
    QString mimeComment;
    QString iconName;
    qint64 size = -1;
    QDateTime modified;

    QImage image;
    QSize imageSize;
    QByteArray data;
    QString text;
    bool truncated = false;
    bool video = false;
    int items = -1;
    int hiddenItems = 0;
    bool moreItems = false;

    // Reads `path`; safe to call on any thread.
    static Preview load(const QString &path, const Limits &limits);
    static Preview load(const QString &path) { return load(path, Limits()); }

    // `data` as lines of offset, hex bytes and printable characters. `total`
    // is the file's size, for the offsets' width; `perRow` is 16 or 8.
    static QString hexDump(const QByteArray &data, qint64 total = -1, int perRow = 16);
    // How many characters wide a row of hexDump() is.
    static int hexRowWidth(qint64 total, int perRow);
    // Text, as read: valid UTF-8, or Latin-1 when it isn't; empty, with
    // `isText` false, when the bytes hold a NUL and so aren't text.
    static QString decodeText(const QByteArray &bytes, bool *isText);

    // The program that renders PDF pages; tests point it elsewhere.
    static QString pdfRenderer();
    static void setPdfRenderer(const QString &program);
};
