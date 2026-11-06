#ifndef SENDROW_H
#define SENDROW_H


#include <QWidget>
#include "widget.h"
class SendRow : public QWidget
{
    Q_OBJECT
public:
    explicit SendRow(QWidget *parent = nullptr);
    bool    isChecked() const { return m_check->isChecked(); }
        QString text() const      { return m_edit->text(); }
        void    setText(const QString &s) { m_edit->setText(s); }
signals:


private:
    QCheckBox *m_check =nullptr;
    QLineEdit   *m_edit  = nullptr;
    QPushButton *m_btn   = nullptr;
};

#endif // SENDROW_H
