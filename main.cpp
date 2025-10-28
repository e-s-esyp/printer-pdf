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
#include <QDebug>
#define QD qDebug()
#define DUMP(var)  #var << ": " << (var) << " "
#define DUMPH(var) #var << ": " << QString::number(var, 16) << " "
#define DUMPS(var) #var << ": " << QString::fromLatin1(var, 4) << " "

class PdfApp final : public QMainWindow {
    Q_OBJECT

    QWidget *centralWidget{};
    QVBoxLayout *mainLayout{};
    QHBoxLayout *buttonLayout{};
    QBuffer m_buffer{};
    QByteArray m_pdfData;

public:
    explicit PdfApp(QWidget *parent = nullptr) : QMainWindow(parent) {
        setupUI();
        setupConnections();
        m_document = new QPdfDocument(this);
        m_pdfView->setDocument(m_document);
        m_buffer.setBuffer(&m_pdfData);
    }

private slots:
    void createPdf() {
        // Получаем текст из текстового редактора
        const QString text = m_textEdit->toPlainText();
        if (text.isEmpty()) {
            QMessageBox::warning(this, "Предупреждение", "Введите текст для создания PDF");
            return;
        }

        // Запрашиваем место сохранения файла
        QString fileName = QFileDialog::getSaveFileName(
            this, "Сохранить PDF", "", "PDF Files (*.pdf)");
        if (fileName.isEmpty()) return;

        // Добавляем расширение .pdf если его нет
        if (!fileName.endsWith(".pdf", Qt::CaseInsensitive)) {
            fileName += ".pdf";
        }

        // Создаем PDF принтер
        QPrinter printer(QPrinter::HighResolution);
        printer.setOutputFormat(QPrinter::PdfFormat);
        printer.setPageSize(QPageSize(QPageSize::A4));
        printer.setOutputFileName(fileName);

        // Создаем painter и рисуем содержимое
        QPainter painter;
        if (!painter.begin(&printer)) {
            QMessageBox::critical(this, "Ошибка", "Не удалось начать рисование на принтере");
            return;
        }

        // Устанавливаем шрифт и рисуем текст
        QFont font = painter.font();
        font.setPointSize(12);
        painter.setFont(font);

        const QRectF pageRect = printer.pageRect(QPrinter::DevicePixel);
        painter.drawText(pageRect, Qt::AlignLeft | Qt::TextWordWrap, text);

        painter.end();

        QMessageBox::information(this, "Успех", "PDF файл успешно создан: " + fileName);

        // Загружаем созданный PDF для просмотра
        loadPdfForView(fileName);
    }

    void createPdf_B() {
        QD;
        // Получаем текст из текстового редактора
        const QString text = m_textEdit->toPlainText();
        if (text.isEmpty()) {
            QMessageBox::warning(this, "Предупреждение", "Введите текст для создания PDF");
            return;
        }
        QD;
        // Создаем буфер для хранения PDF данных
        // m_pdfData.clear();
        // m_buffer.reset();
        // m_buffer.setBuffer(&m_pdfData);
        m_buffer.open(QIODevice::WriteOnly);
        QD;
        // Создаем PDF writer
        QPdfWriter pdfWriter(&m_buffer);
        pdfWriter.setPageSize(QPageSize(QPageSize::A4));
        pdfWriter.setResolution(300); // High resolution
        // Создаем painter и рисуем содержимое
        QPainter painter;
        if (!painter.begin(&pdfWriter)) {
            QMessageBox::critical(this, "Ошибка", "Не удалось начать рисование");
            m_buffer.close();
            return;
        }
        // Устанавливаем шрифт и рисуем текст
        QFont font("Times", 14);
        painter.setFont(font);
        const QRectF pageRect =
                pdfWriter.pageLayout().paintRectPixels(pdfWriter.resolution());
        painter.drawText(pageRect, Qt::AlignLeft | Qt::TextWordWrap, text);
        painter.end();
        m_buffer.close();
        // Загружаем данные в QPdfDocument.
        // Открываем буфер для чтения
        if (!m_buffer.open(QIODevice::ReadOnly)) {
            QMessageBox::critical(this, "Ошибка", "Не удалось открыть буфер для чтения");
            return;
        }
        // Создаем документ, если еще не создан
        if (!m_document) {
            QD<<"new m_document";
            m_document = new QPdfDocument(this);
        }
        m_document->load(&m_buffer);
        connect(m_document, &QPdfDocument::statusChanged, this,
                [this](const QPdfDocument::Status status) {
                    QD << DUMP(status);
                    if (status == QPdfDocument::Status::Ready || status ==
                        QPdfDocument::Status::Error) {
                        m_buffer.close();
                        // закрываем буфер, когда загрузка завершена (успешно или с ошибкой)
                        if (status == QPdfDocument::Status::Ready) {
                            QD << "Документ успешно загружен, страниц:" << m_document->
                                    pageCount();
                            m_pdfView->setDocument(m_document);
                        } else {
                            QD << "Ошибка загрузки документа:" << m_document->error();
                        }
                    }
                });
        QD << "done";
        // QMessageBox::information(this, "Успех", "PDF документ успешно создан в памяти");
    }

    void printPdf() {
        if (!m_document || m_document->pageCount() == 0) {
            QMessageBox::warning(this, "Предупреждение", "Нет документа для печати");
            return;
        }

        // Создаем принтер и диалог печати
        QPrinter printer(QPrinter::HighResolution);

        // if (!printer.supportedPageSizes().contains(QPageSize::A5)) {
        // qDebug() << "A5 not supported, using default";
        // }

        // Настраиваем параметры печати по умолчанию
        // printer.setOutputFormat(OutputFormat format);
        // printer.setPdfVersion(PdfVersion version);
        // printer.setPrinterName(const QString &);
        // printer.setOutputFileName(const QString &);
        // printer.setPrintProgram(const QString &);
        // printer.setDocName(const QString &);
        // printer.setCreator(const QString &);
        printer.setPageSize(QPageSize(QSizeF(297, 210), QPageSize::Unit::Millimeter));
        //297x210
        // printer.setPageMargins
        printer.setPageOrientation(QPageLayout::Orientation::Landscape); //setOrientation
        // printer.pageLayout();
        // printer.pageLayout().fullRectPixels(resolution())
        // printer.pageLayout().paintRectPixels(resolution())
        // printer.pageLayout().margins()
        // printer.setPageSize
        // printer.setPaperName(QString("PaperName"));
        // printer.setPageOrder
        // printer.setResolution
        printer.setColorMode(QPrinter::Color);
        printer.setCollateCopies(true);
        printer.setFullPage(true);
        printer.setCopyCount(1); //setNumCopies
        // printer.setPaperSource
        printer.setDuplex(QPrinter::DuplexMode::DuplexNone); //setDoubleSidedPrinting
        // printer.setFontEmbeddingEnabled
        // printer.setPrinterSelectionOption
        // printer.setFromTo
        // printer.setPrintRange
        // printer.setPageMargins(QMarginsF, QPageLayout::Unit)
        // printer.setEngines(QPrintEngine *printEngine, QPaintEngine *paintEngine);

        // Создаем layout с миллиметрами
        QPageLayout pageLayout;
        pageLayout.setPageSize(QPageSize(QPageSize::A4)); // не работает
        pageLayout.setUnits(QPageLayout::Millimeter);
        // Устанавливаем мм как единицы измерения
        pageLayout.setMargins(QMarginsF(10, 10, 10, 10)); // Отступы в мм

        // Применяем layout к принтеру
        printer.setPageLayout(pageLayout);

        QPrintDialog printDialog(&printer, this);
        printDialog.setWindowTitle("Печать документа");
        if (printDialog.exec() == QDialog::Accepted) {
            QPainter painter;
            if (!painter.begin(&printer)) {
                QMessageBox::critical(this, "Ошибка", "Не удалось начать печать");
                return;
            }

            // Получаем DPI принтера для высококачественного рендеринга
            // int printerDpi = printer.logicalDpiX();

            const bool isLandscape = printer.pageLayout().orientation() ==
                                     QPageLayout::Landscape;

            for (int pageIndex = 0; pageIndex < m_document->pageCount(); ++pageIndex) {
                if (pageIndex > 0) {
                    printer.newPage();
                }

                QSizeF pdfPageSize = m_document->pageSize(pageIndex);
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
                QImage image = m_document->render(pageIndex, renderSize);

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

private:
    void setupUI() {
        centralWidget = new QWidget(this);
        setCentralWidget(centralWidget);

        mainLayout = new QVBoxLayout(centralWidget);

        // Создаем элементы управления
        buttonLayout = new QHBoxLayout();

        m_textEdit = new QTextEdit();
        m_textEdit->setPlaceholderText("Введите текст для создания PDF...");

        m_pdfView = new QPdfView();

        auto *createButton = new QPushButton("Создать PDF");
        auto *openButton = new QPushButton("Открыть PDF");
        auto *printButton = new QPushButton("Печать");

        buttonLayout->addWidget(createButton);
        buttonLayout->addWidget(openButton);
        buttonLayout->addWidget(printButton);

        mainLayout->addLayout(buttonLayout);
        mainLayout->addWidget(m_textEdit);
        mainLayout->addWidget(m_pdfView);

        // Сохраняем указатели на кнопки для соединений
        m_createButton = createButton;
        m_openButton = openButton;
        m_printButton = printButton;

        setWindowTitle("PDF приложение Qt5");
        resize(800, 600);
    }

    void setupConnections() {
        connect(m_createButton, &QPushButton::clicked, this, &PdfApp::createPdf_B);
        connect(m_openButton, &QPushButton::clicked, this, &PdfApp::openPdf);
        connect(m_printButton, &QPushButton::clicked, this, &PdfApp::printPdf);
    }

    void loadPdfForView(const QString &fileName) {
        if (m_document->load(fileName) != QPdfDocument::NoError) {
            QMessageBox::critical(this, "Ошибка",
                                  "Не удалось загрузить PDF файл: " + fileName);
        }
    }

    // Элементы интерфейса
    QTextEdit *m_textEdit{};
    QPdfView *m_pdfView{};
    QPushButton *m_createButton{};
    QPushButton *m_openButton{};
    QPushButton *m_printButton{};
    QPdfDocument *m_document;
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
    // if (type == QtFatalMsg)
    // abort();
}

int main(int argc, char *argv[]) {
    qInstallMessageHandler(myMessageHandler);
    QApplication app(argc, argv);
    PdfApp window;
    window.show();
    return QApplication::exec();
}

#include "main.moc"
