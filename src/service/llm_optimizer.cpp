#include "llm_optimizer.h"
#include <QNetworkRequest>
#include <QSslConfiguration>
#include <QDebug>

LlmOptimizer::LlmOptimizer(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
{
}

LlmOptimizer::~LlmOptimizer()
{
}

void LlmOptimizer::setConfig(const LlmOptimizerConfig &config)
{
    m_config = config;
}

void LlmOptimizer::optimizeText(const QString &text)
{
    qDebug() << "LlmOptimizer::optimizeText - Input text:" << text;
    if (!m_config.enabled) {
        qDebug() << "LlmOptimizer::optimizeText - Optimization disabled, returning original";
        emit optimizationResult(text, text);
        return;
    }

    QString context;
    for (const auto &sentence : m_sentenceHistory) {
        context += sentence + "\n";
    }

    m_sentenceHistory.push_back(text);
    while (m_sentenceHistory.size() > static_cast<size_t>(m_config.context_sentences)) {
        m_sentenceHistory.pop_front();
    }
    qDebug() << "LlmOptimizer::optimizeText - Context size after update:" << m_sentenceHistory.size();

    sendRequest(context, text);
}

void LlmOptimizer::clearHistory()
{
    m_sentenceHistory.clear();
}

void LlmOptimizer::sendRequest(const QString &context, const QString &current)
{
    QString apiUrl = QString::fromStdString(m_config.api_url);
    if (!apiUrl.endsWith("/chat/completions")) {
        if (!apiUrl.endsWith("/")) {
            apiUrl += "/";
        }
        apiUrl += "chat/completions";
    }

    QNetworkRequest request;
    request.setUrl(QUrl(apiUrl));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", QString("Bearer %1").arg(QString::fromStdString(m_config.api_key)).toUtf8());

    QSslConfiguration sslConfig = QSslConfiguration::defaultConfiguration();
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    request.setSslConfiguration(sslConfig);

    QJsonObject messageObj;
    messageObj["role"] = "user";
    messageObj["content"] = buildPrompt(context, current);

    QJsonArray messagesArray;
    messagesArray.append(messageObj);

    QJsonObject jsonObj;
    jsonObj["model"] = QString::fromStdString(m_config.model_name);
    jsonObj["messages"] = messagesArray;
    jsonObj["temperature"] = 0.3;
    jsonObj["max_tokens"] = 512;

    QJsonDocument jsonDoc(jsonObj);
    QByteArray jsonData = jsonDoc.toJson();

    qDebug() << "LlmOptimizer::sendRequest - API URL:" << apiUrl;
    qDebug() << "LlmOptimizer::sendRequest - Request JSON:" << jsonData;

    QNetworkReply *reply = m_networkManager->post(request, jsonData);
    reply->setProperty("originalText", current);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onReplyFinished(reply);
    });
}

QString LlmOptimizer::buildPrompt(const QString &context, const QString &current)
{
    QString prompt = QString::fromStdString(m_config.prompt);
    prompt.replace("{context}", context);
    prompt.replace("{current}", current);
    return prompt;
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
    QString original = reply->property("originalText").toString();
    qDebug() << "LlmOptimizer::onReplyFinished - Original text:" << original;

    if (reply->error() != QNetworkReply::NoError) {
        QString error = QString("LLM 请求错误: %1").arg(reply->errorString());
        qDebug() << "LlmOptimizer::onReplyFinished - Error:" << error;
        reply->deleteLater();
        emit optimizationError(error);
        return;
    }

    QByteArray responseData = reply->readAll();
    qDebug() << "LlmOptimizer::onReplyFinished - Response data:" << responseData;
    reply->deleteLater();

    QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData);
    QString optimized = extractResponse(jsonDoc);
    qDebug() << "LlmOptimizer::onReplyFinished - Optimized text:" << optimized;

    if (optimized.isEmpty()) {
        emit optimizationResult(original, original);
    } else {
        emit optimizationResult(original, optimized);
    }
}
