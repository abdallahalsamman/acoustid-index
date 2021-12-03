// Copyright (C) 2019  Lukas Lalinsky
// Distributed under the MIT license, see the LICENSE file for details.

#ifndef ACOUSTID_SERVER_HTTP_H_
#define ACOUSTID_SERVER_HTTP_H_

#include <QRegularExpression>
#include <QSharedPointer>

#include "qhttpserver.hpp"
#include "qhttpserverrequest.hpp"
#include "qhttpserverresponse.hpp"

namespace Acoustid {

class MultiIndex;

namespace Server {

class Metrics;

class HttpRequest {
 public:
    HttpRequest(const QMap<QString, QString> &args);

    QString getArg(const QString &name, const QString &defaultValue = QString()) const;

 private:
    QMap<QString, QString> m_args;
};

class HttpResponse {
 public:
    HttpResponse();

    void setStatus(qhttp::TStatusCode status);
    void setHeader(const QString &name, const QString &value);
    void setBody(const QString &text);
    void setBody(const QJsonDocument &doc);

    void send(qhttp::server::QHttpResponse *response) const;

 private:
    qhttp::TStatusCode m_status;
    QMap<QString, QString> m_headers;
    QByteArray m_body;
};

typedef std::function<HttpResponse(const HttpRequest &)> HttpRequestHandlerFunc;

class HttpRequestHandler : public QObject {
    Q_OBJECT

 public:
    HttpRequestHandler(QSharedPointer<MultiIndex> indexes, QSharedPointer<Metrics> metrics);

    void handleRequest(qhttp::server::QHttpRequest *req, qhttp::server::QHttpResponse *res);

    template <typename T>
    HttpResponse makeResponse(qhttp::TStatusCode status, const T &body) {
        HttpResponse response;
        response.setStatus(status);
        response.setBody(body);
        return response;
    }

 private:
    QSharedPointer<MultiIndex> m_indexes;
    QSharedPointer<Metrics> m_metrics;

    void addHandler(qhttp::THttpMethod, const QString &pattern, HttpRequestHandlerFunc handler);

    std::vector<std::tuple<qhttp::THttpMethod, QRegularExpression, HttpRequestHandlerFunc>> m_handlers;
};

}  // namespace Server
}  // namespace Acoustid

#endif
