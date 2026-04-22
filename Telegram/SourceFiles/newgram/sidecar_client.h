/*
This file is part of Newgram, a fork of Telegram Desktop.

Newgram is licensed under the terms of the GPLv3; see LICENSE at repo root.
*/
#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QJsonObject>
#include <QtCore/QObject>
#include <QtCore/QString>

#include <memory>

class QNetworkAccessManager;
class QNetworkReply;

namespace Newgram {

// One in-flight task stream. Constructed by SidecarClient::startTask; owns
// its own QNetworkReply and surfaces AgentEvent JSON payloads as Qt signals
// on the UI thread. Delete to cancel.
//
// The signal carries the raw JSON bytes of each SSE-framed event rather than
// the parsed std::variant — moc can't reliably parse discriminated-union
// template types in signal parameters, and having the receiver call
// Newgram::ParseAgentEvent() on the payload keeps the Qt metaobject surface
// free of our variant types.
class TaskStream : public QObject {
	Q_OBJECT
public:
	explicit TaskStream(QObject *parent = nullptr);
	~TaskStream() override;

Q_SIGNALS:
	void agentEventJson(QByteArray payload);
	void finished();
	void failed(QString reason);

private:
	friend class SidecarClient;
	void attach(QNetworkReply *reply);
	void handleReadyRead();
	void handleFinished();

	QNetworkReply *_reply = nullptr;
	QByteArray _buffer;
	bool _done = false;
};

// Small client wrapping QNetworkAccessManager for the sidecar's REST +
// SSE endpoints. Not thread-safe; call from the Qt UI thread.
class SidecarClient : public QObject {
	Q_OBJECT
public:
	explicit SidecarClient(QObject *parent = nullptr);
	~SidecarClient() override;

	// Updates the endpoint + token. Call whenever SidecarProcess emits ready().
	void configure(const QString &baseUrl, const QString &token);

	[[nodiscard]] bool isConfigured() const { return !_baseUrl.isEmpty(); }
	[[nodiscard]] QString baseUrl() const { return _baseUrl; }

	// Async GET /healthz → bool via callback.
	void probeHealth(std::function<void(bool ok)> callback);

	// Start a task. Caller owns the returned TaskStream and deletes it to cancel.
	TaskStream *startTask(
		const QString &taskName,
		const QJsonObject &payload,
		const QString &providerOverride = QString());

private:
	QNetworkAccessManager *_manager;
	QString _baseUrl;
	QString _token;
};

} // namespace Newgram
