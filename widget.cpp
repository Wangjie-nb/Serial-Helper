#include "widget.h"
#include "ui_widget.h"

#include <QDebug>
#include <QMessageBox>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QTextCursor>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QButtonGroup>
#include <QSpinBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QPushButton>
#include <QSerialPortInfo>

Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
{
    ui->setupUi(this);
    this->setLayout(ui->gridLayoutCHUANKOU);

    qserialport  = new QSerialPort(this);
    sendTimer    = new QTimer(this);
    currentTimer = new QTimer(this);
    loopTimer    = new QTimer(this);
    portTimer    = new QTimer(this);

    // ---------------- 时钟 ----------------
    currentTimer->setInterval(1000);
    connect(currentTimer, &QTimer::timeout, this, &Widget::updateTime);
    currentTimer->start();

    // ---------------- 串口列表 ----------------
    ui->comboBox_boautrate->setCurrentIndex(6);   // 默认 115200
    ui->comboBox_databit->setCurrentIndex(3);     // 默认 8
    refreshPortList();

    portTimer->setInterval(2000);
    connect(portTimer, &QTimer::timeout, this, [this]() {
        if (!IsOpen) refreshPortList();
    });
    portTimer->start();

    // ---------------- 接收 ----------------
    connect(qserialport, &QSerialPort::readyRead,
            this, &Widget::on_SerialData_readyToRead);

    // ---------------- 定时发送 ----------------
    connect(sendTimer, &QTimer::timeout, this, [this]() {
        doSend(false);
    });

    connect(ui->checkBox_timesend, &QCheckBox::stateChanged, this, [this](int state) {
        if (state != Qt::Checked)
            sendTimer->stop();
    });

    // ---------------- 多文本 10 行 ----------------
    initMultiRows();

    // ---------------- 其余控件按文字绑定 ----------------
    bindExtraControls();
}

Widget::~Widget()
{
    if (qserialport->isOpen())
        qserialport->close();
    delete ui;
}

// ============================================================
//  多文本 10 行：行首 checkbox = 该行「按 HEX 发送」开关
// ============================================================
void Widget::initMultiRows()
{
    m_rowChecks = { ui->checkBox_1,  ui->checkBox_2,  ui->checkBox_3,  ui->checkBox_4,
                    ui->checkBox_5,  ui->checkBox_6,  ui->checkBox_7,  ui->checkBox_8,
                    ui->checkBox_9,  ui->checkBox_10 };

    m_rowEdits  = { ui->lineEdit_1,  ui->lineEdit_2,  ui->lineEdit_3,  ui->lineEdit_4,
                    ui->lineEdit_5,  ui->lineEdit_6,  ui->lineEdit_7,  ui->lineEdit_8,
                    ui->lineEdit_9,  ui->lineEdit_10 };

    BaseSendData(ui->checkBox_1,  ui->lineEdit_1,  ui->pushButton_1);
    BaseSendData(ui->checkBox_2,  ui->lineEdit_2,  ui->pushButton_2);
    BaseSendData(ui->checkBox_3,  ui->lineEdit_3,  ui->pushButton_3);
    BaseSendData(ui->checkBox_4,  ui->lineEdit_4,  ui->pushButton_4);
    BaseSendData(ui->checkBox_5,  ui->lineEdit_5,  ui->pushButton_5);
    BaseSendData(ui->checkBox_6,  ui->lineEdit_6,  ui->pushButton_6);
    BaseSendData(ui->checkBox_7,  ui->lineEdit_7,  ui->pushButton_7);
    BaseSendData(ui->checkBox_8,  ui->lineEdit_8,  ui->pushButton_8);
    BaseSendData(ui->checkBox_9,  ui->lineEdit_9,  ui->pushButton_9);
    BaseSendData(ui->checkBox_10, ui->lineEdit_10, ui->pushButton_10);
}

// ============================================================
//  单行发送：模式 = 该行 checkbox
// ============================================================
void Widget::BaseSendData(QCheckBox *qcheck, QLineEdit *qline, QPushButton *qpush)
{
    connect(qpush, &QPushButton::clicked, this, [this, qcheck, qline]() {
        QString err;
        const QByteArray data = parseText(qline->text(), &err, qcheck->isChecked());
        if (!err.isEmpty()) {
            QMessageBox::warning(this, "提示", err);
            return;
        }
        sendBytes(data, true);
    });
}

// ============================================================
//  按文字绑定其余控件
// ============================================================
void Widget::bindExtraControls()
{
    QPushButton *runBtn = nullptr;
    QPushButton *dlBtn  = nullptr;

    for (QPushButton *b : findChildren<QPushButton *>()) {
        const QString t = b->text().trimmed();
        if (t == "保存") {
            connect(b, &QPushButton::clicked, this, &Widget::saveMultiConfig);
        } else if (t == "载入") {
            connect(b, &QPushButton::clicked, this, &Widget::loadMultiConfig);
        } else if (t == "重置") {
            connect(b, &QPushButton::clicked, this, &Widget::resetMultiConfig);
        } else if (t == "保存接收") {
            connect(b, &QPushButton::clicked, this, &Widget::saveReceiveToFile);
        } else if (t == "运行模式") {
            runBtn = b;
        } else if (t == "下载模式") {
            dlBtn = b;
        }
    }

    // 运行 / 下载：互斥
    if (runBtn && dlBtn) {
        runBtn->setCheckable(true);
        dlBtn->setCheckable(true);
        QButtonGroup *g = new QButtonGroup(this);
        g->setExclusive(true);
        g->addButton(runBtn);
        g->addButton(dlBtn);
        runBtn->setChecked(true);

        connect(runBtn, &QPushButton::clicked, this, [this]() { setDownloadMode(false); });
        connect(dlBtn,  &QPushButton::clicked, this, [this]() { setDownloadMode(true);  });
    }

    // 接收时间 / 循环输入 / 格式输入
    for (QCheckBox *c : findChildren<QCheckBox *>()) {
        const QString t = c->text().trimmed();
        if (t == "接收时间") {
            m_checkRevTime = c;
        } else if (t == "循环输入") {
            m_checkLoop = c;
        } else if (t == "格式输入") {
            static const QRegularExpression reHexInput("^[0-9A-Fa-f \\,]*$");
            m_hexValidator = new QRegularExpressionValidator(reHexInput, this);
            connect(c, &QCheckBox::toggled, this, [this](bool on) {
                ui->lineEdit_content->setValidator(on ? m_hexValidator : nullptr);
            });
        }
    }

    // 循环间隔（界面里唯一的 QSpinBox）
    m_spinLoopMs = findChild<QSpinBox *>();

    if (m_checkLoop && m_spinLoopMs) {
        connect(m_checkLoop, &QCheckBox::toggled, this, [this](bool on) {
            if (!on) {
                loopTimer->stop();
                return;
            }
            const int ms = m_spinLoopMs->value();
            loopTimer->setInterval(ms > 0 ? ms : 100);
            loopTimer->start();
        });

        // 每周期：把所有“有内容”的行各发一次，模式由各行 checkbox 决定
        connect(loopTimer, &QTimer::timeout, this, [this]() {
            if (!qserialport->isOpen()) {
                loopTimer->stop();
                if (m_checkLoop) m_checkLoop->setChecked(false);
                return;
            }
            for (int i = 0; i < m_rowEdits.size(); ++i) {
                if (m_rowEdits.at(i)->text().trimmed().isEmpty())
                    continue;

                QString err;
                const QByteArray data = parseText(m_rowEdits.at(i)->text(),
                                                  &err,
                                                  m_rowChecks.at(i)->isChecked());
                if (err.isEmpty() && !data.isEmpty())
                    sendBytes(data, false);
            }
        });
    }
}

// ============================================================
//  多文本 保存 / 载入 / 重置（第一列存的就是 HEX 开关）
// ============================================================
void Widget::saveMultiConfig()
{
    const QString path = QFileDialog::getSaveFileName(
        this, "保存多文本", QDir::homePath() + "/multiSend.txt", "文本文件 (*.txt)");
    if (path.isEmpty()) return;

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "提示", "保存失败：" + f.errorString());
        return;
    }
    QTextStream ts(&f);
    for (int i = 0; i < m_rowChecks.size(); ++i) {
        ts << (m_rowChecks.at(i)->isChecked() ? '1' : '0')
           << '\t' << m_rowEdits.at(i)->text() << '\n';
    }
    ts.flush();
    ui->label_issend->setText("保存成功");
}

void Widget::loadMultiConfig()
{
    const QString path = QFileDialog::getOpenFileName(
        this, "载入多文本", QDir::homePath(), "文本文件 (*.txt)");
    if (path.isEmpty()) return;

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "提示", "载入失败：" + f.errorString());
        return;
    }
    QTextStream ts(&f);
    int i = 0;
    while (!ts.atEnd() && i < m_rowChecks.size()) {
        const QString line = ts.readLine();
        const QStringList parts = line.split('\t');
        m_rowChecks.at(i)->setChecked(parts.value(0) == "1");
        m_rowEdits.at(i)->setText(parts.value(1));
        ++i;
    }
    ui->label_issend->setText("载入成功");
}

void Widget::resetMultiConfig()
{
    for (int i = 0; i < m_rowChecks.size(); ++i) {
        m_rowChecks.at(i)->setChecked(false);
        m_rowEdits.at(i)->clear();
    }
    ui->label_issend->setText("已重置");
}

// ============================================================
//  保存接收
// ============================================================
void Widget::saveReceiveToFile()
{
    const QString path = QFileDialog::getSaveFileName(
        this, "保存接收内容", QDir::homePath() + "/receive.txt", "文本文件 (*.txt)");
    if (path.isEmpty()) return;

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "提示", "保存失败：" + f.errorString());
        return;
    }
    QTextStream ts(&f);
    ts << ui->textEditRev->toPlainText();
    ts.flush();
    ui->label_issend->setText("接收内容已保存");
}

// ============================================================
//  运行 / 下载模式
// ============================================================
void Widget::setDownloadMode(bool on)
{
    ui->groupBoxRecord->setVisible(!on);
    ui->pushButton_hideRecord->setEnabled(!on);
    ui->pushButton_hidepanel->setEnabled(!on);
    ui->label_issend->setText(on ? "下载模式" : "运行模式");
}

// ============================================================
//  串口列表刷新
// ============================================================
void Widget::refreshPortList()
{
    QStringList names;
    for (const QSerialPortInfo &info : QSerialPortInfo::availablePorts())
        names << info.portName();
    if (names.isEmpty())
        names << "(无串口)";

    QStringList existing;
    for (int i = 0; i < ui->comboBox_serialNum->count(); ++i)
        existing << ui->comboBox_serialNum->itemText(i);

    if (names == existing)
        return;

    const QString cur = ui->comboBox_serialNum->currentText();
    ui->comboBox_serialNum->clear();
    ui->comboBox_serialNum->addItems(names);
    const int idx = ui->comboBox_serialNum->findText(cur);
    if (idx >= 0)
        ui->comboBox_serialNum->setCurrentIndex(idx);
}

// ============================================================
//  打开 / 关闭串口
// ============================================================
void Widget::on_pushButton_openorcloseserial_clicked()
{
    if (IsOpen) {
        qserialport->close();
        // qDebug() << "serial" << qserialport->portName() << "closed";
        IsOpen = false;
        ui->pushButton_openorcloseserial->setText("打开串口");
        return;
    }

    qserialport->setPortName(ui->comboBox_serialNum->currentText());
    qserialport->setBaudRate(ui->comboBox_boautrate->currentText().toInt());

    const int db = ui->comboBox_databit->currentText().toInt();
    qserialport->setDataBits(QSerialPort::DataBits(db >= 5 && db <= 8 ? db : 8));

    const QString p = ui->comboBox_jiaoyan->currentText().trimmed();
    if (p.compare("Even",  Qt::CaseInsensitive) == 0)      qserialport->setParity(QSerialPort::EvenParity);
    else if (p.compare("Odd",   Qt::CaseInsensitive) == 0) qserialport->setParity(QSerialPort::OddParity);
    else if (p.compare("Mark",  Qt::CaseInsensitive) == 0) qserialport->setParity(QSerialPort::MarkParity);
    else if (p.compare("Space", Qt::CaseInsensitive) == 0) qserialport->setParity(QSerialPort::SpaceParity);
    else                                                   qserialport->setParity(QSerialPort::NoParity);

    const QString sb = ui->comboBox_stopbit->currentText().trimmed();
    if (sb.compare("Two", Qt::CaseInsensitive) == 0 || sb == "2")
        qserialport->setStopBits(QSerialPort::TwoStop);
    else if (sb.compare("OneAndHalf", Qt::CaseInsensitive) == 0 || sb == "1.5")
        qserialport->setStopBits(QSerialPort::OneAndHalfStop);
    else
        qserialport->setStopBits(QSerialPort::OneStop);

    const QString fc = ui->comboBox_filecon->currentText().trimmed();
    if (fc.compare("Hardware", Qt::CaseInsensitive) == 0)
        qserialport->setFlowControl(QSerialPort::HardwareControl);
    else if (fc.compare("Software", Qt::CaseInsensitive) == 0)
        qserialport->setFlowControl(QSerialPort::SoftwareControl);
    else
        qserialport->setFlowControl(QSerialPort::NoFlowControl);

    if (qserialport->open(QIODevice::ReadWrite)) {
        IsOpen = true;
        ui->pushButton_openorcloseserial->setText("关闭串口");
        // qDebug() << "serial success opened";
    } else {
        IsOpen = false;
        QMessageBox::warning(this, "提示", "打开串口失败：" + qserialport->errorString());
        // qDebug() << "打开失败！原因：" << qserialport->errorString();
    }
}

// ============================================================
//  文本 -> 字节
// ============================================================
QByteArray Widget::parseText(const QString &text, QString *err, bool hex)
{
    if (err) err->clear();

    QByteArray data;

    if (hex) {
        QString s = text;
        s.remove(QRegularExpression("0[xX]"));
        s.remove(QRegularExpression("[\\s,;:]"));

        if (s.isEmpty()) {
            if (err) *err = "HEX 内容为空";
            return QByteArray();
        }
        static const QRegularExpression reHexOnly("^[0-9A-Fa-f]+$");
        if (!reHexOnly.match(s).hasMatch()) {
            if (err) *err = "包含非十六进制字符（只能是 0-9 A-F）";
            return QByteArray();
        }
        if (s.size() % 2 != 0) {
            if (err) *err = "十六进制位数必须是偶数，例如 AA BB 0D";
            return QByteArray();
        }
        data = QByteArray::fromHex(s.toLatin1());
    } else {
        data = text.toUtf8();
    }

    if (ui->checkBox_sendnewline->isChecked())
        data.append("\r\n");

    return data;
}

// ============================================================
//  主输入框（跟底部「HEX发送」走）
// ============================================================
QByteArray Widget::buildSendData(QString *err)
{
    return parseText(ui->lineEdit_content->text(), err,
                     ui->checkBox_HEXsend->isChecked());
}

QString Widget::formatBytes(const QByteArray &data, bool hex)
{
    if (hex)
        return QString::fromLatin1(data.toHex(' ').toUpper());
    return QString::fromUtf8(data);
}

// ============================================================
//  写串口 + 计数 + 记录去重
// ============================================================
bool Widget::sendBytes(const QByteArray &data, bool interactive)
{
    if (!qserialport->isOpen()) {
        if (interactive)
            QMessageBox::warning(this, "提示", "串口未打开");
        return false;
    }
    if (data.isEmpty())
        return false;

    const int n = qserialport->write(data);
    if (n <= 0) {
        ui->label_issend->setText("Send Error");
        // qDebug() << "sendError!";
        return false;
    }

    SendTotal += n;
    ui->label_sendnum->setNum(SendTotal);

    if (sendbak != data) {
        sendbak = data;
        ui->textEditRecord->append(
            formatBytes(data, ui->checkBox_HEXsend->isChecked()));
    }

    ui->label_issend->setText("Send Ok！");
    // qDebug() << "Sent:" << data;
    return true;
}

bool Widget::doSend(bool interactive)
{
    QString err;
    const QByteArray data = buildSendData(&err);
    if (!err.isEmpty()) {
        if (interactive) {
            QMessageBox::warning(this, "提示", err);
        } else {
            ui->checkBox_timesend->setChecked(false);
            sendTimer->stop();
        }
        return false;
    }
    return sendBytes(data, interactive);
}

// ============================================================
//  发送按钮
// ============================================================
void Widget::on_pushButton_SEND_clicked()
{
    if (!doSend(true))
        return;

    if (ui->checkBox_timesend->isChecked()) {
        bool ok = false;
        const int interval = ui->lineEdit_TimerSendContent->text().toInt(&ok);
        if (!ok || interval <= 0) {
            QMessageBox::warning(this, "提示", "定时发送间隔必须是大于 0 的整数");
            ui->checkBox_timesend->setChecked(false);
            sendTimer->stop();
            return;
        }
        sendTimer->setInterval(interval);
        sendTimer->start();
    } else {
        sendTimer->stop();
    }
}

// ============================================================
//  接收
// ============================================================
void Widget::on_SerialData_readyToRead()
{
    const QByteArray bytes = qserialport->readAll();
    if (bytes.isEmpty())
        return;

    QString text = formatBytes(bytes, ui->checkBox_HEXdisplay->isChecked());

    // 时间戳：Qt 的 toString 不支持毫秒占位符，手动拼
    if (m_checkRevTime && m_checkRevTime->isChecked()) {
        const QDateTime now = QDateTime::currentDateTime();
        const QString ts = now.toString("hh:mm:ss") + "." +
                           QString("%1").arg(now.time().msec(), 3, 10, QChar('0'));
        text.prepend("[" + ts + "] ");
    }

    if (ui->checkBox_autowrap->isChecked()) {
        ui->textEditRev->append(text);
    } else {
        ui->textEditRev->moveCursor(QTextCursor::End);
        ui->textEditRev->insertPlainText(text);
    }

    ReadTotal += bytes.size();
    ui->label_ReceiveNum->setNum(ReadTotal);
}

// ============================================================
//  清空接收
// ============================================================
void Widget::on_pushButton_12_clicked()
{
    ui->textEditRev->clear();
    ReadTotal = 0;
    ui->label_ReceiveNum->setNum(ReadTotal);
}

// ============================================================
//  隐藏 / 显示 面板
// ============================================================
void Widget::on_pushButton_hidepanel_clicked()
{
    const bool visible = !ui->groupBoxparams->isVisible();
    ui->groupBoxparams->setVisible(visible);
    ui->pushButton_hidepanel->setText(visible ? "隐藏面板" : "显示面板");
}

// ============================================================
//  隐藏 / 显示 历史
// ============================================================
void Widget::on_pushButton_hideRecord_clicked()
{
    const bool visible = !ui->groupBoxRecord->isVisible();
    ui->groupBoxRecord->setVisible(visible);
    ui->pushButton_hideRecord->setText(visible ? "隐藏历史" : "显示历史");
}

// ============================================================
//  时间刷新
// ============================================================
void Widget::updateTime()
{
    currentDateAndTime = QDateTime::currentDateTime();
    ui->label_NowTime->setText(currentDateAndTime.toString("yyyy-MM-dd hh:mm:ss"));
}
