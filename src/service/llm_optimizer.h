#ifndef LLMMOPTIMIZER_H
#define LLMMOPTIMIZER_H

#include <QObject>
#include <QString>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <deque>
#include "config.h"

class LlmOptimizer : public QObject
{
    Q_OBJECT

public:
    explicit LlmOptimizer(QObject *parent = nullptr);
    ~LlmOptimizer();

    void setConfig(const LlmOptimizerConfig &config);
    void optimizeText(const QString &text);
    void clearHistory();

signals:
    void optimizationResult(const QString &original, const QString &optimized);
    void optimizationError(const QString &error);

private slots:
    void onReplyFinished(QNetworkReply *reply);

private:
    void sendRequest(const QString &context, const QString &current);
    QString buildPrompt(const QString &context, const QString &current);
    QString extractResponse(const QJsonDocument &jsonDoc);

    LlmOptimizerConfig m_config;
    QNetworkAccessManager *m_networkManager;
    std::deque<QString> m_sentenceHistory;
};

#endif // LLMMOPTIMIZER_H
