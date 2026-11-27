#include "preview.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QMimeDatabase>
#include <QProcess>
#include <QStandardPaths>
#include <QStringDecoder>

#include <mutex>

namespace {

std::mutex rendererMutex;
QString rendererOverride;
bool rendererSet = false;

QString tr(const char *text) {
    return QCoreApplication::translate("Preview", text);
}

QByteArray readStart(const QString &path, qint64 bytes, QString *error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        *error = file.errorString();
        return {};
    }
    return file.read(bytes);
}

bool isTextMime(const QMimeType &mime) {
    return mime.inherits("text/plain") || mime.name().startsWith("text/");
}

void readFolder(const QString &path, const Preview::Limits &limits, Preview *p) {
    QDir dir(path);
    if (!dir.isReadable()) {
        p->kind = Preview::Kind::Error;
        p->error = tr("You don't have permission to look in this folder.");
        return;
    }
    p->kind = Preview::Kind::Folder;
    p->items = 0;
    QDirIterator it(path, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
    while (it.hasNext()) {
        it.next();
        if (p->items >= limits.folderItems) {
            p->moreItems = true;
            break;
        }
        ++p->items;
        if (it.fileName().startsWith('.'))
            ++p->hiddenItems;
    }
}

bool readImage(const QString &path, const Preview::Limits &limits, Preview *p) {
    QImageReader reader(path);
    reader.setAutoTransform(true);
    if (!reader.canRead())
        return false;
    p->imageSize = reader.size();
    if (reader.supportsAnimation() && reader.imageCount() != 1 && p->size <= limits.animationBytes) {
        QString error;
        p->data = readStart(path, limits.animationBytes, &error);
        if (!p->data.isEmpty()) {
            p->kind = Preview::Kind::Animation;
            return true;
        }
    }
    const QSize size = reader.size();
    if (size.isValid() && (size.width() > limits.imageSide || size.height() > limits.imageSide))
        reader.setScaledSize(size.scaled(limits.imageSide, limits.imageSide, Qt::KeepAspectRatio));
    p->image = reader.read();
    if (p->image.isNull()) {
        p->kind = Preview::Kind::Error;
        p->error = tr("This image can't be read: %1").arg(reader.errorString());
        return true;
    }
    if (!p->imageSize.isValid())
        p->imageSize = p->image.size();
    p->kind = Preview::Kind::Image;
    return true;
}

void readPdf(const QString &path, const Preview::Limits &limits, Preview *p) {
    p->kind = Preview::Kind::Info;
    const QString program = Preview::pdfRenderer();
    if (program.isEmpty()) {
        p->note = tr("Install poppler to see the first page of PDFs here.");
        return;
    }
    QProcess process;
    process.setProgram(program);
    // With no file to write to, pdftoppm sends the page to its output.
    process.setArguments({"-png", "-singlefile", "-f", "1", "-l", "1", "-scale-to", QString::number(limits.pdfSide),
                          "--", path});
    process.start();
    if (!process.waitForFinished(15000)) {
        process.kill();
        process.waitForFinished(1000);
        p->note = tr("The first page took too long to draw.");
        return;
    }
    const QImage page = QImage::fromData(process.readAllStandardOutput(), "PNG");
    if (process.exitCode() != 0 || page.isNull()) {
        p->note = tr("The first page couldn't be drawn.");
        return;
    }
    p->image = page;
    p->kind = Preview::Kind::Pdf;
}

} // namespace

Preview Preview::load(const QString &path, const Limits &limits) {
    Preview p;
    p.path = path;
    if (path.isEmpty())
        return p;
    const QFileInfo info(path);
    p.name = info.fileName().isEmpty() ? path : info.fileName();
    if (!info.exists() && !info.isSymLink()) {
        p.kind = Kind::Error;
        p.error = tr("%1 is no longer there.").arg(p.name);
        return p;
    }
    p.modified = info.lastModified();
    QMimeDatabase db;
    if (info.isDir()) {
        const QMimeType mime = db.mimeTypeForName("inode/directory");
        p.mimeName = mime.name();
        p.mimeComment = mime.comment();
        p.iconName = mime.iconName();
        readFolder(path, limits, &p);
        return p;
    }
    if (!info.exists()) {
        p.kind = Kind::Error;
        p.error = tr("%1 is a link to something that isn't there.").arg(p.name);
        return p;
    }
    p.size = info.size();
    const QMimeType mime = db.mimeTypeForFile(info);
    p.mimeName = mime.name();
    p.mimeComment = mime.comment();
    p.iconName = mime.iconName();
    if (!info.isFile()) {
        // Devices, pipes and sockets: reading them could wait for ever.
        p.kind = Kind::Info;
        return p;
    }
    if (!info.isReadable()) {
        p.kind = Kind::Error;
        p.error = tr("You don't have permission to read %1.").arg(p.name);
        return p;
    }
    if (p.size == 0) {
        p.kind = Kind::Info;
        p.note = tr("The file is empty.");
        return p;
    }
    if (mime.name().startsWith("image/") && readImage(path, limits, &p))
        return p;
    if (mime.inherits("application/pdf")) {
        readPdf(path, limits, &p);
        return p;
    }
    if (mime.name().startsWith("audio/") || mime.name().startsWith("video/")) {
        p.kind = Kind::Media;
        p.video = mime.name().startsWith("video/");
        return p;
    }

    QString error;
    const bool textual = isTextMime(mime);
    QByteArray bytes = readStart(path, textual ? limits.textBytes : limits.hexBytes, &error);
    if (bytes.isEmpty() && !error.isEmpty()) {
        p.kind = Kind::Error;
        p.error = tr("%1 can't be read: %2").arg(p.name, error);
        return p;
    }
    bool isText = false;
    // Files with no type of their own, such as most settings files, are
    // shown as text when that's what they hold.
    if (textual || mime.isDefault()) {
        if (!textual && p.size > bytes.size())
            bytes += readStart(path, limits.textBytes, &error).mid(bytes.size());
        p.text = decodeText(bytes, &isText);
    }
    if (isText) {
        p.kind = Kind::Text;
        p.data = bytes.left(limits.hexBytes);
        p.truncated = p.size > bytes.size();
        return p;
    }
    p.kind = Kind::Hex;
    p.text.clear();
    p.data = bytes.left(limits.hexBytes);
    p.truncated = p.size > p.data.size();
    return p;
}

QString Preview::decodeText(const QByteArray &bytes, bool *isText) {
    *isText = !bytes.contains('\0');
    if (!*isText)
        return {};
    // Stateful, so a character cut in two at the end of what was read is
    // held back rather than counted as an error.
    QStringDecoder utf8(QStringDecoder::Utf8);
    QString text = utf8.decode(bytes);
    if (utf8.hasError())
        text = QString::fromLatin1(bytes);
    return text;
}

namespace {
int offsetWidth(qint64 total) {
    total = qMax<qint64>(0, total);
    int width = 8;
    while (width < 16 && (total >> (4 * width)) != 0)
        ++width;
    return width;
}
} // namespace

int Preview::hexRowWidth(qint64 total, int perRow) {
    // Offset, two spaces, three per byte and one between the halves, then
    // the characters between bars.
    return offsetWidth(total) + 2 + 3 * perRow + (perRow > 8 ? 1 : 0) + 1 + perRow + 2;
}

QString Preview::hexDump(const QByteArray &data, qint64 total, int perRow) {
    const int width = offsetWidth(qMax<qint64>(total, data.size()));
    QString out;
    out.reserve(int(data.size() / perRow + 1) * (hexRowWidth(total, perRow) + 1));
    for (qsizetype row = 0; row < data.size(); row += perRow) {
        out += QString::number(row, 16).rightJustified(width, '0');
        out += "  ";
        QString chars;
        for (int i = 0; i < perRow; ++i) {
            if (i == 8)
                out += ' ';
            if (row + i < data.size()) {
                const uchar c = uchar(data.at(row + i));
                out += QString::number(c, 16).rightJustified(2, '0');
                out += ' ';
                chars += (c >= 0x20 && c < 0x7f) ? QChar(c) : QChar('.');
            } else {
                out += "   ";
            }
        }
        out += " |" + chars + "|\n";
    }
    return out;
}

QString Preview::pdfRenderer() {
    std::lock_guard<std::mutex> lock(rendererMutex);
    if (rendererSet)
        return rendererOverride;
    return QStandardPaths::findExecutable("pdftoppm");
}

void Preview::setPdfRenderer(const QString &program) {
    std::lock_guard<std::mutex> lock(rendererMutex);
    rendererOverride = program;
    rendererSet = true;
}
