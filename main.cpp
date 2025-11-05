#include <QApplication>
#include <QMainWindow>
#include <QPainter>
#include <QRect>
#include <QHBoxLayout>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QPrinter>
#include <QPrintDialog>
#include <QPdfDocument>
#include <QPdfView>
#include <QPageSize>
#include <QBuffer>
#include <QPdfWriter>
#include <QTimer>
#include <QSpinBox>
#include <QLabel>
#include <QPdfPageNavigation>
#include <QPlainTextDocumentLayout>
#include <qmath.h>
#include <QSplitter>
#include <QDebug>

#define debug_ false

#define QD qDebug()
#define DUMP(var)  #var << ": " << (var) << " "
#define DUMPH(var) #var << ": " << QString::number(var, 16) << " "
#define DUMPS(var) #var << ": " << QString::fromLatin1(var, 4) << " "

static double p2mm(const qreal x) {
    return x * 25.4 / 300.0;
}

static double mm2p(const qreal x) {
    return x * 300.0 / 25.4;
}

namespace Format {
    enum {
        AlignTop = 1,
        AlignVCenter = 2,
        AlignBottom = 3,
        AlignLeft = 4,
        AlignHCenter = 8,
        AlignRight = 12,
        Italic = 16,
        Bold = 32,
        Picture = 64,
        VUse = 128,
        Small = 256
    };
}

struct Pdf final {
    QIODevice *device;
    QPdfWriter *writer{};
    QPainter painter;
    int pageNumber = 1;
    qreal posY = 0;
    qreal pageHeight = 0;

    explicit Pdf(QIODevice *device): device(device) {
        if (device->isOpen()) {
            device->close();
        }
        if (!device->open(QIODevice::WriteOnly)) {
            return;
        }
        writer = new QPdfWriter(device);
        writer->setResolution(300);
    }

    void begin() {
        if (!writer) return;
        if (!painter.begin(writer)) {
            device->close();
        }
        const QRectF pageRect = writer->pageLayout().paintRectPixels(writer->resolution());
        pageHeight = pageRect.height();
    }

    void end() {
        if (pageNumber > 1) {
            drawPageNumber();
        }
        painter.end();
        device->close();
    }

    ~Pdf() {
        end();
        delete writer;
    }

    // размеры в миллиметрах
    void setPageSize(const int w, const int h) const {
        if (!writer) return;
        writer->setPageSize(QPageSize(QSize(w, h), QPageSize::Millimeter, "Report"));
    }

    // размеры в миллиметрах
    void setMargins(const int l, const int t, const int r, const int b) const {
        if (!writer) return;
        auto pdfWriterLayout = writer->pageLayout();
        pdfWriterLayout.setUnits(QPageLayout::Millimeter);
        pdfWriterLayout.setMargins(QMarginsF(l, t, r, b));
        writer->setPageLayout(pdfWriterLayout);
    }

    qreal textHeightManualWithNewlines(const QString &text, const qreal width,
                                       const qreal lineSpacing = 1.5) const {
        const QFontMetricsF fm(painter.font());
        const qreal lineHeight = fm.height() * lineSpacing;
        qreal totalHeight = 0;

        // Разбиваем текст на параграфы по символам переноса строки
        QStringList paragraphs = text.split('\n');

        for (const QString &paragraph: paragraphs) {
            if (paragraph.isEmpty()) {
                // Пустая строка - все равно учитываем высоту
                totalHeight += lineHeight;
                continue;
            }

            QStringList words = paragraph.split(' ');
            QStringList lines;
            QString currentLine;

            // Формируем строки с переносом по словам для каждого параграфа
            for (const QString &word: words) {
                if (word.isEmpty()) continue;

                QString testLine = currentLine.isEmpty() ? word : currentLine + " " + word;
                if (fm.horizontalAdvance(testLine) <= width || currentLine.isEmpty()) {
                    currentLine = testLine;
                } else {
                    lines.append(currentLine);
                    currentLine = word;
                }
            }

            if (!currentLine.isEmpty()) {
                lines.append(currentLine);
            }

            totalHeight += lines.count() * lineHeight;
        }

        return totalHeight;
    }

    qreal calculateTextHeightAdvanced(const QString &text, const qreal width,
                                      const qreal lineSpacing = 1.5) const {
        const QFontMetricsF fm(painter.font());
        const qreal lineHeight = fm.height() * lineSpacing;
        qreal totalHeight = 0;

        QStringList paragraphs = text.split('\n');

        for (const QString &paragraph: paragraphs) {
            // Учитываем даже полностью пустые параграфы
            if (paragraph.trimmed().isEmpty()) {
                totalHeight += lineHeight;
                continue;
            }

            QStringList words = paragraph.split(' ', Qt::SkipEmptyParts);
            if (words.isEmpty()) {
                totalHeight += lineHeight;
                continue;
            }

            QStringList lines;
            QString currentLine = words.first();

            for (int i = 1; i < words.size(); ++i) {
                const QString &word = words[i];
                QString testLine = currentLine + " " + word;

                if (fm.horizontalAdvance(testLine) <= width) {
                    currentLine = testLine;
                } else {
                    lines.append(currentLine);
                    currentLine = word;
                }
            }

            lines.append(currentLine);
            totalHeight += lines.count() * lineHeight;
        }

        return totalHeight;
    }

    void drawTextWithWordWrap(const QRectF &rect, const int alignmentFlags,
                              const QString &text) {
        QTextDocument textDoc;

        // Настройка документа
        textDoc.setDefaultFont(painter.font());
        textDoc.setTextWidth(rect.width()); // Ключевой параметр для переноса

        QTextCursor cursor(&textDoc);
        QTextBlockFormat blockFormat;
        blockFormat.setLineHeight(150, QTextBlockFormat::ProportionalHeight);
        blockFormat.setAlignment(
            static_cast<Qt::Alignment>(alignmentFlags) | Qt::AlignVCenter);
        cursor.setBlockFormat(blockFormat);
        cursor.insertText(text);

        // Отрисовка
        painter.save();
        painter.translate(rect.topLeft());
        textDoc.drawContents(&painter, QRectF(0, 0, rect.width(), rect.height()));
        painter.restore();
    }

    std::pair<QString, QString> splitTextByHeight(const QString &text, const qreal width,
                                                  const qreal maxHeight,
                                                  const qreal lineSpacing = 1.5) const {
        const QFontMetricsF fm(painter.font());
        const qreal lineHeight = fm.height() * lineSpacing;
        qreal currentHeight = 0;

        QString firstPart;
        QString secondPart;

        // Разбиваем текст на параграфы по переносам строк
        QStringList paragraphs = text.split('\n');

        for (int i = 0; i < paragraphs.size(); i++) {
            const QString &paragraph = paragraphs[i];

            if (paragraph.trimmed().isEmpty()) {
                // Обработка пустых строк
                if (currentHeight + lineHeight <= maxHeight) {
                    if (!firstPart.isEmpty()) firstPart += "\n";
                    firstPart += paragraph;
                    currentHeight += lineHeight;
                } else {
                    // Добавляем оставшиеся параграфы во вторую часть
                    if (!secondPart.isEmpty()) secondPart += "\n";
                    secondPart += paragraph;
                    for (int j = i + 1; j < paragraphs.size(); j++) {
                        secondPart += "\n" + paragraphs[j];
                    }
                    break;
                }
                continue;
            }

            // Разбиваем параграф на слова
            QStringList words = paragraph.split(' ', Qt::SkipEmptyParts);
            if (words.isEmpty()) continue;

            QStringList lines;
            QString currentLine = words.first();

            // Формируем строки для текущего параграфа
            for (int j = 1; j < words.size(); j++) {
                const QString &word = words[j];
                QString testLine = currentLine + " " + word;

                if (fm.horizontalAdvance(testLine) <= width) {
                    currentLine = testLine;
                } else {
                    lines.append(currentLine);
                    currentLine = word;
                }
            }
            lines.append(currentLine);

            // Обрабатываем строки параграфа
            QString paragraphFirstPart;
            QString paragraphSecondPart;

            for (int j = 0; j < lines.size(); j++) {
                if (currentHeight + lineHeight <= maxHeight) {
                    // Строка помещается в первую часть
                    if (!paragraphFirstPart.isEmpty()) paragraphFirstPart += "\n";
                    paragraphFirstPart += lines[j];
                    currentHeight += lineHeight;
                } else {
                    // Строка не помещается - переносим во вторую часть
                    for (int k = j; k < lines.size(); k++) {
                        if (!paragraphSecondPart.isEmpty()) paragraphSecondPart += "\n";
                        paragraphSecondPart += lines[k];
                    }
                    break;
                }
            }

            // Добавляем обработанный параграф в результат
            if (!paragraphFirstPart.isEmpty()) {
                if (!firstPart.isEmpty()) firstPart += "\n";
                firstPart += paragraphFirstPart;
            }

            if (!paragraphSecondPart.isEmpty()) {
                if (!secondPart.isEmpty()) secondPart += "\n";
                secondPart += paragraphSecondPart;

                // Добавляем оставшиеся параграфы во вторую часть
                for (int j = i + 1; j < paragraphs.size(); j++) {
                    secondPart += "\n" + paragraphs[j];
                }
                break;
            }

            // Если параграф завершен, но это был последний - выходим
            if (i == paragraphs.size() - 1) {
                break;
            }
        }

        return {firstPart, secondPart};
    }

    void addText(const qreal l, const qreal r, const QByteArray &text, const int format) {
        const auto w = r - l;
        //
        {
            QString t = text;
            using namespace Format;
            const QFont defaultFont = painter.font();
            QFont font = painter.font();
            font.setPointSize(format & Small ? 12 : 14);
            if (format & Italic) font.setItalic(true);
            if (format & Bold) font.setBold(true);
            painter.setFont(font);
            do {
                auto p = splitTextByHeight(t, w, pageHeight - posY);
                const auto h = calculateTextHeightAdvanced(p.first, w);
                drawTextWithWordWrap(QRectF(l, posY, w, h), Qt::AlignLeft, t);
#ifdef debug_
                QD << DUMP(p.first) << DUMP(p.second) << DUMP(h) << DUMP(w);
                painter.setPen(QPen(Qt::black, 1));
                painter.drawRect(QRectF(l, posY, w, h));
#endif
                posY += h;
                t = p.second;
                if (t.size() > 0) {
                    newPage();
                }
            } while (t.size() > 0);
            // Восстанавливаем стандартный шрифт
            if (format & Italic || format & Bold) {
                painter.setFont(defaultFont);
            }
        }
    }

    void addTableRow(const QVector<qreal> &borders, const QVector<QByteArray> &contents,
                     const QVector<int> &formats, const qreal maxRowHeight = -1,
                     const bool drawGrid = false) {
        // Проверка корректности входных данных
        if (borders.size() < 2 || contents.size() != borders.size() - 1 ||
            contents.size() != formats.size()) {
            return;
        }

        const int cellCount = contents.size();

        // Рассчитываем высоты всех ячеек
        QVector<qreal> cellHeights(cellCount);
        qreal actualMaxHeight = 0;

        for (int i = 0; i < cellCount; ++i) {
            const qreal cellWidth = borders[i + 1] - borders[i];

            if (formats[i] & 64) {
                // Картинка
                QImage image(contents[i]);
                if (!image.isNull()) {
                    // Масштабируем изображение по ширине ячейки
                    QSizeF scaledSize = image.size().scaled(static_cast<int>(cellWidth),
                                                            image.height(),
                                                            Qt::KeepAspectRatio);
                    cellHeights[i] = scaledSize.height();
                    actualMaxHeight = qMax(actualMaxHeight, cellHeights[i]);
                }
            } else {
                // Текст
                QString text = contents[i];
                // Рассчитываем высоту текста с учетом переноса слов
                qreal textHeight = calculateTextHeightAdvanced(text, cellWidth);
                cellHeights[i] = textHeight;
                actualMaxHeight = qMax(actualMaxHeight, textHeight);
            }
        }

        // Определяем итоговую высоту строки
        const qreal rowHeight = maxRowHeight > 0
                                    ? qMin(maxRowHeight, actualMaxHeight)
                                    : actualMaxHeight;

        // Проверяем, помещается ли строка на текущей странице
        if (posY + rowHeight > pageHeight) {
            // Переносим на новую страницу
            newPage();

            // Проверяем, помещается ли на новой странице
            if (rowHeight > pageHeight) {
                // Не помещается даже на пустой странице - не выводим
                return;
            }
        }

        // Рисуем ячейки
        for (int i = 0; i < cellCount; ++i) {
            const qreal left = borders[i];
            const qreal width = borders[i + 1] - borders[i];
            const QByteArray &content = contents[i];
            const int format = formats[i];

            QRectF cellRect(left, posY, width, rowHeight);
            //
            {
                using namespace Format;
                if (format & Picture) {
                    // Картинка
                    QImage image(content);
                    if (!image.isNull()) {
                        // Масштабируем изображение
                        QSizeF scaledSize = image.size().scaled(
                            static_cast<int>(width),
                            static_cast<int>(rowHeight),
                            Qt::KeepAspectRatio);

                        // Вычисляем позицию с учетом выравнивания
                        qreal x = left;
                        qreal y = posY;

                        // Горизонтальное выравнивание
                        if (format & 8) {
                            // центр
                            x += (width - scaledSize.width()) / 2;
                        } else if (format & 12) {
                            // справа
                            x += width - scaledSize.width();
                        }

                        // Вертикальное выравнивание
                        if (format & 2) {
                            // центр
                            y += (rowHeight - scaledSize.height()) / 2;
                        } else if (format & 3) {
                            // низ
                            y += rowHeight - scaledSize.height();
                        }

                        painter.drawImage(
                            QRectF(x, y, scaledSize.width(), scaledSize.height()),
                            image);
                    }
                } else {
                    // Текст
                    QString text = content;

                    // Устанавливаем стиль шрифта
                    QFont defaultFont = painter.font();
                    QFont font = painter.font();
                    font.setPointSize(format & Small ? 12 : 14);
                    if (format & Italic) font.setItalic(true);
                    if (format & Bold) font.setBold(true);
                    painter.setFont(font);

                    // Определяем флаги выравнивания для текста
                    int textAlignment = Qt::TextWordWrap; // Всегда включаем перенос слов

                    // Вертикальное выравнивание
                    switch (format & 3) {
                        case AlignTop: textAlignment |= Qt::AlignTop;
                            break;
                        case AlignVCenter: textAlignment |= Qt::AlignVCenter;
                            break;
                        case AlignBottom: textAlignment |= Qt::AlignBottom;
                            break;
                        default:
                            textAlignment |= Qt::AlignBaseline;
                    }

                    // Горизонтальное выравнивание
                    switch (format & 12) {
                        case AlignLeft: textAlignment |= Qt::AlignLeft;
                            break;
                        case AlignHCenter: textAlignment |= Qt::AlignHCenter;
                            break;
                        case AlignRight: textAlignment |= Qt::AlignRight;
                            break;
                        default:
                            textAlignment |= Qt::AlignJustify;
                    }

                    // Рисуем текст с выравниванием и переносом
                    if (format & VUse) {
                        painter.drawText(cellRect, textAlignment, text);
                    } else {
                        drawTextWithWordWrap(cellRect, textAlignment, text);
                    }

                    // Восстанавливаем стандартный шрифт
                    if (format & Italic || format & Bold) {
                        painter.setFont(defaultFont);
                    }
                }
            }
            // Отладочная рамка вокруг ячейки
            if (debug_ || drawGrid) {
                painter.setPen(QPen(Qt::black, 1));
                painter.drawRect(cellRect);
            }
        }

        // Обновляем позицию по вертикали
        posY += rowHeight;
    }

    void addParagraph(const qreal left, const qreal right, const QVector<QByteArray> &lines,
                      const QVector<int> &formats, const qreal lineSpacing = 1.5,
                      const bool drawBorders = false) {
        // Проверяем корректность входных данных
        if (lines.isEmpty() || left >= right || lines.size() != formats.size()) {
            return;
        }

        const qreal width = right - left;
        qreal currentY = posY;

        // Создаем QTextDocument для всего абзаца с разным форматированием
        QTextDocument textDoc;
        textDoc.setDocumentMargin(0);
        textDoc.setTextWidth(width);

        QTextCursor cursor(&textDoc);
        QTextBlockFormat blockFormat;
        blockFormat.setLineHeight(150, QTextBlockFormat::ProportionalHeight);
        cursor.setBlockFormat(blockFormat);

        // Вставляем весь текст с соответствующим форматированием
        for (int i = 0; i < lines.size(); ++i) {
            QString text = lines[i];

            // Создаем формат для текущего фрагмента текста
            QTextCharFormat charFormat;
            charFormat.setFont(painter.font());

            if (formats[i] & Format::Italic) charFormat.setFontItalic(true);
            if (formats[i] & Format::Bold) charFormat.setFontWeight(QFont::Bold);
            charFormat.setFontPointSize((formats[i] & Format::Small ? 12 : 14) * 3);

            // Вставляем текст с заданным форматом
            cursor.insertText(text, charFormat);
        }

        // Получаем общую высоту документа
        const qreal docHeight = textDoc.size().height();

        // Проверяем, помещается ли весь абзац на текущей странице
        if (currentY + docHeight > pageHeight) {
            // Если не помещается, разбиваем на части
            qreal remainingHeight = docHeight;
            qreal drawnHeight = 0;

            while (remainingHeight > 0) {
                qreal availableHeight = pageHeight - currentY;
                const qreal clipHeight = qMin(availableHeight, remainingHeight);

                // Рисуем часть документа, которая помещается на текущей странице
                painter.save();
                painter.translate(left, currentY);
                textDoc.drawContents(&painter, QRectF(0, -drawnHeight, width, docHeight));
                painter.restore();

                // Отладочная рамка
                if (debug_ || drawBorders) {
                    painter.setPen(QPen(Qt::blue, 1));
                    painter.drawRect(QRectF(left, currentY, width, clipHeight));
                }

                drawnHeight += clipHeight;
                currentY += clipHeight;
                remainingHeight -= clipHeight;

                // Если еще остался текст, переходим на новую страницу
                if (remainingHeight > 0) {
                    newPage();
                    currentY = posY;
                }
            }

            currentY += lineSpacing;
        } else {
            // Если весь абзац помещается на текущей странице
            painter.save();
            painter.translate(left, currentY);
            textDoc.drawContents(&painter);
            painter.restore();

            // Отладочная рамка
            if (debug_ || drawBorders) {
                painter.setPen(QPen(Qt::blue, 1));
                painter.drawRect(QRectF(left, currentY, width, docHeight));
            }

            currentY += docHeight + lineSpacing;
        }

        // Обновляем глобальную позицию
        posY = currentY;
    }

    void addDebugText(QString &text) const {
        if (!writer) return;
        // for (int i = 0; i < 50; ++i) {
        // text += QString("___________%1\n").arg(i);
        // }
        text += QString("\npdfWriter.units: %1").arg(writer->pageLayout().units());
        const auto pdfWriterRect = writer->pageLayout().pageSize().rectPixels(
            writer->resolution());
        text += QString("\npdfWriter.pageSize: %1 %2").arg(pdfWriterRect.width()).arg(
            pdfWriterRect.height());
        const QRectF pageRect = writer->pageLayout().
                paintRectPixels(writer->resolution());
        text += QString("\npageRect: left=%1 top=%2 width=%3 height=%4")
                .arg(pageRect.left())
                .arg(pageRect.top())
                .arg(pageRect.width())
                .arg(pageRect.height());
        text += QString("\npageRect(mm): left=%1 top=%2 width=%3 height=%4")
                .arg(p2mm(pageRect.left()))
                .arg(p2mm(pageRect.top()))
                .arg(p2mm(pageRect.width()))
                .arg(p2mm(pageRect.height()));
        const auto pdfWriterMargins = writer->pageLayout().margins();
        text += QString("\npdfWriterMargins: left=%1 top=%2 right=%3 bottom=%4")
                .arg(pdfWriterMargins.left())
                .arg(pdfWriterMargins.top())
                .arg(pdfWriterMargins.right())
                .arg(pdfWriterMargins.bottom());
        for (int i = 0; i < 200; ++i) {
            text += ". ";
        }
    }

    void paintDocument(const QString &text) {
        if (!writer) return;
        // Создаем QTextDocument
        QTextDocument document;
        document.setPlainText(text);

        // Настраиваем форматирование
        const QFont font("Times", 37); //14p -> 32   16p -> 37
        document.setDefaultFont(font);

        QTextOption textOption;
        textOption.setAlignment(Qt::AlignLeft);
        textOption.setWrapMode(QTextOption::WordWrap);
        document.setDefaultTextOption(textOption);

        // Устанавливаем размер страницы
        const QRectF pageRect = writer->pageLayout().paintRectPixels(writer->resolution());
        document.setPageSize(pageRect.size());
        document.setDocumentMargin(0);

        paint(document);
    }

    void paint(QTextDocument &document) {
        if (!writer) return;
        // Настройки для нумерации страниц
        const QFont pageNumberFont("Times", 14);
        painter.setFont(pageNumberFont);
        // Используем встроенное разбиение на страницы QTextDocument
        const int pageCount = document.pageCount();
        QD << DUMP(pageCount);

        QTextCursor cursor(&document);
        cursor.clearSelection();
        cursor.select(QTextCursor::Document);
        QTextBlockFormat newFormat;
        newFormat.setLineHeight(150, QTextBlockFormat::ProportionalHeight); // Интервал 150%
        cursor.setBlockFormat(newFormat);

        const QRectF pageRect = writer->pageLayout().
                paintRectPixels(writer->resolution());
        for (int i = 0; i < pageCount; ++i) {
            if (i > 0) {
                newPage();
            }
            const qreal textOffset = i < 1 ? header(&pageRect) : 0;
            painter.save();
            painter.translate(0, -i * pageRect.height() + textOffset);
            document.drawContents(&painter,
                                  QRectF(0,
                                         i * pageRect.height() - textOffset,
                                         pageRect.width(),
                                         pageRect.height()));
            painter.restore();
        }
    }

    int header(const QRectF *pageRect) {
        constexpr qreal gapX = 100;
        painter.setPen(QPen(Qt::black, 1));
        QFont font("Times", 14); // 12->14 14->16
        // Загружаем изображение
        const QImage image("../Logotype_VS.png"); // Укажите правильный путь к изображению
        if (image.isNull()) {
            return 0;
        }
        // Загружаем изображение
        const QImage image2("../CUSTOM.png"); // Укажите правильный путь к изображению
        if (image2.isNull()) {
            return 0;
        }
        // Масштабируем изображение под ширину страницы (с учетом отступов)
        const qreal availableWidth = pageRect->width();
        const int _width = static_cast<int>(availableWidth / 9);
        const QImage scaledImage = image.scaledToWidth(_width, Qt::SmoothTransformation);
        const int _height = scaledImage.height();
        const QImage scaledImage2 = image2.scaledToWidth(_width, Qt::SmoothTransformation);
        painter.drawImage(QRectF(availableWidth - _width, 0, _width, _height), scaledImage);
        painter.drawImage(QRectF(0, 0, _width, _height), scaledImage2);
        font.setItalic(true);
        painter.setFont(font);
        const qreal _mes = (availableWidth - 3 * gapX) / 2 - _width;
        // drawText(painter, QRectF(availableWidth / 10 + gapX, 0, _mes, _height),
        // "Справочные данные организации Заказчика");
        painter.drawText(QRectF(_width + gapX, 0, _mes, _height),
                         Qt::AlignVCenter | Qt::AlignLeft | Qt::TextWordWrap,
                         "Справочные данные организации Заказчика");
        painter.setFont(font);
        painter.drawText(QRectF(availableWidth / 2 + gapX / 2, 0, _mes, _height),
                         Qt::AlignVCenter | Qt::AlignRight | Qt::TextWordWrap,
                         "ООО «ВС Инжиниринг» ©ВС Сигнал. Версия: 1.0.0");
        font.setItalic(false);
        //------------------------------------------------------------------------------------
        painter.drawRect(QRectF(availableWidth / 2 - gapX / 2, 0, gapX, _height));
        painter.drawRect(QRectF(_width, 0, gapX, _height));
        painter.drawRect(QRectF(availableWidth - _width, 0, -gapX, _height));
        painter.setFont(QFont("Arial", 8));
        painter.drawText(QRectF(0, 0, availableWidth, _height), 0,
                         QString("%1 x %2").arg(availableWidth).arg(_height));
        painter.drawRect(QRectF(0, 0, availableWidth, _height));
        painter.drawRect(QRectF(-mm2p(10), -mm2p(10), mm2p(10), mm2p(10)));
        painter.drawRect(QRectF(mm2p(50), mm2p(50), mm2p(100), mm2p(100)));
        painter.drawRect(QRectF(0, 0, pageRect->width(), pageRect->height()));
        //--------------------------------------------------------------------------------
        return _height + 20;
    }

    void startPagination() {
        pageNumber = 1;
    }

    void drawPageNumber() {
        if (!writer) return;
        const auto f = painter.font();
        const QFont pageNumberFont("Times", 12);
        painter.setFont(pageNumberFont);
        const QFontMetrics pageNumberMetrics(pageNumberFont);
        const int pageNumberWidth = pageNumberMetrics.horizontalAdvance(pageNumber);
        const QRectF pageRect = writer->pageLayout().paintRectPixels(writer->resolution());
        painter.drawText(
            QPointF(pageRect.width() - pageNumberWidth - 20, pageRect.height() + 40),
            QString("%1").arg(pageNumber));
        painter.setFont(f);
    }

    void newPage() {
        if (!writer) return;
        drawPageNumber();
        writer->newPage();
        pageNumber++;
        posY = 0;
    }

    qreal width() const {
        if (!writer) return 0;
        return writer->width();
    }

    void setFont(const QFont &font) {
        if (!writer) return;
        painter.setFont(font);
    }

    void skip(const qreal i) {
        posY += i;
    }
};

class PdfApp final : public QMainWindow {
    Q_OBJECT

    // Элементы интерфейса
    QTextEdit textEdit;
    QPdfView pdfView;
    QPushButton createButton;
    QPushButton openButton;
    QPushButton printButton;
    QWidget centralWidget;
    QVBoxLayout mainLayout;
    QHBoxLayout buttonLayout;
    QHBoxLayout pageNavigationLayout;
    QWidget pageNavigationWidget;
    QSpinBox pageSpinBox;
    QLabel pageCountLabel;
    QLabel notation;
    QLabel pageLabel;
    QSplitter splitter;
    QBuffer buffer;
    QByteArray pdfData;
    QPdfDocument document;

public:
    static void _saveImage(QPdfDocument *document) {
        if (document->pageCount() > 0) {
            const QImage image = document->render(0, QSize(300, 400));
            if (!image.isNull()) {
                qDebug() << "Первая страница успешно отрендерена. Размер:"
                        << image.size();
                if (!image.save("debug_page.png")) {
                }
            } else {
                qDebug() << "Не удалось отрендерить первую страницу.";
            }
        } else {
            qDebug() << "В документе нет страниц.";
        }
    }

    static void _save(const QByteArray &data) {
        QFile file("tmp.pdf");
        if (file.open(QIODevice::WriteOnly)) {
            file.write(data);
            file.close();
            QD << "Файл успешно сохранен";
        } else {
            QD << "Ошибка открытия файла:" << file.errorString();
        }
    }

    explicit PdfApp(QWidget *parent = nullptr) : QMainWindow(parent),
                                                 splitter(Qt::Vertical, &centralWidget) {
        setupUI();
        setupConnections();
    }

private slots:
    void updatePageNavigation() {
        const int pageCount = document.pageCount();
        pageSpinBox.setRange(1, qMax(1, pageCount));
        pageCountLabel.setText(QString(" / %1").arg(pageCount));
        // В Qt 5.15 используем pageNavigation()
        if (pageCount > 0) {
            // Устанавливаем первую страницу
            pdfView.pageNavigation()->setCurrentPage(0);
        }
        // Активируем/деактивируем спинбокс в зависимости от наличия страниц
        pageSpinBox.setEnabled(pageCount > 0);
    }

    void createPdf_B() {
        pdfData.clear();
        //
        {
            using namespace Format;
            Pdf doc(&buffer);
            doc.setPageSize(210 - 20 + 2, 297 - 20 + 2);
            doc.setMargins(11, 11, 1, 11);
            doc.begin(); {
                using namespace Format;
                doc.setFont(QFont("Times", 14));
                qreal w = doc.width();
                doc.addTableRow({0, 230, 300, w / 2 - 50, w / 2 + 50, w - 300, w - 230, w},
                                {
                                    "../res/CUSTOM.png",
                                    "",
                                    // "ООО «ВС Инжиниринг»\n©ВС Сигнал. Версия: 1.0.0",
                                    "Справочные данные организации Заказчика",
                                    "",
                                    "ООО «ВС Инжиниринг»\n©ВС Сигнал. Версия: 1.0.0",
                                    "",
                                    "../res/VS.png"
                                },
                                {
                                    Picture, 0,
                                    AlignVCenter + AlignLeft + Italic + VUse, 0,
                                    AlignVCenter + AlignRight + Italic + VUse, 0,
                                    Picture
                                },
                                230);
                doc.skip(40);
                doc.addTableRow({150, 650, w}, {
                                    "Источник данных:",
                                    "Завод_Установка_Агрегат_Точка Измерения"
                                }, {AlignBottom + VUse, AlignBottom + Italic + Small + VUse});
                doc.addTableRow({150, 700, w}, {
                                    "Дата и время печати:",
                                    "ДД.ММ.ГГГГ чч:мм:сс"
                                }, {AlignBottom + VUse, AlignBottom + Italic + Small + VUse});
                doc.addTableRow({150, w}, {"Рассчитанные значения:"}, {AlignBottom + VUse});
                doc.skip(20);
                doc.addTableRow({
                                    0, w / 9, 3 * w / 9, 4 * w / 9, 6 * w / 9,
                                    7 * w / 9 + 100, w
                                },
                                {
                                    " СКЗ:",
                                    "______ед. изм.",
                                    " Макс.:",
                                    "______ед. изм.",
                                    " Оборотная:",
                                    "______ед. изм.",
                                },
                                {
                                    AlignBottom + VUse, AlignBottom + Small + VUse,
                                    AlignBottom + VUse, AlignBottom + Small + VUse,
                                    AlignBottom + VUse, AlignBottom + Small + VUse
                                },
                                65, true);
                doc.skip(100);
                doc.addTableRow({0, w}, {"../res/Plot.png"}, {Picture}, w);
                doc.addTableRow({0, w}, {"Подпись рисунка"}, {
                                    AlignBottom + AlignHCenter + Small + Italic
                                });
                doc.addTableRow({150, 750, w}, {
                                    "Алгоритмы обработки:",
                                    "ФНЧ 1000 Гц, ФВЧ 5 Гц"
                                }, {AlignBottom + VUse, AlignBottom + Italic + Small + VUse});
                doc.skip(150);
                doc.addTableRow({150, w}, {"Примечание:"}, {AlignBottom + Italic + VUse});
                doc.skip(-70);
                doc.addText(0, w,
                            "                                       " + textEdit.
                            toPlainText().toUtf8(), Italic + Small);
            }
        }

        _save(pdfData);

        // Загружаем в документ
        if (!buffer.open(QIODevice::ReadOnly)) {
            QMessageBox::critical(this, "Ошибка", "Не удалось открыть буфер для чтения");
            return;
        }

        document.load(&buffer);
        updatePageNavigation();
        QD << "done, создано страниц: " << document.pageCount();
    }

    void printPdf() {
        if (document.pageCount() == 0) {
            QMessageBox::warning(this, "Предупреждение", "Нет документа для печати");
            return;
        }

        // Создаем принтер и диалог печати
        QPrinter printer(QPrinter::HighResolution);

        printer.setColorMode(QPrinter::Color);
        printer.setCollateCopies(true);
        printer.setFullPage(true);
        printer.setCopyCount(1); //setNumCopies
        printer.setDuplex(QPrinter::DuplexMode::DuplexNone);

        QPageLayout pageLayout;
        // Устанавливаем мм как единицы измерения
        pageLayout.setPageSize(QPageSize(QPageSize::A5)); // работает странно, не убирать
        QD << pageLayout.pageSize();
        pageLayout.setUnits(QPageLayout::Millimeter);
        pageLayout.setMargins(QMarginsF(9 + 4, 4 + 4, 1 + 4, 6 + 4)); // Отступы в мм
        pageLayout.setOrientation(QPageLayout::Orientation::Portrait);
        printer.setPageLayout(pageLayout);
        QD << printer.pageLayout().pageSize().id();

        QPrintDialog printDialog(&printer, this);
        printDialog.setWindowTitle("Печать документа");
        if (printDialog.exec() == QDialog::Accepted) {
            QPainter painter;
            if (!painter.begin(&printer)) {
                QMessageBox::critical(this, "Ошибка", "Не удалось начать печать");
                return;
            }

            const bool isLandscape = printer.pageLayout().orientation() ==
                                     QPageLayout::Landscape;

            for (int pageIndex = 0; pageIndex < document.pageCount(); ++pageIndex) {
                if (pageIndex > 0) {
                    printer.newPage();
                }

                QSizeF pdfPageSize = document.pageSize(pageIndex);
                QRectF printerRect = printer.pageRect(QPrinter::DevicePixel);

                // Корректировка размеров с учетом ориентации
                if (isLandscape) {
                    // Для альбомной ориентации учитываем поворот
                    printerRect = QRectF(printerRect.y(), printerRect.x(),
                                         printerRect.height(), printerRect.width());
                }

                // Рассчитываем масштаб с учетом ориентации
                qreal scaleX = printerRect.width() / pdfPageSize.width();
                qreal scaleY = printerRect.height() / pdfPageSize.height();
                const qreal scale = qMin(scaleX, scaleY);

                // Размер для рендеринга с учетом масштаба и высокого DPI для качества
                QSize renderSize(static_cast<int>(pdfPageSize.width() * scale * 2),
                                 static_cast<int>(pdfPageSize.height() * scale * 2));

                // Рендерим страницу PDF
                QImage image = document.render(pageIndex, renderSize);

                if (!image.isNull()) {
                    // Позиционирование с центрированием
                    QPointF imagePos(
                        (printerRect.width() - static_cast<double>(renderSize.width()) / 2) /
                        2,
                        (printerRect.height() - static_cast<double>(renderSize.height()) / 2)
                        / 2);

                    // Рисуем изображение с учетом масштаба
                    painter.drawImage(
                        QRectF(imagePos, QSizeF(static_cast<double>(renderSize.width()) / 2,
                                                static_cast<double>(renderSize.height()) /
                                                2)),
                        image
                    );
                }
            }

            painter.end();
            QMessageBox::information(this, "Информация",
                                     "Документ отправлен на печать.\n"
                                     "Выбранный принтер: " + printer.printerName() + "\n" +
                                     "Копий: " + QString::number(printer.copyCount()));
        }
        qDebug() << printer.pageLayout().pageSize().id();
    }

    void openPdf() {
        const QString fileName = QFileDialog::getOpenFileName(
            this, "Открыть PDF", "", "PDF Files (*.pdf)");
        if (!fileName.isEmpty()) {
            loadPdfForView(fileName);
        }
    }

    void onPageChanged(const int page) const {
        // page в spinbox начинается с 1, а в QPdfView с 0
        const int pageIndex = page - 1;
        if (pageIndex >= 0 && pageIndex < document.pageCount()) {
            // В Qt 5.15 используем pageNavigation()
            pdfView.pageNavigation()->setCurrentPage(pageIndex);
        }
    }

    void onCurrentPageChanged(const int page) {
        // Блокируем сигналы, чтобы избежать рекурсии
        pageSpinBox.blockSignals(true);
        pageSpinBox.setValue(page + 1); // Конвертируем из 0-based в 1-based
        pageSpinBox.blockSignals(false);
    }

private:
    void setupPageNavigation() {
        // Панель навигации по страницам
        pageLabel.setText("Страница:");
        pageSpinBox.setRange(1, 1);
        pageSpinBox.setValue(1);
        pageSpinBox.setEnabled(false);
        pageCountLabel.setText(" / 0");

        pageNavigationLayout.addWidget(&pageLabel);
        pageNavigationLayout.addWidget(&pageSpinBox);
        pageNavigationLayout.addWidget(&pageCountLabel);
        pageNavigationLayout.addStretch();
        pageNavigationWidget.setLayout(&pageNavigationLayout);
        pageNavigationWidget.setMaximumHeight(40);
    }

    void setupUI() {
        buffer.setBuffer(&pdfData);
        setCentralWidget(&centralWidget);
        centralWidget.setLayout(&mainLayout);

        // Создаем элементы управления
        createButton.setText("Создать PDF");
        openButton.setText("Открыть PDF");
        printButton.setText("Печать");
        setupPageNavigation();
        buttonLayout.addWidget(&createButton);
        buttonLayout.addWidget(&openButton);
        buttonLayout.addWidget(&printButton);
        buttonLayout.addWidget(&pageNavigationWidget);

        notation.setText("Добавьте примечание:");

        // Создаем splitter для горизонтального расположения
        textEdit.setPlaceholderText("Введите текст для создания PDF...");

        // Устанавливаем шрифт соответствующий PDF
        const QFont textEditFont("Times", 12);
        textEdit.setFont(textEditFont);

        // m_pdfView = new QPdfView();
        pdfView.setDocument(&document);

        // Добавляем виджеты в splitter
        splitter.addWidget(&textEdit);
        splitter.addWidget(&pdfView);

        splitter.setStretchFactor(0, 20);
        splitter.setStretchFactor(1, 100);

        mainLayout.addLayout(&buttonLayout);
        mainLayout.addWidget(&notation);
        mainLayout.addWidget(&splitter); // Добавляем splitter вместо отдельных виджетов

        // Сохраняем указатели на кнопки для соединений

        setWindowTitle("PDF приложение Qt5");
        setGeometry(QRect(1920, 0, 960, 1080));
        // Увеличиваем начальный размер окна для горизонтального расположения
    }

    void setupConnections() {
        connect(&createButton, &QPushButton::clicked, this, &PdfApp::createPdf_B);
        connect(&openButton, &QPushButton::clicked, this, &PdfApp::openPdf);
        connect(&printButton, &QPushButton::clicked, this, &PdfApp::printPdf);
        connect(&pageSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
                this, &PdfApp::onPageChanged);

        connect(&document, &QPdfDocument::statusChanged, this,
                [this](const QPdfDocument::Status status) {
                    QD << DUMP(status);
                    if (document.status() == QPdfDocument::Error) {
                        qDebug() << "Ошибка при загрузке документа:" << document.error();
                    } else if (document.status() == QPdfDocument::Ready) {
                        updatePageNavigation();
                    }
                });

        connect(&document, &QPdfDocument::pageCountChanged, this,
                &PdfApp::updatePageNavigation);

        // В Qt 5.15 используем pageNavigation() вместо pageNavigator()
        connect(pdfView.pageNavigation(), &QPdfPageNavigation::currentPageChanged,
                this, &PdfApp::onCurrentPageChanged);
    }

    void loadPdfForView(const QString &fileName) {
        if (document.load(fileName) != QPdfDocument::NoError) {
            QMessageBox::critical(this, "Ошибка",
                                  "Не удалось загрузить PDF файл: " + fileName);
        } else {
            updatePageNavigation();
        }
    }
};

#ifdef Q_OS_UNIX
#include <unistd.h>
#include <sys/syscall.h>
#include <QDateTime>
#include <execinfo.h>   // Для backtrace
#include <cxxabi.h>     // Для деманглинга имен функций

void printBacktrace() {
    constexpr int maxFrames = 20;
    void *frames[maxFrames];
    const int frameCount = backtrace(frames, maxFrames);
    char **symbols = backtrace_symbols(frames, frameCount);
    if (!symbols) {
        fprintf(stderr, "Ошибка получения backtrace\n");
        return;
    }
    fprintf(stderr, "Stack trace:\n");
    for (int i = 0; i < frameCount; ++i) {
        // Деманглинг (опционально)
        char *mangledName = nullptr, *offsetBegin = nullptr, *offsetEnd = nullptr;
        for (char *p = symbols[i]; *p; ++p) {
            if (*p == '(')
                mangledName = p;
            else if (*p == '+')
                offsetBegin = p;
            else if (*p == ')') {
                offsetEnd = p;
                break;
            }
        }
        if (mangledName && offsetBegin && offsetEnd && mangledName < offsetBegin) {
            *offsetBegin = '\0';
            *offsetEnd = '\0';
            int status;
            char *realName = abi::__cxa_demangle(mangledName + 1, nullptr, nullptr, &status);
            if (status == 0 && realName) {
                fprintf(stderr, "  %s : %s+%s\n", symbols[i], realName, offsetBegin + 1);
                free(realName);
            } else {
                fprintf(stderr, "  %s : %s+%s\n", symbols[i], mangledName + 1,
                        offsetBegin + 1);
            }
            *offsetBegin = '+';
            *offsetEnd = ')';
        } else {
            fprintf(stderr, "  %s\n", symbols[i]);
        }
    }
    free(symbols);
}
#endif

bool debug = true;

void myMessageHandler(const QtMsgType type, const QMessageLogContext &context,
                      const QString &msg) {
    if (!debug) return;
    QString logLevel;
    switch (type) {
        case QtDebugMsg: logLevel = "DEBUG";
            break;
        case QtInfoMsg: logLevel = "INFO";
            break;
        case QtWarningMsg: logLevel = "WARNING";
            break;
        case QtCriticalMsg: logLevel = "CRITICAL";
            break;
        case QtFatalMsg: logLevel = "FATAL";
#ifdef Q_OS_UNIX
            // Печатаем бектрейс
            printBacktrace();
#endif
            break;
    }

#ifdef _WIN64
    long tid = static_cast<long>(reinterpret_cast<quintptr>(QThread::currentThreadId()));
#endif
#ifdef __linux__
    long tid = syscall(SYS_gettid);
#endif

    const QString formattedMsg = QString("[%1 tid: %7] %2 %3:%4, %5\n%6")
            .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz"), logLevel,
                 QString(context.file))
            .arg(context.line).arg(QString(context.function), msg).arg(tid);
    fprintf(stderr, "%s\n", formattedMsg.toLocal8Bit().constData());
    fflush(stderr);
}

int main(int argc, char *argv[]) {
    qInstallMessageHandler(myMessageHandler);
    QApplication app(argc, argv);
    PdfApp window;
    window.show();
    return QApplication::exec();
}

#include "main.moc"
