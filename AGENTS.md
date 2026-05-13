# Build commands

- Go server: `bash build.sh` (from repo root) — uses old glibc 2.17 toolchain, **do not modify** `build.sh`
- qltox Qt3: `cd qltox && bash buildqt3.sh` — generates `qltox`
- qltox Qt4: `cd qltox && bash buildqt4.sh` — generates `q4tox`
- Run server: `./go-toxhttpd/toxhttpd-go 8181`
- Run client: `./qltox/qltox`
- Build qltox cpp: `cd qltox && make -j1`

# Architecture

- **Go server** (`go-toxhttpd/`): `server.go` is the real entrypoint (1848 lines, all logic). `main.go` is just `package main` (2 lines). Web static files live in `web/` at repo root.
- **qltox client** (`qltox/`): `mainwindow.cpp` is the hub. `chatview.cpp` replaced QTextEdit with a virtualized `QWidget+QScrollBar` (see ChatView class). `api.cpp` wraps REST calls via libcurl.
- **go-toxcore-c** is a local fork with NGC support (`groupchat.go` = NGC Group*, `group.go` = legacy Conference*). `go.mod` replaces `github.com/TokTok/go-toxcore-c`.

# Qt3/Qt4 compatibility

- `compat34.h` handles API diffs (QString, layout, events, files). Use it.
- `.pro` file auto-detects Qt version: sets `QT3_BUILD` for Qt3 (no `QT_VERSION`), no define for Qt4.
- Qt3 moc chokes on `#ifdef` inside class body. When `#ifdef QT3_BUILD`/`#else` changes the **base class** inside the class body, Qt3 3.5.0 moc v26 processes BOTH branches, generating duplicate moc code (redefinition errors). Fix: never put `Q_OBJECT` inside an `#ifdef` block; if the class has no custom signals/slots, remove `Q_OBJECT` entirely. For different base classes, define the base via a macro OUTSIDE the class body (`#define LIME_BASE QStyle` / `#else` / `#define LIME_BASE QProxyStyle`), but note that moc always sees the `#else` branch (no `-DQT3_BUILD`), so the moc file will reference the Qt4 base — only safe when `Q_OBJECT` is omitted. Affected classes use `QWidget` base + manual `QScrollBar` (see `ChatView`), or no `Q_OBJECT` (see `LimeStyle`).
- `MOC_DIR = .` and `OBJECTS_DIR = .` — generated moc/source files land in source dir.

# Important constraints

- **Never auto-commit to git.** Only commit when explicitly asked by the user.

- Peer name cache key: `"conference_{id}_{peer}"` / `"group_{id}_{peer}"` — web (`app.js`) and qltox (`mainwindow.cpp`) must stay in sync.
- Event system: long-poll `/api/events`. qltox uses `QThread` + libcurl, events delivered via `CustomEventBase` (compat wrapper).
- Translation: nested JSON in `lang/*.json`, loaded by `Translator` class (`_("key")`), supports dot-path (e.g. `"statuses.online"`).
- ChatView scroll multiplier: `wheelEvent` uses `step * N`, adjusted for feel (currently `*5`).
- **C++ 代码不要抛出异常，也不要 try-catch**。Qt3 编译环境可能未启用异常支持，且项目风格不依赖异常处理。所有错误通过返回值或结构体（如 `TranslateApiResult`）传递。
- **emoji 不要使用 `\x` 转义格式**（如 `"\xF0\x9F\x93\x8B"`），直接用原始 UTF-8 字符（如 `"📋"`）。源代码文件本身是 UTF-8 编码，字符串字面量中的 emoji 自然包含正确字节序列。

# Minimize HTTP requests

- Use cached data over extra HTTP calls. Available caches:
  - Web: `contacts.groups[]` / `contacts.friends[]` / `contacts.conferences[]` (global), `peerInfoMap`, `selfInfo` (own profile), `friendNameMap`
  - qltox: `ContactListWidget` item text / `allContacts`, `peerInfoMap`, `friendNameMap`, `SelfInfoWidget::selfName()`
