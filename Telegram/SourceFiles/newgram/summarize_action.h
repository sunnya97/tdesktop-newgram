/*
This file is part of Newgram, a fork of Telegram Desktop.

Newgram is licensed under the terms of the GPLv3; see LICENSE at repo root.
*/
#pragma once

class PeerData;

namespace Window {
class SessionController;
} // namespace Window

namespace Newgram {

// Entry point for the "Summarize chat" peer-menu action. For Phase 0 this
// opens an informational box announcing the task — the actual SidecarClient
// invocation + streaming UI is wired in a follow-up commit once the build
// verifies green through CI. Keeping the placeholder minimal so the menu
// wiring (which is harder to revert) can be validated in isolation.
void ShowSummarizeChatPlaceholder(
	Window::SessionController *controller,
	PeerData *peer);

} // namespace Newgram
