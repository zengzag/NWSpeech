#ifndef FLOATING_WINDOW_H
#define FLOATING_WINDOW_H

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QMouseEvent>
#include "config.h"

class FloatingWindow : public QWidget
{
    Q_OBJECT

public:
    explicit FloatingWindow(QWidget *parent = nullptr);
    ~FloatingWindow();

    void setConfig(const SubtitleWindowConfig &config);
    void setText(const QString &text, AudioSourceTag source = AudioSourceTag::kSystem);
    void clearText();
    void updateLabelVisibility(AudioSource source);

signals:
    void windowHidden();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    void applyConfig();
    void setupUI();

    QLabel *m_systemLabel;
    QLabel *m_micLabel;
    QPoint m_dragPosition;
    SubtitleWindowConfig m_config;
};

#endif // FLOATING_WINDOW_H
