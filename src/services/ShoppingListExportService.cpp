#include "ShoppingListExportService.h"

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QSaveFile>
#include <QXmlStreamWriter>
#include <QtMath>

#include <utility>

namespace {
QString xmlEscape(QString text)
{
    text.replace(QLatin1Char('&'), QStringLiteral("&amp;"));
    text.replace(QLatin1Char('<'), QStringLiteral("&lt;"));
    text.replace(QLatin1Char('>'), QStringLiteral("&gt;"));
    text.replace(QLatin1Char('"'), QStringLiteral("&quot;"));
    text.replace(QLatin1Char('\''), QStringLiteral("&apos;"));
    return text;
}

QString quantity(double grams)
{
    if (grams >= 1000.0)
        return QStringLiteral("%1 kg").arg(grams / 1000.0, 0, 'f', grams >= 10000.0 ? 0 : 1);
    return QStringLiteral("%1 g").arg(qMax(1, qCeil(grams)));
}

quint32 crc32(const QByteArray &data)
{
    quint32 crc = 0xFFFFFFFFu;
    for (const char byte : data) {
        crc ^= static_cast<quint8>(byte);
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return crc ^ 0xFFFFFFFFu;
}

void append16(QByteArray &out, quint16 value)
{
    out.append(char(value & 0xFF));
    out.append(char((value >> 8) & 0xFF));
}

void append32(QByteArray &out, quint32 value)
{
    append16(out, quint16(value & 0xFFFF));
    append16(out, quint16((value >> 16) & 0xFFFF));
}

struct ZipEntry { QByteArray name; QByteArray data; quint32 crc = 0; quint32 offset = 0; };

bool writeStoredZip(const QString &path, QList<ZipEntry> entries, QString *error)
{
    QByteArray archive;
    for (ZipEntry &entry : entries) {
        entry.crc = crc32(entry.data);
        entry.offset = archive.size();
        append32(archive, 0x04034b50);
        append16(archive, 20); append16(archive, 0); append16(archive, 0);
        append16(archive, 0); append16(archive, 0);
        append32(archive, entry.crc);
        append32(archive, entry.data.size()); append32(archive, entry.data.size());
        append16(archive, entry.name.size()); append16(archive, 0);
        archive.append(entry.name); archive.append(entry.data);
    }
    const quint32 centralOffset = archive.size();
    for (const ZipEntry &entry : std::as_const(entries)) {
        append32(archive, 0x02014b50);
        append16(archive, 20); append16(archive, 20); append16(archive, 0); append16(archive, 0);
        append16(archive, 0); append16(archive, 0);
        append32(archive, entry.crc);
        append32(archive, entry.data.size()); append32(archive, entry.data.size());
        append16(archive, entry.name.size()); append16(archive, 0); append16(archive, 0);
        append16(archive, 0); append16(archive, 0); append32(archive, 0);
        append32(archive, entry.offset); archive.append(entry.name);
    }
    const quint32 centralSize = archive.size() - centralOffset;
    append32(archive, 0x06054b50);
    append16(archive, 0); append16(archive, 0);
    append16(archive, entries.size()); append16(archive, entries.size());
    append32(archive, centralSize); append32(archive, centralOffset); append16(archive, 0);

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(archive) != archive.size() || !file.commit()) {
        if (error) *error = QStringLiteral("无法写入文件：%1").arg(path);
        return false;
    }
    return true;
}

QByteArray docxDocument(const QList<ShoppingListItem> &items, const QString &scope)
{
    QStringList lines = {QStringLiteral("膳衡智能购物清单（%1）").arg(scope),
                         QStringLiteral("生成时间：%1").arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm")))};
    QString category;
    for (const ShoppingListItem &item : items) {
        if (category != item.category) {
            category = item.category;
            lines << QString() << QStringLiteral("【%1】").arg(category);
        }
        lines << QStringLiteral("□ %1  %2").arg(item.name, quantity(item.buyGrams));
    }
    if (items.isEmpty()) lines << QStringLiteral("当前冰箱库存已覆盖该方案，无需额外购买。");
    lines << QString() << QStringLiteral("注：常备盐、味精、鸡精、普通酱油等未列入；八角、桂皮等香料会保留。");
    QString body;
    for (const QString &line : std::as_const(lines))
        body += QStringLiteral("<w:p><w:r><w:t xml:space=\"preserve\">%1</w:t></w:r></w:p>").arg(xmlEscape(line));
    return QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?><w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\"><w:body>%1<w:sectPr><w:pgSz w:w=\"11906\" w:h=\"16838\"/></w:sectPr></w:body></w:document>").arg(body).toUtf8();
}

QByteArray xlsxSheet(const QList<ShoppingListItem> &items)
{
    QList<QStringList> rows = {{QStringLiteral("购买"), QStringLiteral("分类"), QStringLiteral("食材"),
                                QStringLiteral("方案需要"), QStringLiteral("冰箱已有"), QStringLiteral("实际购买")}};
    for (const ShoppingListItem &item : items)
        rows.append({QStringLiteral("□"), item.category, item.name, quantity(item.plannedGrams),
                     quantity(item.fridgeGrams), quantity(item.buyGrams)});
    QString xml = QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?><worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\"><sheetData>");
    for (int row = 0; row < rows.size(); ++row) {
        xml += QStringLiteral("<row r=\"%1\">").arg(row + 1);
        for (int col = 0; col < rows.at(row).size(); ++col) {
            const QString ref = QString(QChar('A' + col)) + QString::number(row + 1);
            xml += QStringLiteral("<c r=\"%1\" t=\"inlineStr\"><is><t xml:space=\"preserve\">%2</t></is></c>")
                       .arg(ref, xmlEscape(rows.at(row).at(col)));
        }
        xml += QStringLiteral("</row>");
    }
    xml += QStringLiteral("</sheetData></worksheet>");
    return xml.toUtf8();
}

bool exportPdf(const QString &path, const QList<ShoppingListItem> &items,
               const QString &scope, QString *error)
{
    QPdfWriter writer(path);
    writer.setResolution(96);
    writer.setPageSize(QPageSize(QPageSize::A4));
    writer.setPageMargins(QMarginsF(16, 16, 16, 16), QPageLayout::Millimeter);
    writer.setTitle(QStringLiteral("膳衡智能购物清单"));
    QPainter painter(&writer);
    if (!painter.isActive()) {
        if (error) *error = QStringLiteral("无法创建 PDF 文件。");
        return false;
    }
    const QStringList candidates = {QStringLiteral("Noto Sans SC"), QStringLiteral("Microsoft YaHei UI")};
    const QStringList families = QFontDatabase::families();
    QString family;
    for (const QString &candidate : candidates) if (families.contains(candidate)) { family = candidate; break; }
    QFont font(family); font.setPixelSize(14); painter.setFont(font);
    const QRect page = writer.pageLayout().paintRectPixels(writer.resolution());
    int y = page.top();
    auto drawLine = [&](const QString &line, bool heading = false) {
        if (y + 28 > page.bottom()) { writer.newPage(); y = page.top(); }
        QFont current = font; current.setPixelSize(heading ? 20 : 14); current.setBold(heading);
        painter.setFont(current); painter.setPen(QColor(QStringLiteral("#14213D")));
        painter.drawText(QRect(page.left(), y, page.width(), heading ? 34 : 25), Qt::AlignLeft | Qt::AlignVCenter, line);
        y += heading ? 40 : 27;
    };
    drawLine(QStringLiteral("膳衡智能购物清单（%1）").arg(scope), true);
    drawLine(QStringLiteral("生成时间：%1").arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm"))));
    QString category;
    for (const ShoppingListItem &item : items) {
        if (category != item.category) { category = item.category; y += 8; drawLine(QStringLiteral("【%1】").arg(category), true); }
        drawLine(QStringLiteral("□  %1    %2").arg(item.name, quantity(item.buyGrams)));
    }
    if (items.isEmpty()) drawLine(QStringLiteral("当前冰箱库存已覆盖该方案，无需额外购买。"));
    y += 10;
    drawLine(QStringLiteral("注：常备盐、味精、鸡精、普通酱油等未列入；八角、桂皮等香料会保留。"));
    painter.end();
    return QFile::exists(path) && QFileInfo(path).size() > 0;
}
} // namespace

QString ShoppingListExportService::extension(Format format)
{
    switch (format) {
    case Format::Pdf: return QStringLiteral("pdf");
    case Format::Word: return QStringLiteral("docx");
    case Format::Excel: return QStringLiteral("xlsx");
    case Format::Text: return QStringLiteral("txt");
    }
    return QStringLiteral("txt");
}

bool ShoppingListExportService::exportList(const QString &path, Format format,
                                           const QList<ShoppingListItem> &items,
                                           const QString &scopeLabel, QString *error)
{
    if (format == Format::Text) {
        QSaveFile file(path);
        const QByteArray data = ShoppingListService::toShareText(items, scopeLabel).toUtf8();
        if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
            if (error) *error = QStringLiteral("无法写入文本文件。");
            return false;
        }
        return true;
    }
    if (format == Format::Pdf)
        return exportPdf(path, items, scopeLabel, error);
    if (format == Format::Word) {
        return writeStoredZip(path, {
            {"[Content_Types].xml", "<?xml version=\"1.0\" encoding=\"UTF-8\"?><Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\"><Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/><Default Extension=\"xml\" ContentType=\"application/xml\"/><Override PartName=\"/word/document.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml\"/></Types>"},
            {"_rels/.rels", "<?xml version=\"1.0\" encoding=\"UTF-8\"?><Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\"><Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"word/document.xml\"/></Relationships>"},
            {"word/document.xml", docxDocument(items, scopeLabel)}}, error);
    }
    return writeStoredZip(path, {
        {"[Content_Types].xml", "<?xml version=\"1.0\" encoding=\"UTF-8\"?><Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\"><Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/><Default Extension=\"xml\" ContentType=\"application/xml\"/><Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/><Override PartName=\"/xl/worksheets/sheet1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/></Types>"},
        {"_rels/.rels", "<?xml version=\"1.0\" encoding=\"UTF-8\"?><Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\"><Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"xl/workbook.xml\"/></Relationships>"},
        {"xl/workbook.xml", "<?xml version=\"1.0\" encoding=\"UTF-8\"?><workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\"><sheets><sheet name=\"购物清单\" sheetId=\"1\" r:id=\"rId1\"/></sheets></workbook>"},
        {"xl/_rels/workbook.xml.rels", "<?xml version=\"1.0\" encoding=\"UTF-8\"?><Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\"><Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet1.xml\"/></Relationships>"},
        {"xl/worksheets/sheet1.xml", xlsxSheet(items)}}, error);
}
