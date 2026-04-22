/*
This file is part of Newgram, a fork of Telegram Desktop.

Newgram is licensed under the terms of the GPLv3; see LICENSE at repo root.
*/
#pragma once

#include "newgram/sidecar_settings.h"

#include <QObject>
#include <QProcess>
#include <QString>
#include <QTimer>

namespace Newgram {

// Manages the agent-host sidecar process's lifecycle. The fork spawns one
// of these at application start; it waits for the sidecar's stdout
// handshake line (a JSON object with port + token), then exposes those
// values for SidecarClient to use.
//
// On `NEWGRAM_SIDECAR_URL` being set in env, the spawn is skipped — the
// fork connects to a manually-run sidecar instead (handy for iteration).
class SidecarProcess : public QObject {
	Q_OBJECT
public:
	explicit SidecarProcess(SidecarSettings settings, QObject *parent = nullptr);
	~SidecarProcess() override;

	void start();
	void stop();

	[[nodiscard]] bool isReady() const { return _ready; }
	[[nodiscard]] QString baseUrl() const { return _baseUrl; }
	[[nodiscard]] QString token() const { return _token; }

Q_SIGNALS:
	void ready(QString baseUrl, QString token);
	void died(int exitCode, QString stderrTail);
	void logLine(QString line);

private:
	void launchProcess();
	void handleStdoutLine(const QByteArray &line);
	void onFinished(int exitCode, QProcess::ExitStatus status);
	void scheduleRestart();

	SidecarSettings _settings;
	QProcess *_proc = nullptr;
	QTimer _restartTimer;
	QByteArray _stdoutBuffer;
	int _restartAttempts = 0;
	bool _ready = false;
	QString _baseUrl;
	QString _token;
};

} // namespace Newgram
