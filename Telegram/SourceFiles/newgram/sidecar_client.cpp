/*
This file is part of Newgram, a fork of Telegram Desktop.

Newgram is licensed under the terms of the GPLv3; see LICENSE at repo root.
*/
#include "newgram/sidecar_client.h"

#include <QtCore/QByteArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QUrl>
#include <QtCore/QUrlQuery>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

namespace Newgram {
namespace {

constexpr auto kTokenHeader = "X-Newgram-Token";
constexpr auto kAcceptHeader = "Accept";
constexpr auto kSseMime = "text/event-stream";

QList<QByteArray> SplitSseFrames(QByteArray &buffer) {
	// SSE frames are terminated by a blank line (``\n\n`` or ``\r\n\r\n``).
	// We split on ``\n\n`` after normalizing CRLFs so callers never have to
	// see raw bytes.
	buffer.replace("\r\n", "\n");
	QList<QByteArray> out;
	int sep;
	while ((sep = buffer.indexOf("\n\n")) >= 0) {
		out.append(buffer.left(sep));
		buffer.remove(0, sep + 2);
	}
	return out;
}

QByteArray ExtractDataPayload(const QByteArray &frame) {
	// Concatenate all "data:" lines as per the SSE spec (stripping the
	// leading "data:" and the single following optional space).
	QByteArray data;
	for (const auto &line : frame.split('\n')) {
		if (line.startsWith("data:")) {
			auto slice = line.mid(5);
			if (slice.startsWith(' ')) {
				slice.remove(0, 1);
			}
			if (!data.isEmpty()) {
				data.append('\n');
			}
			data.append(slice);
		}
	}
	return data;
}

} // namespace


TaskStream::TaskStream(QObject *parent)
: QObject(parent) {
}

TaskStream::~TaskStream() {
	if (_reply && !_done) {
		_reply->abort();
		_reply->deleteLater();
		_reply = nullptr;
	}
}

void TaskStream::attach(QNetworkReply *reply) {
	_reply = reply;
	connect(reply, &QNetworkReply::readyRead, this, &TaskStream::handleReadyRead);
	connect(reply, &QNetworkReply::finished, this, &TaskStream::handleFinished);
}

void TaskStream::handleReadyRead() {
	if (!_reply) return;
	_buffer.append(_reply->readAll());
	for (const auto &frame : SplitSseFrames(_buffer)) {
		const auto payload = ExtractDataPayload(frame);
		if (!payload.isEmpty()) {
			Q_EMIT agentEventJson(payload);
		}
	}
}

void TaskStream::handleFinished() {
	if (!_reply) return;
	// Drain any trailing data.
	_buffer.append(_reply->readAll());
	for (const auto &frame : SplitSseFrames(_buffer)) {
		const auto payload = ExtractDataPayload(frame);
		if (!payload.isEmpty()) {
			Q_EMIT agentEventJson(payload);
		}
	}

	const auto error = _reply->error();
	_reply->deleteLater();
	_reply = nullptr;
	_done = true;
	if (error == QNetworkReply::NoError) {
		Q_EMIT finished();
	} else {
		Q_EMIT failed(QString("network error: %1").arg(error));
	}
}

SidecarClient::SidecarClient(QObject *parent)
: QObject(parent)
, _manager(new QNetworkAccessManager(this)) {
}

SidecarClient::~SidecarClient() = default;

void SidecarClient::configure(const QString &baseUrl, const QString &token) {
	_baseUrl = baseUrl;
	_token = token;
}

void SidecarClient::probeHealth(std::function<void(bool)> callback) {
	if (!isConfigured()) {
		callback(false);
		return;
	}
	QNetworkRequest request(QUrl(_baseUrl + "/healthz"));
	auto *reply = _manager->get(request);
	connect(reply, &QNetworkReply::finished, this, [reply, cb = std::move(callback)]() {
		const auto ok = (reply->error() == QNetworkReply::NoError);
		reply->deleteLater();
		cb(ok);
	});
}

TaskStream *SidecarClient::startTask(
		const QString &taskName,
		const QJsonObject &payload,
		const QString &providerOverride) {
	auto *stream = new TaskStream(this);
	if (!isConfigured()) {
		QMetaObject::invokeMethod(stream, [stream] {
			Q_EMIT stream->failed(QStringLiteral("sidecar not configured yet"));
		}, Qt::QueuedConnection);
		return stream;
	}

	QUrl url(_baseUrl + "/tasks/" + taskName);
	if (!providerOverride.isEmpty()) {
		QUrlQuery q;
		q.addQueryItem(QStringLiteral("agent"), providerOverride);
		url.setQuery(q);
	}

	QNetworkRequest request(url);
	request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
	request.setRawHeader(kAcceptHeader, kSseMime);
	request.setRawHeader(kTokenHeader, _token.toUtf8());

	const auto body = QJsonDocument(payload).toJson(QJsonDocument::Compact);
	auto *reply = _manager->post(request, body);
	stream->attach(reply);
	return stream;
}

} // namespace Newgram
