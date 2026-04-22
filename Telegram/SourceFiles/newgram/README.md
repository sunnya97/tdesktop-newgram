# `newgram/` — AI bridge module

Thin C++ layer that connects the tdesktop UI to the Newgram agent-host
sidecar. Everything here is additive: the module can be dropped in without
modifying any existing tdesktop file other than `Telegram/CMakeLists.txt`
(which grows a handful of new source entries).

## Files

| File                       | Purpose                                                              |
| -------------------------- | -------------------------------------------------------------------- |
| `newgram_types.{h,cpp}`    | `AgentEvent` tagged union mirroring the sidecar's Python types.      |
| `sidecar_process.{h,cpp}`  | `QProcess` lifecycle: auto-spawn sidecar, parse handshake, restart.  |
| `sidecar_client.{h,cpp}`   | `QNetworkAccessManager` + SSE parser; issues task streams.           |
| `sidecar_settings.{h,cpp}` | Runtime config resolution (env + tdesktop data dir).                 |

## Wiring (Phase 0 bootstrap path)

The module is currently self-contained — it builds into the `Telegram`
target but isn't instantiated anywhere. Two small additions light it up:

1. **Startup** — in whichever `Application`/`Core` setup file handles "one
   global singleton per app", construct a `Newgram::SidecarProcess`, call
   `start()`, and on its `ready` signal configure a `Newgram::SidecarClient`.
   Keep the singleton alive for the lifetime of the app.

2. **UI entry point** — the bootstrap feature is "Summarize chat". Append a
   new `Filler::addSummarizeChat()` method in
   `window/window_peer_menu.cpp`, modelled on `addClearHistory()`:
   ```cpp
   void Filler::addSummarizeChat() {
       _addAction(QStringLiteral("Summarize chat"),
           [peer = _peer] { Newgram::ShowSummarizeDialog(peer); },
           &st::menuIconSearch);
   }
   ```
   `ShowSummarizeDialog` (TBD — new file here) opens a modal, calls
   `SidecarClient::startTask("summarize_chat", {"chat_id": peer->id})`, and
   renders the streaming `AgentEvent`s.

Both steps deliberately land in a later commit so this Phase-0 bootstrap
can be verified to compile + link without touching the upstream UI.

## Integration constraints

- Settings should eventually surface via the tdesktop `Core::Settings`
  serialization pipeline, not env vars. For Phase 0 the env path keeps the
  patch surface small.
- The sidecar handshake line format is stable API; see
  `sidecar/newgram_sidecar/__main__.py`.
- The sidecar is a managed child process of the fork. Its stderr is
  forwarded via `SidecarProcess::logLine` — route that into the tdesktop
  log system (`LOG(...)`) once the module is actually instantiated.
