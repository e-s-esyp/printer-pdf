#include <QApplication>
#include <QMainWindow>
#include <QPainter>
#include <QRect>
#include <QHBoxLayout>
#include <QTextEdit>
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
#include <QTime>
#include <QEvent>
#include <QMap>
#include <QTimer>
#include <QSpinBox>
#include <QLabel>
#include <QPdfPageNavigation>
#include <QPlainTextDocumentLayout>
#include <qmath.h>
#include <QSplitter>
#include <QDebug>

#define QD qDebug()
#define DUMP(var)  #var << ": " << (var) << " "
#define DUMPH(var) #var << ": " << QString::number(var, 16) << " "
#define DUMPS(var) #var << ": " << QString::fromLatin1(var, 4) << " "

class PdfApp final : public QMainWindow {
    Q_OBJECT

    // Элементы интерфейса
    QTextEdit *m_textEdit{};
    QPdfView *m_pdfView{};
    QPushButton *m_createButton{};
    QPushButton *m_openButton{};
    QPushButton *m_printButton{};
    QWidget *centralWidget{};
    QVBoxLayout *mainLayout{};
    QHBoxLayout *buttonLayout{};
    QHBoxLayout *pageNavigationLayout{};
    QSpinBox *m_pageSpinBox{};
    QLabel *m_pageCountLabel{};
    QSplitter *m_splitter{};
    QBuffer m_buffer{};
    QByteArray m_pdfData;
    QPdfDocument m_document;

public:
    void updatePageNavigation() const {
        const int pageCount = m_document.pageCount();
        m_pageSpinBox->setRange(1, qMax(1, pageCount));
        m_pageCountLabel->setText(QString(" / %1").arg(pageCount));
        // В Qt 5.15 используем pageNavigation()
        if (pageCount > 0) {
            // Устанавливаем первую страницу
            m_pdfView->pageNavigation()->setCurrentPage(0);
        }
        // Активируем/деактивируем спинбокс в зависимости от наличия страниц
        m_pageSpinBox->setEnabled(pageCount > 0);
    }

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

    explicit PdfApp(QWidget *parent = nullptr) : QMainWindow(parent) {
        setupUI();
        setupConnections();
    }

private slots:
    static void drawText(QPainter &painter, const QRectF &rect, const QString &text) {
        QTextDocument textDoc;
        textDoc.setDefaultFont(painter.font()); // Используем текущий шрифт painter'а
        textDoc.setTextWidth(rect.width());
        QTextCursor cursor(&textDoc);
        QTextBlockFormat blockFormat;
        blockFormat.setLineHeight(150, QTextBlockFormat::ProportionalHeight); // 150% интервал
        cursor.setBlockFormat(blockFormat);
        cursor.insertText(text);
        textDoc.adjustSize();
        const qreal docHeight = textDoc.size().height();
        qreal startY = 0;
        if (docHeight < rect.height()) {
            startY = (rect.height() - docHeight) / 2; // Выравнивание по вертикали
        }
        painter.save();
        painter.translate(rect.left(), startY);
        textDoc.drawContents(&painter);
        painter.restore();
    }

    int header(QPainter &painter, const QRectF *pageRect) {
        constexpr qreal gapX = 100;
        painter.setPen(QPen(Qt::black, 1));
        QFont font("Times", 14); // 12->14 14->16
        // Загружаем изображение
        const QImage image("../Logotype_VS.png"); // Укажите правильный путь к изображению
        if (image.isNull()) {
            QMessageBox::warning(this, "Предупреждение", "Не удалось загрузить изображение");
            return 0;
        }
        // Загружаем изображение
        const QImage image2("../CUSTOM.png"); // Укажите правильный путь к изображению
        if (image2.isNull()) {
            QMessageBox::warning(this, "Предупреждение", "Не удалось загрузить изображение");
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

    static double mm(const qreal x) {
        return x * 25.4 / 300.0;
    }

    static double mm2p(const qreal x) {
        return x * 300.0 / 25.4;
    }

    static void debugText(QPdfWriter &pdfWriter, QString &text) {
        text += "___________0\n" "___________\n" "___________\n" "___________\n"
                "___________\n" "___________\n" "___________\n" "___________\n"
                "___________\n" "___________\n" "___________10\n" "___________\n"
                "___________\n" "___________\n" "___________\n" "___________\n"
                "___________\n" "___________\n" "___________\n" "___________\n"
                "___________20";
        text += QString("\npdfWriter.units: %1").arg(pdfWriter.pageLayout().units());
        pdfWriter.setResolution(300);
        auto pdfWriterRect = pdfWriter.pageLayout().pageSize().rectPixels(
            pdfWriter.resolution());
        text += QString("\npdfWriter.pageSize: %1 %2").arg(pdfWriterRect.width()).arg(
            pdfWriterRect.height());
        const QRectF pageRect = pdfWriter.pageLayout().
                paintRectPixels(pdfWriter.resolution());
        text += QString("\npageRect: left=%1 top=%2 width=%3 height=%4")
                .arg(pageRect.left())
                .arg(pageRect.top())
                .arg(pageRect.width())
                .arg(pageRect.height());
        text += QString("\npageRect(mm): left=%1 top=%2 width=%3 height=%4")
                .arg(mm(pageRect.left()))
                .arg(mm(pageRect.top()))
                .arg(mm(pageRect.width()))
                .arg(mm(pageRect.height()));
        const auto pdfWriterMargins = pdfWriter.pageLayout().margins();
        text += QString("\npdfWriterMargins: left=%1 top=%2 right=%3 bottom=%4")
                .arg(pdfWriterMargins.left())
                .arg(pdfWriterMargins.top())
                .arg(pdfWriterMargins.right())
                .arg(pdfWriterMargins.bottom());
        for (int i = 0; i < 200; ++i) {
            text += ". ";
        }
    }

    void createPdf_B() {
        QString text = m_textEdit->toPlainText();
        if (text.isEmpty()) {
            QMessageBox::warning(this, "Предупреждение", "Введите текст для создания PDF");
            return;
        }

        m_pdfData.clear();
        if (m_buffer.isOpen()) {
            m_buffer.close();
        }
        if (!m_buffer.open(QIODevice::WriteOnly)) {
            QMessageBox::critical(this, "Ошибка", "Не удалось открыть буфер для записи");
            return;
        }

        QPdfWriter pdfWriter(&m_buffer);
        pdfWriter.setPageSize(QPageSize(QSize(210 - 20 + 2, 297 - 20 + 2),
                                        QPageSize::Millimeter,
                                        "VS-Report"));
        auto pdfWriterLayout = pdfWriter.pageLayout();
        pdfWriterLayout.setUnits(QPageLayout::Millimeter);
        pdfWriterLayout.setMargins(QMarginsF(11, 11, 1, 11));
        pdfWriter.setPageLayout(pdfWriterLayout);

        debugText(pdfWriter, text);

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
        const QRectF pageRect = pdfWriter.pageLayout().
                paintRectPixels(pdfWriter.resolution());

        // Устанавливаем размер страницы
        document.setPageSize(pageRect.size());
        const double _documentMargin = 0; //pageRect.left(); //50 + 78;
        document.setDocumentMargin(_documentMargin);
        QPainter painter;
        if (!painter.begin(&pdfWriter)) {
            QMessageBox::critical(this, "Ошибка", "Не удалось начать рисование");
            m_buffer.close();
            return;
        }

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

        for (int i = 0; i < pageCount; ++i) {
            if (i > 0) {
                pdfWriter.newPage();
            }
            const qreal textOffset = i == 0 ? header(painter, &pageRect) : 0;
            painter.save();
            painter.translate(0, -i * pageRect.height() + textOffset);
            document.drawContents(&painter,
                                  QRectF(0,
                                         i * pageRect.height() - textOffset,
                                         pageRect.width(),
                                         pageRect.height() - textOffset));
            painter.restore();
            if (pageCount > 1) {
                // Добавляем номер страницы
                QString pageNumber = QString::number(i + 1);
                QFontMetrics pageNumberMetrics(pageNumberFont);
                const int pageNumberWidth = pageNumberMetrics.horizontalAdvance(pageNumber);
                painter.drawText(
                    QPointF(pageRect.width() - pageNumberWidth - 20, pageRect.height() + 40),
                    pageNumber
                );
            }
        }
        painter.end();
        m_buffer.close();
        _save(m_pdfData);

        // Загружаем в документ
        if (!m_buffer.open(QIODevice::ReadOnly)) {
            QMessageBox::critical(this, "Ошибка", "Не удалось открыть буфер для чтения");
            return;
        }

        m_document.load(&m_buffer);
        updatePageNavigation();
        QD << "done, создано страниц: " << m_document.pageCount();
    }

    void printPdf() {
        if (m_document.pageCount() == 0) {
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
        // ReSharper disable once CppDeprecatedEntity
        QD << printer.pageSize();
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

            for (int pageIndex = 0; pageIndex < m_document.pageCount(); ++pageIndex) {
                if (pageIndex > 0) {
                    printer.newPage();
                }

                QSizeF pdfPageSize = m_document.pageSize(pageIndex);
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
                QImage image = m_document.render(pageIndex, renderSize);

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
        if (pageIndex >= 0 && pageIndex < m_document.pageCount()) {
            // В Qt 5.15 используем pageNavigation()
            m_pdfView->pageNavigation()->setCurrentPage(pageIndex);
        }
    }

    void onCurrentPageChanged(const int page) const {
        // Блокируем сигналы, чтобы избежать рекурсии
        m_pageSpinBox->blockSignals(true);
        m_pageSpinBox->setValue(page + 1); // Конвертируем из 0-based в 1-based
        m_pageSpinBox->blockSignals(false);
    }

private:
    void setupUI() {
        m_buffer.setBuffer(&m_pdfData);

        centralWidget = new QWidget(this);
        setCentralWidget(centralWidget);

        mainLayout = new QVBoxLayout(centralWidget);

        // Создаем элементы управления
        buttonLayout = new QHBoxLayout();

        // Панель навигации по страницам
        pageNavigationLayout = new QHBoxLayout();
        auto *pageLabel = new QLabel("Страница:");
        m_pageSpinBox = new QSpinBox();
        m_pageSpinBox->setRange(1, 1);
        m_pageSpinBox->setValue(1);
        m_pageSpinBox->setEnabled(false);
        m_pageCountLabel = new QLabel(" / 0");

        pageNavigationLayout->addWidget(pageLabel);
        pageNavigationLayout->addWidget(m_pageSpinBox);
        pageNavigationLayout->addWidget(m_pageCountLabel);
        pageNavigationLayout->addStretch();
        auto *pageNavigationWidget = new QWidget();
        pageNavigationWidget->setLayout(pageNavigationLayout);
        pageNavigationWidget->setMaximumHeight(40);

        // Создаем splitter для горизонтального расположения
        m_splitter = new QSplitter(Qt::Horizontal, centralWidget);

        m_textEdit = new QTextEdit();
        m_textEdit->setPlaceholderText("Введите текст для создания PDF...");
        // Устанавливаем шрифт соответствующий PDF
        const QFont textEditFont("Times", 12);
        m_textEdit->setFont(textEditFont);

        m_pdfView = new QPdfView();
        m_pdfView->setDocument(&m_document);

        // Добавляем виджеты в splitter
        m_splitter->addWidget(m_textEdit);
        m_splitter->addWidget(m_pdfView);

        // Устанавливаем начальные пропорции (50/50)
        // m_splitter->setSizes({1, 3});
        m_splitter->setStretchFactor(0, 20);
        m_splitter->setStretchFactor(1, 100);

        auto *createButton = new QPushButton("Создать PDF");
        auto *openButton = new QPushButton("Открыть PDF");
        auto *printButton = new QPushButton("Печать");

        buttonLayout->addWidget(createButton);
        buttonLayout->addWidget(openButton);
        buttonLayout->addWidget(printButton);
        buttonLayout->addWidget(pageNavigationWidget);

        mainLayout->addLayout(buttonLayout);
        mainLayout->addWidget(m_splitter); // Добавляем splitter вместо отдельных виджетов

        // Сохраняем указатели на кнопки для соединений
        m_createButton = createButton;
        m_openButton = openButton;
        m_printButton = printButton;

        setWindowTitle("PDF приложение Qt5");
        setGeometry(QRect(1920, 0, 960, 1080));
        // Увеличиваем начальный размер окна для горизонтального расположения
    }

    void setupConnections() {
        connect(m_createButton, &QPushButton::clicked, this, &PdfApp::createPdf_B);
        connect(m_openButton, &QPushButton::clicked, this, &PdfApp::openPdf);
        connect(m_printButton, &QPushButton::clicked, this, &PdfApp::printPdf);
        connect(m_pageSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
                this, &PdfApp::onPageChanged);

        connect(&m_document, &QPdfDocument::statusChanged, this,
                [this](const QPdfDocument::Status status) {
                    QD << DUMP(status);
                    if (m_document.status() == QPdfDocument::Error) {
                        qDebug() << "Ошибка при загрузке документа:" << m_document.error();
                    } else if (m_document.status() == QPdfDocument::Ready) {
                        updatePageNavigation();
                    }
                });

        connect(&m_document, &QPdfDocument::pageCountChanged, this,
                &PdfApp::updatePageNavigation);

        // В Qt 5.15 используем pageNavigation() вместо pageNavigator()
        connect(m_pdfView->pageNavigation(), &QPdfPageNavigation::currentPageChanged,
                this, &PdfApp::onCurrentPageChanged);
    }

    void loadPdfForView(const QString &fileName) {
        if (m_document.load(fileName) != QPdfDocument::NoError) {
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
    QByteArray localMsg = msg.toLocal8Bit();
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
