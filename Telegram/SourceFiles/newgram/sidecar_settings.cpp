/*
This file is part of Newgram, a fork of Telegram Desktop.

Newgram is licensed under the terms of the GPLv3; see LICENSE at repo root.
*/
#include "newgram/sidecar_settings.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QStandardPaths>

namespace Newgram {

QStringList SidecarSettings::hintKeysFromEnv() const {
	QStringList keys;
	const auto env = QProcessEnvironment::systemEnvironment();
	if (env.contains(QStringLiteral("ANTHROPIC_API_KEY"))) {
		keys << QStringLiteral("anthropic");
	}
	if (env.contains(QStringLiteral("OPENAI_API_KEY"))) {
		keys << QStringLiteral("openai");
	}
	return keys;
}

void SidecarSettings::resolveDefaults(const QString &tdesktopDataDir) {
	if (sidecarDir.isEmpty()) {
		// The sidecar lives next to the tdesktop checkout in the newgram
		// umbrella repo. For dev builds we locate it relative to the app
		// binary's source tree; for packaged builds the path will come from
		// the resource bundle and should be set by the caller.
		const auto appDir = QCoreApplication::applicationDirPath();
		const auto candidate = QDir::cleanPath(appDir + QStringLiteral("/../../../../sidecar"));
		if (QFileInfo::exists(candidate + QStringLiteral("/pyproject.toml"))) {
			sidecarDir = candidate;
		} else {
			// Fall back to the parent of the tdesktop data dir — the umbrella
			// repo's top level in most dev setups.
			const auto guess = QDir::cleanPath(tdesktopDataDir + QStringLiteral("/../../../sidecar"));
			if (QFileInfo::exists(guess + QStringLiteral("/pyproject.toml"))) {
				sidecarDir = guess;
			}
		}
	}
	if (uvPath.isEmpty()) {
		uvPath = QStringLiteral("uv");
	}
}

void SidecarSettings::mergeEnvironment() {
	const auto env = QProcessEnvironment::systemEnvironment();
	const auto get = [&env](const QString &key) {
		return env.value(key);
	};
	if (const auto v = get(QStringLiteral("NEWGRAM_SIDECAR_URL")); !v.isEmpty()) {
		manualUrl = v;
	}
	if (const auto v = get(QStringLiteral("NEWGRAM_SIDECAR_DIR")); !v.isEmpty()) {
		sidecarDir = v;
	}
	if (const auto v = get(QStringLiteral("NEWGRAM_UV_PATH")); !v.isEmpty()) {
		uvPath = v;
	}
	if (const auto v = get(QStringLiteral("NEWGRAM_AGENT_PROVIDER")); !v.isEmpty()) {
		defaultProvider = v;
	}
	if (const auto v = get(QStringLiteral("NEWGRAM_SANDBOX")); !v.isEmpty()) {
		sandboxBackend = v;
	}
}

} // namespace Newgram
