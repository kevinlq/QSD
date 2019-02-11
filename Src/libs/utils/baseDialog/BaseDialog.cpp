#include "../utils_global.h"

#include "BaseDialog.h"

#include "../QFrameLessHelper/FrameLessWidgetHelper.h"

/** Qt include*/
#include <QHBoxLayout>
#include <QPushButton>

BaseDialog::BaseDialog(QWidget *parent) : QWidget(parent)
{
    setWindowFlags (Qt::FramelessWindowHint);

    m_pBtnClose = new QPushButton(this);
    m_pBtnClose->setText (tr("pbnClose"));
    m_pBtnClose->setMinimumSize (80,26);

    QHBoxLayout *pLayout = new QHBoxLayout;
    pLayout->addStretch ();
    pLayout->addWidget (m_pBtnClose);

    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    pMainLayout->addLayout (pLayout);
    pMainLayout->addStretch ();

    setLayout (pMainLayout);

    FramelessWidgetHelper *pFrame = new FramelessWidgetHelper(this);
    pFrame->addTitleObject (m_pBtnClose);
    pFrame->setTitleHeight (30);

    this->resize (640,480);

    connect(m_pBtnClose, &QPushButton::clicked, this, &BaseDialog::close);
}

BaseDialog::~BaseDialog()
{
}

#include "moc_BaseDialog.cpp"
