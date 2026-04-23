/*
This file is part of Newgram, a fork of Telegram Desktop.

Newgram is licensed under the terms of the GPLv3; see LICENSE at repo root.
*/
#include "newgram/summarize_action.h"

#include "data/data_peer.h"
#include "main/main_session.h"
#include "storage/storage_account.h"
#include "ui/boxes/confirm_box.h"
#include "ui/layers/generic_box.h"
#include "ui/rp_widget.h"
#include "webview/webview_embed.h"
#include "webview/webview_interface.h"
#include "window/window_session_controller.h"

#include <QtCore/QProcessEnvironment>
#include <QtCore/QUrl>
#include <QtCore/QUrlQuery>

namespace Newgram {
namespace {

constexpr auto kPanelWidth = 720;
constexpr auto kPanelHeight = 560;

// Owns the Webview::Window and keeps its QWidget child filling the holder's
// rect. Mirrors the ownership style of Iv::Controller / LocationPicker:
// the Webview::Window is constructed with a parent RpWidget, which does the
// reparenting itself — we just resize its child widget as our size changes.
class WebviewHolder final : public Ui::RpWidget {
public:
	WebviewHolder(
			QWidget *parent,
			Webview::StorageId storageId,
			const QString &url)
	: RpWidget(parent)
	, _webview(std::make_unique<Webview::Window>(
		this,
		Webview::WindowConfig{
			.storageId = std::move(storageId),
			.safe = true,
		})) {
		if (auto *const w = _webview->widget()) {
			w->show();
		}
		_webview->navigate(url);

		sizeValue(
		) | rpl::on_next([this](QSize size) {
			if (auto *const w = _webview->widget()) {
				w->setGeometry(QRect(QPoint(), size));
			}
		}, lifetime());
	}

private:
	std::unique_ptr<Webview::Window> _webview;
};

[[nodiscard]] QString ResolveBaseUrl() {
	return QProcessEnvironment::systemEnvironment().value(
		QStringLiteral("NEWGRAM_SIDECAR_URL"));
}

[[nodiscard]] QString ResolveToken() {
	return QProcessEnvironment::systemEnvironment().value(
		QStringLiteral("NEWGRAM_SIDECAR_TOKEN"));
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

[[nodiscard]] QString SetupMessage() {
	return QStringLiteral(
		"Newgram sidecar not configured.\n\n"
		"Run in a terminal:\n"
		"  cd newgram/sidecar\n"
		"  NEWGRAM_SIDECAR_TOKEN=demo \\\n"
		"      uv run newgram-sidecar --port 8765 --token demo\n\n"
		"Then relaunch Telegram with:\n"
		"  NEWGRAM_SIDECAR_URL=http://127.0.0.1:8765 \\\n"
		"  NEWGRAM_SIDECAR_TOKEN=demo \\\n"
		"      open -a Telegram.app");
}

} // namespace

void ShowSummarizeChatPlaceholder(
		Window::SessionController *controller,
		PeerData *peer) {
	if (!controller || !peer) {
		return;
	}

	const auto baseUrl = ResolveBaseUrl();
	if (baseUrl.isEmpty()) {
		controller->show(Ui::MakeInformBox(SetupMessage()));
		return;
	}

	// lib_webview can fail to initialize on some macOS configurations
	// (missing WKWebView framework entitlements, etc.). Guard before we
	// construct anything that tries to navigate.
	const auto available = Webview::Availability();
	if (available.error != Webview::Available::Error::None) {
		controller->show(Ui::MakeInformBox(
			QStringLiteral("Newgram webview unavailable on this system.")));
		return;
	}

	auto storageId = controller->session().local().resolveStorageIdOther();
	const auto title = QStringLiteral("Summarize %1").arg(peer->name());
	const auto chatId = QString::number(peer->id.value);
	const auto url = BuildUiUrl(
		baseUrl,
		ResolveToken(),
		QStringLiteral("summarize_chat"),
		chatId,
		title);

	controller->show(Box([title, url, storageId = std::move(storageId)](
			not_null<Ui::GenericBox*> box) mutable {
		box->setTitle(rpl::single(title));
		box->setNoContentMargin(true);
		box->setWidth(kPanelWidth);

		const auto holder = box->addRow(
			object_ptr<WebviewHolder>(box, std::move(storageId), url),
			QMargins());
		holder->resize(kPanelWidth, kPanelHeight);

		box->addButton(
			rpl::single(QStringLiteral("Close")),
			[=] { box->closeBox(); });
	}));
}

} // namespace Newgram
