#ifndef LLMMOPTIMIZER_H
#define LLMMOPTIMIZER_H

#include <QObject>
#include <QString>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <deque>
#include "config.h"

struct PendingRequest
{
    enum Type { Optimize, Summary };
    Type type;
    QString context;
    QString current;
    QString content;
    int timeoutMs;
};

class LlmOptimizer : public QObject
{
    Q_OBJECT

public:
    explicit LlmOptimizer(QObject *parent = nullptr);
    ~LlmOptimizer();

    void setOptimizerConfig(const LlmOptimizerConfig &config);
    void setSummaryConfig(const LlmSummaryConfig &config);
    void optimizeText(const QString &text);
    void clearHistory();
    void generateSummary(const QString &content);

signals:
    void optimizationResult(const QString &original, const QString &optimized);
    void summaryResult(const QString &summary);
    void summaryComplete(); // 纪要请求完成（成功或失败）

private slots:
    void onReplyFinished(QNetworkReply *reply);
    void onRequestTimeout();
    void processNextRequest();

private:
    void sendRequest(const QString &context, const QString &current);
    void sendSummaryRequest(const QString &content);
    QString buildPrompt(const QString &context, const QString &current);
    QString buildSummaryPrompt(const QString &content);
    QString extractResponse(const QJsonDocument &jsonDoc);

    LlmOptimizerConfig m_optimizerConfig;
    LlmSummaryConfig m_summaryConfig;
    QNetworkAccessManager *m_networkManager;
    std::deque<QString> m_sentenceHistory;
    std::deque<PendingRequest> m_requestQueue;
    QNetworkReply *m_currentReply;
    QTimer *m_requestTimer;
    bool m_isProcessing;
    PendingRequest::Type m_currentRequestType;
    QString m_currentOriginalText;
};

#endif // LLMMOPTIMIZER_H
