#include "ui/floating_window.h"
#include <QApplication>
#include <QScreen>
#include <QCloseEvent>
#include <QTimer>

FloatingWindow::FloatingWindow(QWidget *parent)
    : QWidget(parent)
{
    m_config.window_width = 500;
    m_config.window_height = 80;
    m_config.font_size = 16;
    m_config.text_color = "#FFFFFF";
    m_config.background_color = "#000000";
    m_config.background_opacity = 180;
    setupUI();
}

FloatingWindow::~FloatingWindow()
{
}

void FloatingWindow::setConfig(const SubtitleWindowConfig &config)
{
    m_config = config;
    applyConfig();
}

void FloatingWindow::applyConfig()
{
    setFixedSize(m_config.window_width, m_config.window_height);

    bool ok;
    QString bgColor = QString::fromStdString(m_config.background_color);
    int r = bgColor.mid(1, 2).toInt(&ok, 16);
    int g = bgColor.mid(3, 2).toInt(&ok, 16);
    int b = bgColor.mid(5, 2).toInt(&ok, 16);

    QString baseStyle = QString(R"(
        QLabel {
            color: %1;
            font-size: %2px;
            padding: 5px;
            background-color: rgba(%3, %4, %5, %6);
            border-radius: 6px;
        }
    )").arg(
        QString::fromStdString(m_config.text_color),
        QString::number(m_config.font_size)
    ).arg(r).arg(g).arg(b).arg(m_config.background_opacity);

    m_systemLabel->setStyleSheet(baseStyle);
    m_micLabel->setStyleSheet(baseStyle);
}

void FloatingWindow::setupUI()
{
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(4);

    m_systemLabel = new QLabel("", this);
    m_systemLabel->setWordWrap(true);
    m_systemLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_systemLabel->setStyleSheet("color: #007AFF;");
    layout->addWidget(m_systemLabel);

    m_micLabel = new QLabel("", this);
    m_micLabel->setWordWrap(true);
    m_micLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_micLabel->setStyleSheet("color: #30D158;");
    layout->addWidget(m_micLabel);

    applyConfig();

    QScreen *screen = QApplication::primaryScreen();
    if (screen) {
        QRect screenGeometry = screen->availableGeometry();
        move(screenGeometry.width() - width() - 20, screenGeometry.height() - height() - 100);
    }
}

void FloatingWindow::setText(const QString &text, AudioSourceTag source)
{
    if (source == AudioSourceTag::kSystem) {
        QString displayText = text.isEmpty() ? "" : "[系统] " + text;
        if (m_systemLabel->text() != displayText) {
            m_systemLabel->setText(displayText);
            m_systemLabel->setVisible(!text.isEmpty());
            m_systemLabel->updateGeometry();
            m_systemLabel->repaint();
        }
    } else if (source == AudioSourceTag::kMicrophone) {
        QString displayText = text.isEmpty() ? "" : "[麦克风] " + text;
        if (m_micLabel->text() != displayText) {
            m_micLabel->setText(displayText);
            m_micLabel->setVisible(!text.isEmpty());
            m_micLabel->updateGeometry();
            m_micLabel->repaint();
        }
    }
    updateGeometry();
    repaint();
    QApplication::processEvents(QEventLoop::AllEvents, 5);
}

void FloatingWindow::clearText()
{
    m_systemLabel->clear();
    m_micLabel->clear();
}

void FloatingWindow::updateLabelVisibility(AudioSource source)
{
    if (source == AudioSource::kSystemAudio) {
        m_systemLabel->setVisible(!m_systemLabel->text().isEmpty());
        m_micLabel->setVisible(false);
        m_micLabel->clear();
    } else if (source == AudioSource::kMicrophone) {
        m_micLabel->setVisible(!m_micLabel->text().isEmpty());
        m_systemLabel->setVisible(false);
        m_systemLabel->clear();
    } else if (source == AudioSource::kBoth) {
        m_systemLabel->setVisible(!m_systemLabel->text().isEmpty());
        m_micLabel->setVisible(!m_micLabel->text().isEmpty());
    }
    update();
}

void FloatingWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
    }
}

void FloatingWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton) {
        move(event->globalPosition().toPoint() - m_dragPosition);
        event->accept();
    }
}

void FloatingWindow::closeEvent(QCloseEvent *event)
{
    emit windowHidden();
    event->accept();
}
