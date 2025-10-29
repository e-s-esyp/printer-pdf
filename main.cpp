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
#include <QDebug>

#define QD qDebug()
#define DUMP(var)  #var << ": " << (var) << " "
#define DUMPH(var) #var << ": " << QString::number(var, 16) << " "
#define DUMPS(var) #var << ": " << QString::fromLatin1(var, 4) << " "


class DetailedEventTracker : public QObject {
    const char *eventTypeToString(int type) {
        static const char *eventNames[] = {
            /* 0 */ "Invalid event",
            /* 1 */ "Timer event",
            /* 2 */ "Mouse button pressed",
            /* 3 */ "Mouse button released",
            /* 4 */ "Mouse button double click",
            /* 5 */ "Mouse move",
            /* 6 */ "Key pressed",
            /* 7 */ "Key released",
            /* 8 */ "Keyboard focus received",
            /* 9 */ "Keyboard focus lost",
            /* 10 */ "Mouse enters widget",
            /* 11 */ "Mouse leaves widget",
            /* 12 */ "Paint widget",
            /* 13 */ "Move widget",
            /* 14 */ "Resize widget",
            /* 15 */ "After widget creation",
            /* 16 */ "During widget destruction",
            /* 17 */ "Widget is shown",
            /* 18 */ "Widget is hidden",
            /* 19 */ "Request to close widget",
            /* 20 */ "Request to quit application",
            /* 21 */ "Widget has been reparented",
            /* 22 */ "Object has changed threads",
            /* 23 */ "Keyboard focus is about to be lost",
            /* 24 */ "Window was activated",
            /* 25 */ "Window was deactivated",
            /* 26 */ "Widget is shown to parent",
            /* 27 */ "Widget is hidden to parent",
            /* 28-30 */ nullptr, nullptr, nullptr,
            /* 31 */ "Wheel event",
            /* 32 */ nullptr,
            /* 33 */ "Window title changed",
            /* 34 */ "Icon changed",
            /* 35 */ "Application icon changed",
            /* 36 */ "Application font changed",
            /* 37 */ "Application layout direction changed",
            /* 38 */ "Application palette changed",
            /* 39 */ "Widget palette changed",
            /* 40 */ "Internal clipboard event",
            /* 41 */ nullptr,
            /* 42 */ "Reserved for speech input",
            /* 43 */ "Meta call event",
            /* 44-49 */ nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
            /* 50 */ "Socket activation",
            /* 51 */ "Shortcut override request",
            /* 52 */ "Deferred delete event",
            /* 53-59 */ nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
            /* 60 */ "Drag moves into widget",
            /* 61 */ "Drag moves in widget",
            /* 62 */ "Drag leaves or is cancelled",
            /* 63 */ "Actual drop",
            /* 64 */ "Drag accepted/rejected",
            /* 65-67 */ nullptr, nullptr, nullptr,
            /* 68 */ "New child widget",
            /* 69 */ "Polished child widget",
            /* 70 */ nullptr,
            /* 71 */ "Deleted child widget",
            /* 72 */ nullptr,
            /* 73 */ "Widget's window should be mapped",
            /* 74 */ "Widget should be polished",
            /* 75 */ "Widget is polished",
            /* 76 */ "Widget should be relayouted",
            /* 77 */ "Widget should be repainted",
            /* 78 */ "Request update() later",
            /* 79 */ "ActiveX embedding",
            /* 80 */ "ActiveX activation",
            /* 81 */ "ActiveX deactivation",
            /* 82 */ "Context popup menu",
            /* 83 */ "Input method",
            /* 84-86 */ nullptr, nullptr, nullptr,
            /* 87 */ "Wacom tablet event",
            /* 88 */ "The system locale changed",
            /* 89 */ "The application language changed",
            /* 90 */ "The layout direction changed",
            /* 91 */ "Internal style event",
            /* 92 */ "Tablet press",
            /* 93 */ "Tablet release",
            /* 94 */ "CE (Ok) button pressed",
            /* 95 */ "CE (?) button pressed",
            /* 96 */ "Proxy icon dragged",
            /* 97 */ "Font has changed",
            /* 98 */ "Enabled state has changed",
            /* 99 */ "Window activation has changed",
            /* 100 */ "Style has changed",
            /* 101 */ "Icon text has changed. Deprecated",
            /* 102 */ "Modified state has changed",
            /* 103 */ "Window is about to be blocked modally",
            /* 104 */ "Windows modal blocking has ended",
            /* 105 */ "Window state change",
            /* 106 */ "Readonly state has changed",
            /* 107-108 */ nullptr, nullptr,
            /* 109 */ "Mouse tracking state has changed",
            /* 110 */ "ToolTip",
            /* 111 */ "WhatsThis",
            /* 112 */ "StatusTip",
            /* 113 */ "Action changed",
            /* 114 */ "Action added",
            /* 115 */ "Action removed",
            /* 116 */ "File open request",
            /* 117 */ "Shortcut triggered",
            /* 118 */ "WhatsThis clicked",
            /* 119 */ nullptr,
            /* 120 */ "Toolbar visibility toggled",
            /* 121 */ "Application activate (deprecated)",
            /* 122 */ "Application deactivate (deprecated)",
            /* 123 */ "Query what's this widget help",
            /* 124 */ "Enter what's this mode",
            /* 125 */ "Leave what's this mode",
            /* 126 */ "Child widget has had its z-order changed",
            /* 127 */ "Mouse cursor enters a hover widget",
            /* 128 */ "Mouse cursor leaves a hover widget",
            /* 129 */ "Mouse cursor move inside a hover widget",
            /* 130 */ nullptr,
            /* 131 */ "Sent just before the parent change is done",
            /* 132 */ "Win event activation"
        };

        const int arraySize = sizeof(eventNames) / sizeof(eventNames[0]);

        if (type >= 0 && type < arraySize && eventNames[type] != nullptr) {
            return eventNames[type];
        } else {
            static char unknownBuffer[64];
            snprintf(unknownBuffer, sizeof(unknownBuffer), "Unknown event (%d)", type);
            return unknownBuffer;
        }
    }

protected:
    bool eventFilter(QObject *obj, QEvent *event) override {
        QString className = obj ? obj->metaObject()->className() : "null";
        QString objName = obj ? obj->objectName() : "unnamed";
        const char *eventDescription = eventTypeToString(event->type());
        qDebug().noquote()
                << "Time:" << QTime::currentTime().toString("hh:mm:ss.zzz")
                << "Event:" << eventDescription
                << "Type:" << QString("%1").arg(event->type(), 3)
                << "Class:" << className.left(20).leftJustified(20, ' ')
                << "Object:" << objName.left(15).leftJustified(15, ' ')
                << "Accepted:" << event->isAccepted();

        return false;
    }
};

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
    QBuffer m_buffer{};
    QByteArray m_pdfData;
    QPdfDocument m_document;

public:
    void _debugPdfView() const {
        qDebug() << "=== Диагностика QPdfView ===";
        qDebug() << "Документ установлен:" << (m_pdfView->document() != nullptr);
        qDebug() << "Размер view:" << m_pdfView->size();
        qDebug() << "Видимость:" << m_pdfView->isVisible();
        qDebug() << "Режим масштабирования:" << m_pdfView->zoomMode();
        qDebug() << "Масштаб:" << m_pdfView->zoomFactor();
        qDebug() << "Режим страниц:" << m_pdfView->pageMode();

        qDebug() << "Количество страниц в документе:" << m_document.pageCount();
        qDebug() << "Статус документа:" << m_document.status();

        // Проверяем родительскую иерархию
        QWidget *parent = m_pdfView;
        while (parent) {
            qDebug() << "Виджет:" << parent->objectName()
                    << "Размер:" << parent->size()
                    << "Видим:" << parent->isVisible();
            parent = parent->parentWidget();
        }
        qDebug() << "=== Конец диагностики ===";
    }

    void _updatePdfView() const {
        // 1. Перепривязываем документ
        // m_pdfView->setDocument(nullptr);
        // m_pdfView->setDocument(&m_document);

        // 2. Настраиваем отображение
        m_pdfView->setPageMode(QPdfView::PageMode::MultiPage);
        // m_pdfView->setZoomMode(QPdfView::ZoomMode::FitInView);

        // 3. Принудительное обновление
        m_pdfView->update();

        // 4. Если в layout, возможно нужно обновить layout
        if (m_pdfView->parentWidget()) {
            m_pdfView->parentWidget()->updateGeometry();
        }

        // 5. Отладочная информация
        _debugPdfView();
    }

    static void _saveImage(QPdfDocument *document) {
        // const auto document = m_pdfView->document();
        if (document->pageCount() > 0) {
            // Получаем первую страницу (индекс 0) в виде QImage
            QImage image = document->render(0, QSize(300, 400));
            if (!image.isNull()) {
                qDebug() << "Первая страница успешно отрендерена. Размер:"
                        << image.size();
                auto r = image.save("debug_page.png");
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
        m_pdfData.clear();
        if (m_buffer.isOpen()) {
            QD;
            m_buffer.close();
        }
        if (!m_buffer.open(QIODevice::WriteOnly)) {
            QMessageBox::critical(this, "Ошибка", "Не удалось открыть буфер для записи");
            return;
        }
        QD;
        // Создаем PDF writer
        QPdfWriter pdfWriter(&m_buffer);
        pdfWriter.setPageSize(QPageSize(QPageSize::A4));
        pdfWriter.setResolution(600); // High resolution
        // Создаем painter и рисуем содержимое
        QPainter painter;
        if (!painter.begin(&pdfWriter)) {
            QMessageBox::critical(this, "Ошибка", "Не удалось начать рисование");
            m_buffer.close();
            return;
        }
        // Устанавливаем шрифт и рисуем текст
        const QFont font("Times", 14);
        painter.setFont(font);
        const QRectF pageRect =
                pdfWriter.pageLayout().paintRectPixels(pdfWriter.resolution());
        painter.drawText(pageRect, Qt::AlignLeft | Qt::TextWordWrap, text);
        painter.end();
        m_buffer.close();
        // Загружаем данные в QPdfDocument.
        m_document.close();
        // Открываем буфер для чтения
        if (!m_buffer.open(QIODevice::ReadOnly)) {
            QMessageBox::critical(this, "Ошибка", "Не удалось открыть буфер для чтения");
            return;
        }
        // Создаем документ, если еще не создан
        QD << m_document.status();
        m_buffer.seek(0);
        m_document.load(&m_buffer);
        m_buffer.close();
        QD << "done";
    }

    static void updateDocument(QPdfDocument *document) {
        QD;
        // QCoreApplication::processEvents();
    }

    void printPdf() {
        if (m_document.pageCount() == 0) {
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
        // printer.setPageSize(QPageSize(QSizeF(297, 210), QPageSize::Unit::Millimeter));
        //297x210
        // printer.setPageMargins
        // printer.setPageOrientation(QPageLayout::Orientation::Landscape); //setOrientation
        // printer.setPageOrientation(QPageLayout::Orientation::Portrait);//не работает
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
        // printer.setPageSize(QPageSize(QPageSize::A5));
        QPageLayout pageLayout;
        // Устанавливаем мм как единицы измерения
        pageLayout.setPageSize(QPageSize(QPageSize::A5)); // работает странно, не убирать
        QD << pageLayout.pageSize();
        pageLayout.setUnits(QPageLayout::Millimeter);
        pageLayout.setMargins(QMarginsF(10, 10, 10, 10)); // Отступы в мм
        pageLayout.setOrientation(QPageLayout::Orientation::Portrait);
        // pageLayout.setMode(QPageLayout::StandardMode);
        // Применяем layout к принтеру
        printer.setPageLayout(pageLayout);
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

            // Получаем DPI принтера для высококачественного рендеринга
            // int printerDpi = printer.logicalDpiX();

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

private:
    void setupUI() {
        m_buffer.setBuffer(&m_pdfData);

        centralWidget = new QWidget(this);
        setCentralWidget(centralWidget);

        mainLayout = new QVBoxLayout(centralWidget);

        // Создаем элементы управления
        buttonLayout = new QHBoxLayout();

        m_textEdit = new QTextEdit();
        m_textEdit->setPlaceholderText("Введите текст для создания PDF...");

        m_pdfView = new QPdfView();
        m_pdfView->setDocument(&m_document);

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
        connect(&m_document, &QPdfDocument::statusChanged, this,
                [this](const QPdfDocument::Status status) {
                    QD << DUMP(status);
                    if (m_document.status() == QPdfDocument::Ready) {
                        DetailedEventTracker tracker;
                        qApp->installEventFilter(&tracker);
                        // m_pdfView->update();
                        QCoreApplication::processEvents();
                        QD << "----------------------";
                        QTimer::singleShot(1000, [this] {
                            QCoreApplication::processEvents();
                        });
                        qApp->removeEventFilter(&tracker);
                    } else if (m_document.status() == QPdfDocument::Error) {
                        qDebug() << "Ошибка при загрузке документа:" << m_document.
                                error();
                    }
                });
    }

    void loadPdfForView(const QString &fileName) {
        if (m_document.load(fileName) != QPdfDocument::NoError) {
            QMessageBox::critical(this, "Ошибка",
                                  "Не удалось загрузить PDF файл: " + fileName);
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
    // if (type == QtFatalMsg)
    // abort();
}

int main(int argc, char *argv[]) {
    // qInstallMessageHandler(myMessageHandler);
    QApplication app(argc, argv);
    PdfApp window;
    window.show();
    return QApplication::exec();
}

#include "main.moc"
