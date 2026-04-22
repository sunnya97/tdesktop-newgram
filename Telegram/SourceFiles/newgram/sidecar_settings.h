/*
This file is part of Newgram, a fork of Telegram Desktop.

Newgram is licensed under the terms of the GPLv3; see LICENSE at repo root.
*/
#pragma once

#include <QtCore/QString>
#include <QtCore/QStringList>

namespace Newgram {

// Runtime-resolvable sidecar configuration. Everything here is either
// derived from env (dev override) or persisted to the tdesktop data dir.
// Full tdesktop Core::Settings integration is a follow-up — see settings.cpp.
struct SidecarSettings {
	// If non-empty, the fork connects here instead of auto-spawning. Set via
	// the `NEWGRAM_SIDECAR_URL` env var to hack on the sidecar in isolation.
	QString manualUrl;

	// Path to `uv` executable used to launch the sidecar.
	QString uvPath = QStringLiteral("uv");

	// Path (absolute) of the sidecar Python project directory.
	QString sidecarDir;

	// Default provider preferred for new tasks. Overridden per-request.
	QString defaultProvider = QStringLiteral("claude_sdk");

	// Sandbox backend — "auto" (recommended on macOS), "noop" (dev), "sandbox_exec", "docker".
	QString sandboxBackend = QStringLiteral("auto");

	// Provider credentials flow via environment, not through the fork. These
	// lists are only for discovery — the fork surfaces which credentials are
	// set/missing in the UI.
	[[nodiscard]] QStringList hintKeysFromEnv() const;

	// Resolves sensible defaults given a tdesktop data dir (e.g.
	// `~/Library/Application Support/Telegram Desktop/`).
	void resolveDefaults(const QString &tdesktopDataDir);

	// Load overrides from the environment. Non-destructive: only keys present
	// in env are changed.
	void mergeEnvironment();
};

} // namespace Newgram
