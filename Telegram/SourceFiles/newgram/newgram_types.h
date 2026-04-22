/*
This file is part of Newgram, a fork of Telegram Desktop.

Newgram is licensed under the terms of the GPLv3; see LICENSE at repo root.
*/
#pragma once

#include <QString>
#include <QJsonObject>
#include <variant>

namespace Newgram {

// Mirrors sidecar/newgram_sidecar/agents/base.py's AgentEvent discriminated
// union. Kept intentionally narrow — anything richer lives in the sidecar.
struct AgentMeta {
	QString task;
	QString provider;
	QString serverVersion;
};

struct ThinkingDelta {
	QString text;
};

struct TextDelta {
	QString text;
};

struct ToolCall {
	QString callId;
	QString toolName;
	QJsonObject input;
};

struct ToolResult {
	QString callId;
	QString toolName;
	bool isError = false;
	QString contentJson;   // raw JSON representation for display
};

struct AgentDone {
	QString finalText;
	QJsonObject usage;
};

struct AgentError {
	QString message;
	bool recoverable = false;
};

using AgentEvent = std::variant<
	AgentMeta,
	ThinkingDelta,
	TextDelta,
	ToolCall,
	ToolResult,
	AgentDone,
	AgentError>;

// Parse one SSE "data:" payload (a JSON object) into an AgentEvent. Returns
// std::nullopt for unrecognized `kind` values (forward-compat with future
// event types the sidecar may emit).
[[nodiscard]] std::optional<AgentEvent> ParseAgentEvent(const QByteArray &json);

// Human-readable one-line label for UI chips ("Reading 50 messages…", etc.).
[[nodiscard]] QString SummaryLabel(const AgentEvent &event);

} // namespace Newgram
