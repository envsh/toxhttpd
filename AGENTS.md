# Build commands

- Go server: `bash build.sh` (from repo root) — uses old glibc 2.17 toolchain, **do not modify** `build.sh`
- q3tox Qt3: `cd q3tox && bash buildqt3.sh` — generates `q3tox`
- q3tox Qt4: `cd q3tox && bash buildqt4.sh` — generates `q4tox`
- Run server: `./go-toxhttpd/toxhttpd-go 8181`
- Run client: `./q3tox/q3tox`

# Architecture

- **Go server** (`go-toxhttpd/`): `server.go` is the real entrypoint (1848 lines, all logic). `main.go` is just `package main` (2 lines). Web static files live in `web/` at repo root.
- **q3tox client** (`q3tox/`): `mainwindow.cpp` is the hub. `chatview.cpp` replaced QTextEdit with a virtualized `QWidget+QScrollBar` (see ChatView class). `api.cpp` wraps REST calls via libcurl.
- **go-toxcore-c** is a local fork with NGC support (`groupchat.go` = NGC Group*, `group.go` = legacy Conference*). `go.mod` replaces `github.com/TokTok/go-toxcore-c`.

# Qt3/Qt4 compatibility

- `compat34.h` handles API diffs (QString, layout, events, files). Use it.
- `.pro` file auto-detects Qt version: sets `QT3_BUILD` for Qt3 (no `QT_VERSION`), no define for Qt4.
- Qt3 moc chokes on `#ifdef` inside class body. Affected classes use `QWidget` base + manual `QScrollBar` (see `ChatView`).
- `MOC_DIR = .` and `OBJECTS_DIR = .` — generated moc/source files land in source dir.

# Important constraints

- Peer name cache key: `"conference_{id}_{peer}"` / `"group_{id}_{peer}"` — web (`app.js`) and q3tox (`mainwindow.cpp`) must stay in sync.
- Event system: long-poll `/api/events`. q3tox uses `QThread` + libcurl, events delivered via `CustomEventBase` (compat wrapper).
- Translation: nested JSON in `lang/*.json`, loaded by `Translator` class (`_("key")`), supports dot-path (e.g. `"statuses.online"`).
- ChatView scroll multiplier: `wheelEvent` uses `step * N`, adjusted for feel (currently `*5`).
