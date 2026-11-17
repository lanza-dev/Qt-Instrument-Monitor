#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QPushButton>
#include <QTcpSocket>
#include <QTimer>

#include <cmath>
#include "batchworker.h"
#include <QThread>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , socket(new QTcpSocket(this))
    , pollTimer(new QTimer(this))
    , requestTimeout(new QTimer(this))
{
    ui->setupUi(this);

    ui->analyzeBatchButton->setEnabled(false);
    connect(ui->analyzeBatchButton, &QPushButton::clicked,
            this, &MainWindow::analyzeBatch);

    pollTimer->setInterval(1000);
    requestTimeout->setInterval(3000);
    requestTimeout->setSingleShot(true);

    connect(ui->connectButton, &QPushButton::clicked,
            this, &MainWindow::connectInstrument);
    connect(ui->disconnectButton, &QPushButton::clicked,
            this, &MainWindow::disconnectInstrument);
    connect(ui->startAcquisitionButton, &QPushButton::clicked,
            this, &MainWindow::startAcquisition);
    connect(ui->stopAcquisitionButton, &QPushButton::clicked,
            this, &MainWindow::stopAcquisition);

    connect(socket, &QTcpSocket::connected, this, [this]
    {
        requestTimeout->stop();

        readings.clear();
        ui->analyzeBatchButton->setEnabled(false);
        ui->batchResultLabel->setText("Batch analysis: no results");

        ui->connectionStatusLabel->setText("Status: Connected");
        updateAcquisitionUi();
    });

    connect(socket, &QTcpSocket::disconnected, this, [this] {
        endSession("Status: Disconnected");
    });

    connect(socket, &QTcpSocket::errorOccurred, this,
            [this](QAbstractSocket::SocketError) {
                endSession(QStringLiteral("Socket error: %1").arg(socket->errorString()));
            });

    // Qt calls this handler when data arrives, so the UI can keep responding
    // readyRead lets Qt tell you when data is available
    connect(socket, &QTcpSocket::readyRead,
            this, &MainWindow::readMeasurements);
    connect(pollTimer, &QTimer::timeout,
            this, &MainWindow::requestMeasurement);
    connect(requestTimeout, &QTimer::timeout, this, [this] {
        endSession("Status: Instrument timed out");
    });

    updateAcquisitionUi();
}

MainWindow::~MainWindow()
{
    // Ask the worker to stop and wait for the thread before deleting the UI
    if (analysisThread != nullptr)
    {
        analysisThread->requestInterruption();
        analysisThread->quit();
        analysisThread->wait();
    }

    delete ui;
}

void MainWindow::connectInstrument()
{
    ui->connectionStatusLabel->setText("Status: Connecting...");
    requestTimeout->start();
    socket->connectToHost(QStringLiteral("127.0.0.1"), 5000);
    updateAcquisitionUi();
}

void MainWindow::disconnectInstrument()
{
    endSession("Status: Disconnected");
}

void MainWindow::startAcquisition()
{
    ui->connectionStatusLabel->setText("Status: Acquiring");
    pollTimer->start();
    updateAcquisitionUi();
    requestMeasurement();
}

void MainWindow::stopAcquisition()
{
    pollTimer->stop();
    ui->connectionStatusLabel->setText("Status: Connected");
    ui->concentrationLabel->setText("Concentration: -- ppm");
    updateAcquisitionUi();
}

void MainWindow::requestMeasurement()
{
    // Send a request only when connected and no reply is pending
    if (socket->state() != QAbstractSocket::ConnectedState || requestTimeout->isActive())
    {
        return;
    }

    requestTimeout->start();
    if (socket->write("READ\n") == -1)
    {
        endSession("Status: Could not send READ");
    }
}

void MainWindow::readMeasurements()
{
    // TCP can deliver part of a reply. Leave it buffered until a newline arrives
    while (socket->canReadLine())
    {
        const QByteArray line = socket->readLine().trimmed();

        if (!requestTimeout->isActive())
        {
            continue;
        }

        // A reply may arrive after Stop. Clear its timeout and skip the reading
        requestTimeout->stop();
        if (!pollTimer->isActive())
        {
            continue;
        }

        bool ok = false;
        const double ppm = line.startsWith("MEASUREMENT,")
                               ? line.mid(12).toDouble(&ok)
                               : 0.0;

        if (ok && std::isfinite(ppm) && ppm >= 0.0)
        {
            ui->concentrationLabel->setText(
                QStringLiteral("Concentration: %1 ppm").arg(ppm, 0, 'f', 1));

            // // Keep the latest 1,000 valid readings for batch analysis
            readings.append(ppm);
            if (readings.size() > 1000)
            {
                readings.removeFirst();
            }
            ui->analyzeBatchButton->setEnabled(analysisThread == nullptr);
        }
        else
        {
            ui->concentrationLabel->setText("Concentration: invalid reply");
        }        
    }
}

void MainWindow::endSession(const QString &status)
{
    pollTimer->stop();
    requestTimeout->stop();

    // Preserve the chosen status if abort() emits disconnected during cleanup.
    if (socket->state() != QAbstractSocket::UnconnectedState)
    {
        const bool previouslyBlocked = socket->blockSignals(true);
        socket->abort();
        socket->blockSignals(previouslyBlocked);
    }

    ui->connectionStatusLabel->setText(status);
    ui->concentrationLabel->setText("Concentration: -- ppm");
    updateAcquisitionUi();
}

void MainWindow::updateAcquisitionUi()
{
    const auto state = socket->state();
    const bool connected = state == QAbstractSocket::ConnectedState;
    const bool acquiring = pollTimer->isActive();

    ui->connectButton->setEnabled(state == QAbstractSocket::UnconnectedState);
    ui->disconnectButton->setEnabled(state != QAbstractSocket::UnconnectedState);
    ui->startAcquisitionButton->setEnabled(connected && !acquiring);
    ui->stopAcquisitionButton->setEnabled(connected && acquiring);
}

void MainWindow::analyzeBatch()
{
    if (readings.isEmpty() || analysisThread != nullptr)
    {
        return;
    }

    auto *thread = new QThread(this);
    // Gives the worker a snapshot so acquisition can keep adding readings
    auto *worker = new BatchWorker(readings);
    analysisThread = thread;
    worker->moveToThread(thread);

    ui->analyzeBatchButton->setEnabled(false);
    ui->batchResultLabel->setText("Analyzing batch...");

    connect(thread, &QThread::started,
            worker, &BatchWorker::analyze, Qt::QueuedConnection);

    // Queue the result back to the UI thread so it can safely update the label
    // The queued resultReady connection delivers the worker’s result to the UI thread
    // resultReady publishes the worker’s result.
    connect(worker, &BatchWorker::resultReady, this,
            [this](qsizetype count, double minimum,
                   double maximum, double average)
            {
                ui->batchResultLabel->setText(
                    QStringLiteral("%1 readings | Min %2 | Max %3 | Avg %4 ppm")
                        .arg(count)
                        .arg(minimum, 0, 'f', 1)
                        .arg(maximum, 0, 'f', 1)
                        .arg(average, 0, 'f', 1));
            }, Qt::QueuedConnection);

    // quit() is thread-safe. Let the worker stop its event loop directly.
    connect(worker, &BatchWorker::finished,
            thread, &QThread::quit, Qt::DirectConnection);
    connect(thread, &QThread::finished, worker, &QObject::deleteLater);
    connect(thread, &QThread::finished, this, [this, thread] {
        analysisThread = nullptr;
        ui->analyzeBatchButton->setEnabled(!readings.isEmpty());
        thread->deleteLater();
    });

    thread->start();
}