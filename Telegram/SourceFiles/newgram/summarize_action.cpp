/*
This file is part of Newgram, a fork of Telegram Desktop.

Newgram is licensed under the terms of the GPLv3; see LICENSE at repo root.
*/
#include "newgram/summarize_action.h"

#include "data/data_peer.h"
#include "ui/boxes/confirm_box.h"
#include "ui/layers/generic_box.h"
#include "window/window_session_controller.h"

#include <QDebug>

namespace Newgram {

void ShowSummarizeChatPlaceholder(
		Window::SessionController *controller,
		PeerData *peer) {
	if (!controller || !peer) {
		return;
	}
	const auto peerName = peer->name();
	qDebug().noquote() << "[newgram] Summarize chat action triggered for peer:"
		<< peerName << "id:" << peer->id.value;

	controller->show(Box([peerName](not_null<Ui::GenericBox*> box) {
		box->setTitle(rpl::single(QString("Summarize chat")));
		const auto message = QString(
			"Newgram would summarize \"%1\" here.\n\n"
			"The sidecar task (`summarize_chat`) is already wired on the "
			"Python side; hooking it up to stream results into this box is "
			"the next commit on the `newgram` branch.")
				.arg(peerName);
		Ui::ConfirmBox(box, {
			.text = message,
			.confirmText = rpl::single(QString("OK")),
			.confirmed = [](Fn<void()> &&close) { close(); },
		});
	}));
}

} // namespace Newgram
