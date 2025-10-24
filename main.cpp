#include <QApplication>
#include <QMainWindow>
#include <QPainter>
#include <QRect>
#include <QVBoxLayout>
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
#include <QDebug>


class PdfApp final : public QMainWindow {
    Q_OBJECT

public:
    PdfApp(QWidget *parent = nullptr) : QMainWindow(parent) {
        setupUI();
        setupConnections();
        m_document = new QPdfDocument(this);
        m_pdfView->setDocument(m_document);
    }

private slots:
    void createPdf() {
        // Получаем текст из текстового редактора
        QString text = m_textEdit->toPlainText();
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
        QPageLayout pageLayout;
        pageLayout.setPageSize(QPageSize(QPageSize::A5));
        printer.setPageLayout(pageLayout);

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
        printer.setPaperName(QString("PaperName"));
        // printer.setPageOrder
        // printer.setResolution
        printer.setColorMode(QPrinter::Color);
        printer.setCollateCopies(true);
        printer.setFullPage(true);
        printer.setCopyCount(2); //setNumCopies
        // printer.setPaperSource
        printer.setDuplex(QPrinter::DuplexMode::DuplexNone); //setDoubleSidedPrinting
        // printer.setFontEmbeddingEnabled
        // printer.setPrinterSelectionOption
        // printer.setFromTo
        // printer.setPrintRange
        // printer.setPageMargins(QMarginsF, QPageLayout::Unit)
        // printer.setEngines(QPrintEngine *printEngine, QPaintEngine *paintEngine);


        QPrintDialog printDialog(&printer, this);
        printDialog.setWindowTitle("Печать документа");
        if (printDialog.exec() == QDialog::Accepted) {
            QPainter painter;
            if (!painter.begin(&printer)) {
                QMessageBox::critical(this, "Ошибка", "Не удалось начать печать");
                return;
            }

            // Получаем DPI принтера для высококачественного рендеринга
            int printerDpi = printer.logicalDpiX();

            bool isLandscape = printer.pageLayout().orientation() == QPageLayout::Landscape;

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
                qreal scale = qMin(scaleX, scaleY);

                // Размер для рендеринга с учетом масштаба и высокого DPI для качества
                QSize renderSize(pdfPageSize.width() * scale * 2,
                                 pdfPageSize.height() * scale * 2);

                // Рендерим страницу PDF
                QImage image = m_document->render(pageIndex, renderSize);

                if (!image.isNull()) {
                    // Позиционирование с центрированием
                    QPointF imagePos(
                        (printerRect.width() - renderSize.width() / 2) / 2,
                        (printerRect.height() - renderSize.height() / 2) / 2
                    );

                    // Рисуем изображение с учетом масштаба
                    painter.drawImage(
                        QRectF(imagePos, QSizeF(renderSize.width() / 2,
                                                renderSize.height() / 2)),
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
        QString fileName = QFileDialog::getOpenFileName(
            this, "Открыть PDF", "", "PDF Files (*.pdf)");
        if (!fileName.isEmpty()) {
            loadPdfForView(fileName);
        }
    }

private:
    void setupUI() {
        const auto centralWidget = new QWidget(this);
        setCentralWidget(centralWidget);

        auto *mainLayout = new QVBoxLayout(centralWidget);

        // Создаем элементы управления
        auto *buttonLayout = new QHBoxLayout();

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
        connect(m_createButton, &QPushButton::clicked, this, &PdfApp::createPdf);
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
    QTextEdit *m_textEdit;
    QPdfView *m_pdfView;
    QPushButton *m_createButton;
    QPushButton *m_openButton;
    QPushButton *m_printButton;
    QPdfDocument *m_document;
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    PdfApp window;
    window.show();

    return QApplication::exec();
}

#include "main.moc"
