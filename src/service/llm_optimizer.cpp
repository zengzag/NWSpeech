#include "llm_optimizer.h"
#include <QNetworkRequest>
#include <QSslConfiguration>
#include <QDebug>

LlmOptimizer::LlmOptimizer(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_currentReply(nullptr)
    , m_requestTimer(new QTimer(this))
    , m_isProcessing(false)
{
    m_requestTimer->setSingleShot(true);
    connect(m_requestTimer, &QTimer::timeout, this, &LlmOptimizer::onRequestTimeout);
}

LlmOptimizer::~LlmOptimizer()
{
    if (m_currentReply) {
        m_currentReply->abort();
        m_currentReply->deleteLater();
    }
}

void LlmOptimizer::setOptimizerConfig(const LlmOptimizerConfig &config)
{
    m_optimizerConfig = config;
}

void LlmOptimizer::setSummaryConfig(const LlmSummaryConfig &config)
{
    m_summaryConfig = config;
}

void LlmOptimizer::optimizeText(const QString &text)
{
    qDebug() << "LlmOptimizer::optimizeText - Input text:" << text;
    if (!m_optimizerConfig.enabled) {
        qDebug() << "LlmOptimizer::optimizeText - Optimization disabled, returning original";
        emit optimizationResult(text, text);
        return;
    }

    QString context;
    for (const auto &sentence : m_sentenceHistory) {
        context += sentence + "\n";
    }

    m_sentenceHistory.push_back(text);
    while (m_sentenceHistory.size() > static_cast<size_t>(m_optimizerConfig.context_sentences)) {
        m_sentenceHistory.pop_front();
    }
    qDebug() << "LlmOptimizer::optimizeText - Context size after update:" << m_sentenceHistory.size();

    PendingRequest request;
    request.type = PendingRequest::Optimize;
    request.context = context;
    request.current = text;
    request.timeoutMs = 30000; // 30秒超时
    m_requestQueue.push_back(request);

    processNextRequest();
}

void LlmOptimizer::clearHistory()
{
    m_sentenceHistory.clear();
}

void LlmOptimizer::generateSummary(const QString &content)
{
    PendingRequest request;
    request.type = PendingRequest::Summary;
    request.content = content;
    request.timeoutMs = 120000; // 2分钟超时
    m_requestQueue.push_back(request);

    processNextRequest();
}

void LlmOptimizer::processNextRequest()
{
    if (m_isProcessing || m_requestQueue.empty()) {
        return;
    }

    m_isProcessing = true;
    PendingRequest request = m_requestQueue.front();
    m_requestQueue.pop_front();

    if (request.type == PendingRequest::Optimize) {
        m_currentRequestType = PendingRequest::Optimize;
        m_currentOriginalText = request.current;
        sendRequest(request.context, request.current);
    } else {
        m_currentRequestType = PendingRequest::Summary;
        m_currentOriginalText = "";
        sendSummaryRequest(request.content);
    }

    m_requestTimer->setInterval(request.timeoutMs);
    m_requestTimer->start();
}

void LlmOptimizer::sendRequest(const QString &context, const QString &current)
{
    QString apiUrl = QString::fromStdString(m_optimizerConfig.api_url);
    if (!apiUrl.endsWith("/chat/completions")) {
        if (!apiUrl.endsWith("/")) {
            apiUrl += "/";
        }
        apiUrl += "chat/completions";
    }

    QNetworkRequest request;
    request.setUrl(QUrl(apiUrl));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", QString("Bearer %1").arg(QString::fromStdString(m_optimizerConfig.api_key)).toUtf8());

    QSslConfiguration sslConfig = QSslConfiguration::defaultConfiguration();
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    request.setSslConfiguration(sslConfig);

    QJsonObject messageObj;
    messageObj["role"] = "user";
    messageObj["content"] = buildPrompt(context, current);

    QJsonArray messagesArray;
    messagesArray.append(messageObj);

    QJsonObject jsonObj;
    jsonObj["model"] = QString::fromStdString(m_optimizerConfig.model_name);
    jsonObj["messages"] = messagesArray;
    jsonObj["temperature"] = 0.3;
    jsonObj["max_tokens"] = 512;

    QJsonDocument jsonDoc(jsonObj);
    QByteArray jsonData = jsonDoc.toJson();

    qDebug() << "LlmOptimizer::sendRequest - API URL:" << apiUrl;

    m_currentReply = m_networkManager->post(request, jsonData);
    m_currentReply->setProperty("originalText", current);
    m_currentReply->setProperty("isSummary", false);
    connect(m_currentReply, &QNetworkReply::finished, this, [this]() {
        onReplyFinished(m_currentReply);
    });
}

QString LlmOptimizer::buildPrompt(const QString &context, const QString &current)
{
    QString prompt = QString::fromStdString(m_optimizerConfig.prompt);
    prompt.replace("{context}", context);
    prompt.replace("{current}", current);
    return prompt;
}

QString LlmOptimizer::buildSummaryPrompt(const QString &content)
{
    QString prompt = QString::fromStdString(m_summaryConfig.prompt);
    prompt.replace("{content}", content);
    return prompt;
}

void LlmOptimizer::sendSummaryRequest(const QString &content)
{
    QString apiUrl = QString::fromStdString(m_summaryConfig.api_url);
    if (!apiUrl.endsWith("/chat/completions")) {
        if (!apiUrl.endsWith("/")) {
            apiUrl += "/";
        }
        apiUrl += "chat/completions";
    }

    QNetworkRequest request;
    request.setUrl(QUrl(apiUrl));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", QString("Bearer %1").arg(QString::fromStdString(m_summaryConfig.api_key)).toUtf8());

    QSslConfiguration sslConfig = QSslConfiguration::defaultConfiguration();
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    request.setSslConfiguration(sslConfig);

    QJsonObject messageObj;
    messageObj["role"] = "user";
    messageObj["content"] = buildSummaryPrompt(content);

    QJsonArray messagesArray;
    messagesArray.append(messageObj);

    QJsonObject jsonObj;
    jsonObj["model"] = QString::fromStdString(m_summaryConfig.model_name);
    jsonObj["messages"] = messagesArray;
    jsonObj["temperature"] = 0.3;
    jsonObj["max_tokens"] = 2048;

    QJsonDocument jsonDoc(jsonObj);
    QByteArray jsonData = jsonDoc.toJson();

    qDebug() << "LlmOptimizer::sendSummaryRequest - API URL:" << apiUrl;

    m_currentReply = m_networkManager->post(request, jsonData);
    m_currentReply->setProperty("isSummary", true);
    connect(m_currentReply, &QNetworkReply::finished, this, [this]() {
        onReplyFinished(m_currentReply);
    });
}

QString LlmOptimizer::extractResponse(const QJsonDocument &jsonDoc)
{
    if (!jsonDoc.isObject()) {
        return QString();
    }

    QJsonObject root = jsonDoc.object();
    if (!root.contains("choices") || !root["choices"].isArray()) {
        return QString();
    }

    QJsonArray choices = root["choices"].toArray();
    if (choices.isEmpty()) {
        return QString();
    }

    QJsonObject choice = choices[0].toObject();
    if (!choice.contains("message") || !choice["message"].isObject()) {
        return QString();
    }

    QJsonObject message = choice["message"].toObject();
    if (!message.contains("content")) {
        return QString();
    }

    return message["content"].toString().trimmed();
}

void LlmOptimizer::onReplyFinished(QNetworkReply *reply)
{
    if (reply != m_currentReply) {
        reply->deleteLater();
        return;
    }

    m_requestTimer->stop();
    bool isSummary = reply->property("isSummary").toBool();

    if (reply->error() != QNetworkReply::NoError) {
        QString error = QString("LLM 请求错误: %1").arg(reply->errorString());
        qDebug() << "LlmOptimizer::onReplyFinished - Error:" << error;
        if (isSummary) {
            // 纪要请求出错不弹窗，只输出日志
            qDebug() << "LlmOptimizer::onReplyFinished - Summary request failed";
            emit summaryComplete();
        } else {
            // 优化请求出错返回原文
            emit optimizationResult(m_currentOriginalText, m_currentOriginalText);
        }
    } else {
        QByteArray responseData = reply->readAll();
        QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData);
        QString result = extractResponse(jsonDoc);

        if (isSummary) {
            qDebug() << "LlmOptimizer::onReplyFinished - Summary result received";
            emit summaryResult(result);
            emit summaryComplete();
        } else {
            qDebug() << "LlmOptimizer::onReplyFinished - Optimized text received";
            if (result.isEmpty()) {
                emit optimizationResult(m_currentOriginalText, m_currentOriginalText);
            } else {
                emit optimizationResult(m_currentOriginalText, result);
            }
        }
    }

    reply->deleteLater();
    m_currentReply = nullptr;
    m_isProcessing = false;

    processNextRequest();
}

void LlmOptimizer::onRequestTimeout()
{
    qDebug() << "LlmOptimizer::onRequestTimeout - Request timed out";

    if (m_currentReply) {
        m_currentReply->abort();
        
        if (m_currentRequestType == PendingRequest::Summary) {
            // 纪要超时不弹窗
            qDebug() << "LlmOptimizer::onRequestTimeout - Summary request timed out";
            emit summaryComplete();
        } else {
            // 优化超时返回原文
            emit optimizationResult(m_currentOriginalText, m_currentOriginalText);
        }

        m_currentReply->deleteLater();
        m_currentReply = nullptr;
    }

    m_isProcessing = false;
    processNextRequest();
}
