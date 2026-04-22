/*
This file is part of Newgram, a fork of Telegram Desktop.

Newgram is licensed under the terms of the GPLv3; see LICENSE at repo root.
*/
#include "newgram/sidecar_process.h"

#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>
#include <QUrl>

namespace Newgram {
namespace {

constexpr auto kMaxRestartAttempts = 3;
constexpr auto kRestartBackoffMs = 1000;

} // namespace

SidecarProcess::SidecarProcess(SidecarSettings settings, QObject *parent)
: QObject(parent)
, _settings(std::move(settings)) {
	_restartTimer.setSingleShot(true);
	connect(&_restartTimer, &QTimer::timeout, this, [this] {
		launchProcess();
	});
}

SidecarProcess::~SidecarProcess() {
	stop();
}

void SidecarProcess::start() {
	if (!_settings.manualUrl.isEmpty()) {
		// Dev override: trust the manually-run sidecar.
		_baseUrl = _settings.manualUrl;
		_ready = true;
		emit ready(_baseUrl, _token);
		return;
	}
	launchProcess();
}

void SidecarProcess::stop() {
	_restartTimer.stop();
	if (_proc) {
		_proc->disconnect(this);
		if (_proc->state() != QProcess::NotRunning) {
			_proc->terminate();
			if (!_proc->waitForFinished(2000)) {
				_proc->kill();
				_proc->waitForFinished(1000);
			}
		}
		_proc->deleteLater();
		_proc = nullptr;
	}
	_ready = false;
}

void SidecarProcess::launchProcess() {
	if (_settings.sidecarDir.isEmpty()) {
		emit died(-1, QStringLiteral("sidecar dir not resolved"));
		return;
	}

	_proc = new QProcess(this);
	_proc->setProgram(_settings.uvPath);
	_proc->setArguments(QStringList{
		QStringLiteral("--directory"),
		_settings.sidecarDir,
		QStringLiteral("run"),
		QStringLiteral("newgram-sidecar"),
		QStringLiteral("--port"),
		QStringLiteral("0"),
		QStringLiteral("--log-level"),
		QStringLiteral("info"),
	});
	_proc->setProcessChannelMode(QProcess::SeparateChannels);

	connect(_proc, &QProcess::readyReadStandardOutput, this, [this] {
		if (!_proc) return;
		_stdoutBuffer += _proc->readAllStandardOutput();
		int nl;
		while ((nl = _stdoutBuffer.indexOf('\n')) >= 0) {
			const auto line = _stdoutBuffer.left(nl);
			_stdoutBuffer.remove(0, nl + 1);
			handleStdoutLine(line);
		}
	});
	connect(_proc, &QProcess::readyReadStandardError, this, [this] {
		if (!_proc) return;
		const auto data = _proc->readAllStandardError();
		for (const auto &chunk : data.split('\n')) {
			if (!chunk.isEmpty()) {
				emit logLine(QString::fromUtf8(chunk));
			}
		}
	});
	connect(_proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
		this, [this](int ec, QProcess::ExitStatus s) { onFinished(ec, s); });

	_proc->start();
	if (!_proc->waitForStarted(5000)) {
		emit died(-1, QStringLiteral("sidecar failed to start (uv missing?)"));
		_proc->deleteLater();
		_proc = nullptr;
		scheduleRestart();
	}
}

void SidecarProcess::handleStdoutLine(const QByteArray &line) {
	const auto doc = QJsonDocument::fromJson(line);
	if (doc.isObject()) {
		const auto obj = doc.object();
		if (obj.value("kind").toString() == QStringLiteral("sidecar_started")) {
			const auto host = obj.value("host").toString(QStringLiteral("127.0.0.1"));
			const auto port = obj.value("port").toInt();
			_token = obj.value("token").toString();
			_baseUrl = QString("http://%1:%2").arg(host).arg(port);
			_ready = true;
			_restartAttempts = 0;
			emit ready(_baseUrl, _token);
			return;
		}
	}
	emit logLine(QString::fromUtf8(line));
}

void SidecarProcess::onFinished(int exitCode, QProcess::ExitStatus) {
	_ready = false;
	const auto tail = _proc ? _proc->readAllStandardError() : QByteArray();
	if (_proc) {
		_proc->deleteLater();
		_proc = nullptr;
	}
	emit died(exitCode, QString::fromUtf8(tail.right(2048)));
	scheduleRestart();
}

void SidecarProcess::scheduleRestart() {
	if (!_settings.manualUrl.isEmpty()) {
		return;
	}
	if (_restartAttempts >= kMaxRestartAttempts) {
		return;
	}
	++_restartAttempts;
	_restartTimer.start(kRestartBackoffMs * _restartAttempts);
}

} // namespace Newgram
