/*
This file is part of Newgram, a fork of Telegram Desktop.

Newgram is licensed under the terms of the GPLv3; see LICENSE at repo root.
*/
#include "newgram/summarize_action.h"

#include "data/data_peer.h"
#include "ui/boxes/confirm_box.h"
#include "ui/layers/generic_box.h"
#include "ui/rp_widget.h"
#include "webview/webview_embed.h"
#include "window/window_session_controller.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QProcessEnvironment>
#include <QtCore/QUrl>
#include <QtCore/QUrlQuery>

namespace Newgram {
namespace {

constexpr auto kPanelWidth = 720;
constexpr auto kPanelHeight = 560;

class WebviewHolder final : public Ui::RpWidget {
public:
	WebviewHolder(QWidget *parent, const QString &url)
	: RpWidget(parent)
	, _webview(std::make_unique<Webview::Window>(
		this,
		Webview::WindowConfig{ .safe = true })) {
		if (auto *const widget = _webview->widget()) {
			widget->setParent(this);
			widget->show();
		}
		_webview->navigate(url);

		sizeValue(
		) | rpl::start_with_next([this](QSize size) {
			if (auto *const widget = _webview->widget()) {
				widget->setGeometry(QRect(QPoint(), size));
			}
		}, lifetime());
	}

private:
	std::unique_ptr<Webview::Window> _webview;
};

[[nodiscard]] QString ResolveBaseUrl(const QProcessEnvironment &env) {
	return env.value(QStringLiteral("NEWGRAM_SIDECAR_URL"));
}

[[nodiscard]] QString ResolveToken(const QProcessEnvironment &env) {
	return env.value(QStringLiteral("NEWGRAM_SIDECAR_TOKEN"));
}

[[nodiscard]] QString BuildUiUrl(
		const QString &baseUrl,
		const QString &token,
		const QString &task,
		const QString &chatId,
		const QString &title) {
	QUrl url(baseUrl);
	url.setPath(url.path() + QStringLiteral("/ui/"));
	QUrlQuery q;
	q.addQueryItem(QStringLiteral("task"), task);
	if (!token.isEmpty()) {
		q.addQueryItem(QStringLiteral("token"), token);
	}
	q.addQueryItem(QStringLiteral("chat_id"), chatId);
	q.addQueryItem(QStringLiteral("title"), title);
	q.addQueryItem(QStringLiteral("sidecar"), baseUrl);
	url.setQuery(q);
	return url.toString();
}

} // namespace

void ShowSummarizeChatPlaceholder(
		Window::SessionController *controller,
		PeerData *peer) {
	if (!controller || !peer) {
		return;
	}

	const auto env = QProcessEnvironment::systemEnvironment();
	const auto baseUrl = ResolveBaseUrl(env);
	if (baseUrl.isEmpty()) {
		controller->show(Ui::MakeInformBox(QStringLiteral(
			"Newgram sidecar not configured.\n\n"
			"Run in a terminal:\n"
			"  NEWGRAM_SIDECAR_TOKEN=demo \\\n"
			"      uv --directory <path-to>/newgram/sidecar run "
			"newgram-sidecar --port 8765 --token demo\n\n"
			"Then relaunch Telegram with:\n"
			"  NEWGRAM_SIDECAR_URL=http://127.0.0.1:8765 \\\n"
			"  NEWGRAM_SIDECAR_TOKEN=demo \\\n"
			"      open -a Telegram.app")));
		return;
	}

	const auto title = QStringLiteral("Summarize %1").arg(peer->name());
	const auto chatId = QString::number(peer->id.value);
	const auto url = BuildUiUrl(
		baseUrl,
		ResolveToken(env),
		QStringLiteral("summarize_chat"),
		chatId,
		title);

	controller->show(Box([url, title](not_null<Ui::GenericBox*> box) {
		box->setTitle(rpl::single(title));
		box->setNoContentMargin(true);
		box->setWidth(kPanelWidth);

		const auto holder = box->addRow(
			object_ptr<WebviewHolder>(box, url),
			QMargins());
		holder->resize(kPanelWidth, kPanelHeight);

		box->addButton(
			rpl::single(QStringLiteral("Close")),
			[=] { box->closeBox(); });
	}));
}

} // namespace Newgram
