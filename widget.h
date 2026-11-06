#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include <QSerialPort>
#include <QTimer>
#include <QDateTime>
#include <QByteArray>
#include <QVector>

QT_BEGIN_NAMESPACE
namespace Ui { class Widget; }
QT_END_NAMESPACE

class QCheckBox;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QRegularExpressionValidator;

class Widget : public QWidget
{
    Q_OBJECT

public:
    explicit Widget(QWidget *parent = nullptr);
    ~Widget();

private slots:
    void on_pushButton_openorcloseserial_clicked();
    void on_pushButton_SEND_clicked();
    void on_SerialData_readyToRead();
    void on_pushButton_12_clicked();
    void on_pushButton_hidepanel_clicked();
    void on_pushButton_hideRecord_clicked();
    void updateTime();

private:
    void initMultiRows();
    void bindExtraControls();
    void BaseSendData(QCheckBox *qcheck, QLineEdit *qline, QPushButton *qpush);

    void saveMultiConfig();
    void loadMultiConfig();
    void resetMultiConfig();
    void saveReceiveToFile();
    void setDownloadMode(bool on);
    void refreshPortList();

    QByteArray parseText(const QString &text, QString *err, bool hex);
    QByteArray buildSendData(QString *err);
    bool sendBytes(const QByteArray &data, bool interactive);
    bool doSend(bool interactive);
    static QString formatBytes(const QByteArray &data, bool hex);

    Ui::Widget *ui;

    QSerialPort *qserialport  = nullptr;
    QTimer      *sendTimer    = nullptr;
    QTimer      *currentTimer = nullptr;
    QTimer      *loopTimer    = nullptr;
    QTimer      *portTimer    = nullptr;

    QDateTime   currentDateAndTime;
    bool        IsOpen    = false;
    int         SendTotal = 0;
    int         ReadTotal = 0;
    QByteArray  sendbak;

    QVector<QCheckBox *> m_rowChecks;
    QVector<QLineEdit *> m_rowEdits;

    QCheckBox *m_checkRevTime = nullptr;
    QCheckBox *m_checkLoop    = nullptr;
    QSpinBox  *m_spinLoopMs   = nullptr;
    QRegularExpressionValidator *m_hexValidator = nullptr;
};

#endif // WIDGET_H
