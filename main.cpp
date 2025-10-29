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
    void createPdf_B() {
        const QString text = m_textEdit->toPlainText();
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
        pdfWriter.setPageSize(QPageSize(QPageSize::A4));
        pdfWriter.setResolution(300);

        // Создаем QTextDocument
        QTextDocument document;
        document.setPlainText(text);

        // Настраиваем форматирование
        QFont font("Times", 14);
        document.setDefaultFont(font);

        QTextOption textOption;
        textOption.setAlignment(Qt::AlignLeft);
        textOption.setWrapMode(QTextOption::WordWrap);
        document.setDefaultTextOption(textOption);

        // Устанавливаем размер страницы
        QRectF pageRect = pdfWriter.pageLayout().paintRectPixels(pdfWriter.resolution());
        document.setTextWidth(pageRect.width()); // Важно: устанавливаем ширину текста

        QPainter painter;
        if (!painter.begin(&pdfWriter)) {
            QMessageBox::critical(this, "Ошибка", "Не удалось начать рисование");
            m_buffer.close();
            return;
        }

        // Ручное разбиение на страницы
        qreal height = document.documentLayout()->documentSize().height();
        int pages = qCeil(height / pageRect.height());

        for (int i = 0; i < pages; ++i) {
            if (i > 0) {
                pdfWriter.newPage();
            }

            // Смещаем область отрисовки для текущей страницы
            painter.save();
            painter.translate(0, -i * pageRect.height());

            // Устанавливаем область отсечения для текущей страницы
            QRectF clipRect(0, i * pageRect.height(), pageRect.width(), pageRect.height());
            painter.setClipRect(clipRect);

            // Рисуем содержимое документа
            document.drawContents(&painter);
            painter.restore();
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
        pageLayout.setMargins(QMarginsF(10, 10, 10, 10)); // Отступы в мм
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
        mainLayout->addLayout(pageNavigationLayout);
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

// ... остальной код без изменений (myMessageHandler, main и т.д.)
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
