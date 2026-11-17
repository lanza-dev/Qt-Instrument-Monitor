#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include <QList>


// This header only needs the class names because it stores pointers
// The header only stores pointers, so it needs the type names
// while the .cpp includes the full definitions to create the objects and call methods
// QMainWindow needs its full definition in the header because it’s the base class
// For instrument connection:
// I would move connection, commands, and response handling behind a device interface
// The UI would keep working with operations such as start, stop, and receive a reading
// For USB or EtherCAT, I would first find out which driver, SDK, or controller the device exposes and what timing it requires
class QTcpSocket;
class QTimer;
class QThread;

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private:
    Ui::MainWindow *ui;
    //void updateAcquisitionUi(bool connected, bool acquiring);
    QTcpSocket *socket;
    QTimer *pollTimer;
    QTimer *requestTimeout;

private slots:
    void connectInstrument();
    void disconnectInstrument();
    void startAcquisition();
    void stopAcquisition();

private:
    void updateAcquisitionUi();
    void requestMeasurement();
    void readMeasurements();
    void endSession(const QString &status);    
    void analyzeBatch();

    QList<double> readings;
    QThread *analysisThread = nullptr;
};
#endif // MAINWINDOW_H
