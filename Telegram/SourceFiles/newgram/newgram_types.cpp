/*
This file is part of Newgram, a fork of Telegram Desktop.

Newgram is licensed under the terms of the GPLv3; see LICENSE at repo root.
*/
#include "newgram/newgram_types.h"

#include <QJsonDocument>
#include <QJsonValue>

namespace Newgram {
namespace {

QJsonObject ToObj(const QJsonValue &v) {
	return v.isObject() ? v.toObject() : QJsonObject();
}

QString ContentToDisplayJson(const QJsonValue &value) {
	if (value.isString()) {
		return value.toString();
	}
	if (value.isNull() || value.isUndefined()) {
		return QString();
	}
	return QString::fromUtf8(
		QJsonDocument(value.isArray()
			? QJsonDocument(value.toArray())
			: QJsonDocument(ToObj(value))).toJson(QJsonDocument::Compact));
}

} // namespace

std::optional<AgentEvent> ParseAgentEvent(const QByteArray &json) {
	const auto doc = QJsonDocument::fromJson(json);
	if (!doc.isObject()) {
		return std::nullopt;
	}
	const auto obj = doc.object();
	const auto kind = obj.value("kind").toString();

	if (kind == "meta") {
		return AgentEvent{ AgentMeta{
			obj.value("task").toString(),
			obj.value("provider").toString(),
			obj.value("server_version").toString(),
		} };
	}
	if (kind == "thinking_delta") {
		return AgentEvent{ ThinkingDelta{ obj.value("text").toString() } };
	}
	if (kind == "text_delta") {
		return AgentEvent{ TextDelta{ obj.value("text").toString() } };
	}
	if (kind == "tool_call") {
		return AgentEvent{ ToolCall{
			obj.value("call_id").toString(),
			obj.value("tool_name").toString(),
			ToObj(obj.value("input")),
		} };
	}
	if (kind == "tool_result") {
		return AgentEvent{ ToolResult{
			obj.value("call_id").toString(),
			obj.value("tool_name").toString(),
			obj.value("is_error").toBool(false),
			ContentToDisplayJson(obj.value("content")),
		} };
	}
	if (kind == "done") {
		return AgentEvent{ AgentDone{
			obj.value("final_text").toString(),
			ToObj(obj.value("usage")),
		} };
	}
	if (kind == "error") {
		return AgentEvent{ AgentError{
			obj.value("message").toString(),
			obj.value("recoverable").toBool(false),
		} };
	}
	return std::nullopt;
}

QString SummaryLabel(const AgentEvent &event) {
	return std::visit([](const auto &e) -> QString {
		using T = std::decay_t<decltype(e)>;
		if constexpr (std::is_same_v<T, AgentMeta>) {
			return QString("Using %1 for %2").arg(e.provider, e.task);
		} else if constexpr (std::is_same_v<T, ThinkingDelta>) {
			return QString("Thinking…");
		} else if constexpr (std::is_same_v<T, TextDelta>) {
			return e.text;
		} else if constexpr (std::is_same_v<T, ToolCall>) {
			return QString("Calling %1…").arg(e.toolName);
		} else if constexpr (std::is_same_v<T, ToolResult>) {
			return e.isError
				? QString("Tool %1 failed").arg(e.toolName.isEmpty() ? QString("(unnamed)") : e.toolName)
				: QString("Tool result ready");
		} else if constexpr (std::is_same_v<T, AgentDone>) {
			return QString("Done");
		} else if constexpr (std::is_same_v<T, AgentError>) {
			return QString("Error: %1").arg(e.message);
		}
	}, event);
}

} // namespace Newgram

