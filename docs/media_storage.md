# SQLite 媒体消息存储设计

## 1. 概述

### 1.1 目标

- 消息历史持久化：重启客户端后聊天记录不丢失
- 媒体文件缓存：图片、GIF、视频、文件下载后本地缓存
- 离线访问：已加载的消息和媒体离线可看
- 内存效率：只保留当前可见的消息在内存，历史消息按需从 SQLite 加载
- TG 级性能：write-behind 批量写入、WAL 并发读、LRU 自动驱逐
- 缓存有上限：默认 cache.db ≤200MB，超出自动 LRU 驱逐至 80%

### 1.2 非目标

- 加密（以后再说）
- 多账号隔离（只支持单账号）
- 服务端消息同步（Go 端 events 表已有，保持不变）
- 端到端同步（客户端本地存储，不强求跨设备）

### 1.3 现状

| 当前 | 问题 |
|------|------|
| `ChatElement` 全在内存 `std::vector<ChatElement>` | 重启丢失，聊天记录不保留 |
| `AvatarManager` 内存缓存 | session 结束后丢失 |
| 图片/文件下载后只存 `QPixmap` 临时对象 | 下次打开需重新下载 |
| 无任何 SQLite 依赖 | 需要新增 |

### 1.4 ChatElement 已有字段

```cpp
struct ChatElement {
    ElementType etype;          // Text, Image, File, Video, Gif, Audio
    QString senderName;
    QString senderNickname;
    int     peerNumber;
    QString avatarUrl;
    QString time;
    QString category;           // "self" / "other"
    QString messageText;
    // Translation...
    // Media fields
    QPixmap thumbnail;          // 缩略图（内存缓存，不存 SQLite）
    QPixmap scaledDisplay;      // 预缩放到显示尺寸的缓存（内存）
    QString caption;
    QString mediaUrl;           // MXC URL
    int mediaWidth;
    int mediaHeight;
    QString fileName;
    int fileSize;
    int progress;
    QString localPath;          // 下载到本地的路径
    int durationSec;
    QString gifPath;
    QMovie* movie;
    // Layout cache
    short cachedWidth;
    short height;
};
```

### 1.5 存储上限总览

| 数据库 | 默认上限 | 驱逐策略 | 用户可调 |
|--------|---------|---------|---------|
| message.db | 无上限 | ❌ 永不自动删除 | ❌ |
| cache.db | 200 MB | LRU 超出后驱逐至 80% | ✅ 设置页"缓存上限"滑块 |

cache.db 上限选取理由：
- 主流聊天场景：按每天 50 条图片/表情，约 5MB/天 → 200MB ≈ 40 天会话
- 与系统磁盘空间占比：200MB 对现代设备极小，无需用户操心
- TG 桌面端默认 512MB，我们初次保守取 200MB，后续可调

超出上限行为：
- 每次写入缓存后检查 `getTotalCacheSize()` > 上限
- 触发 LRU 驱逐直到降至上限的 80%
- 驱逐顺序：超 30 天未访问 → 超 1 天且单文件 > 2MB → 最久未访问

## 2. 整体架构

### 2.1 双库架构（简化版）

```
~/.config/toxhttpd/
└── message.db          # 消息历史 + 媒体引用 + 草稿 + 收藏 + 发送队列 (WAL 模式)

~/.cache/toxhttpd/
└── cache.db            # 文件缓存 (≤30MB inline BLOB + file_refs, WAL 模式)
```

为什么要两个库：
- **message.db**：结构化数据，需要强一致性、外键、事务
- **cache.db**：缓存数据，可丢失，重点是快速读写、LRU 驱逐

### 2.2 文件归类示意图

```
收到消息（含媒体）
    │
    ├── 纯文本 ──→ message.db.messages (data JSON)
    │
    ├── 图片/小文件 (≤1MB)
    │   ├── thumbnail ──→ cache.db (key=thumb_<mxc_hash>)
    │   └── full ──→ cache.db (key=file_<mxc_hash>)
    │
    ├── GIF/视频 (1MB–30MB)
    │   ├── thumbnail ──→ cache.db
    │   └── full ──→ cache.db (key=file_<hash>, >1MB 走 file_refs 路径索引)
    │
    └── avatar ──→ cache.db (key=avatar_<mxc_hash>)
```

### 2.3 各层职责

```
Storage 类 (qltox)
    │
    ├── 消息读写: appendMessage, loadMessages, deleteMessage
    ├── 媒体缓存: storeMedia, loadMedia, evictMedia
    ├── 草稿读写: saveDraft, loadDraft
    ├── 统计: stats() → (tag → {size, count})
    └── 维护: vacuum, checkpoint, integrityCheck
        │
        ├── message.db ──→ SQLite (消息)
        └── cache.db   ──→ SQLite + BLOB + file_refs (缓存)
```

## 3. SQLite 全局优化

### 3.1 Pragma 设置

所有三个库使用相同的 pragma：

```sql
PRAGMA journal_mode = WAL;            -- Write-Ahead Log: 读不阻塞写
PRAGMA synchronous = NORMAL;          -- WAL 模式下 NORMAL 够安全
PRAGMA page_size = 4096;              -- 4KB 页，兼顾小文件 BLOB
PRAGMA cache_size = -64000;           -- 64MB 内存缓存
PRAGMA temp_store = MEMORY;           -- 临时表在内存
PRAGMA mmap_size = 268435456;         -- 256MB 内存映射
PRAGMA busy_timeout = 5000;           -- 等待锁超时 5 秒
PRAGMA foreign_keys = ON;             -- 外键约束（message.db）
```

### 3.2 WAL 模式详解

| 特性 | 说明 |
|------|------|
| 并发读 | 多个 reader 同时读，不阻塞 |
| 写保护 | 同一时间只有一条写入 |
| 持久性 | `synchronous=NORMAL` 时，应用崩溃不丢已提交数据 |
| 回滚 | NORMAL 模式在系统崩溃时可能回滚最后几条，但不会损坏数据 |

### 3.3 连接管理

```cpp
class Database {
    sqlite3* m_db;
    QMutex m_mutex;  // 写锁（WAL 模式不需要读锁）

    // 写操作
    int exec(const char* sql);                    // 简单执行
    int execMany(const char** sql, int count);    // 批量执行

    // 事务
    int begin();
    int commit();
    int rollback();

    // 预处理语句
    sqlite3_stmt* prepare(const char* sql);
    int bindText(sqlite3_stmt*, int col, const QString& val);
    int bindInt64(sqlite3_stmt*, int col, int64_t val);
    int bindBlob(sqlite3_stmt*, int col, const QByteArray& val);
    int bindNull(sqlite3_stmt*, int col);
    int step(sqlite3_stmt*);
    void finalize(sqlite3_stmt*);
};
```

### 3.4 WAL Checkpoint

WAL 文件在持续写入后会不断增长。定期执行 checkpoint 可以控制 WAL 大小：

```sql
PRAGMA wal_checkpoint(TRUNCATE);
```

在 Storage 维护定时器（每 5 分钟）中执行：

```cpp
void Storage::checkpoint() {
    for (auto* db : {m_messageDb, m_cacheDb})
        db->exec("PRAGMA wal_checkpoint(TRUNCATE)");
}
```

### 3.5 线程模型（TG 方案）

参考 Telegram Desktop 的 `internal::Database` + Postbox 的 single-writer-queue 架构。

#### 3.5.1 设计原则

| 原则 | 说明 |
|------|------|
| 单写者队列 | 所有写操作串行在一条专用线程上，消除写冲突 |
| WAL 并发读 | UI 线程可直接读（WAL 模式读不阻塞写） |
| Write-behind | 内存累积变更，commit 时一次性批量写入 |
| 写不阻塞 UI | 写请求投递到队列后立即返回，结果通过信号/回调通知 |

#### 3.5.2 架构图

```
┌─────────────────────────────────────────────────┐
│  UI Thread (MainWindow)                          │
│                                                   │
│  读: sqlite3_prepare + step + finalize            │
│      (直接读 DB，WAL 不阻塞)                       │
│                                                   │
│  写: Storage::postWrite([]{ db->exec(...); })     │
│      └──→ 投递到写队列，立即返回                    │
└──────────────────┬──────────────────────────────┘
                   │ 写请求队列（线程安全）
                   ▼
┌─────────────────────────────────────────────────┐
│  Writer Thread (StorageWorker)                   │
│                                                   │
│  while (true) {                                   │
│    auto task = queue.pop();  // 阻塞等待           │
│    task();                   // 执行 SQL          │
│    notify(result);           // 回调 UI 线程       │
│  }                                                 │
│                                                   │
│  Write-behind: 每 N 条或每 100ms flush 一次事务    │
└─────────────────────────────────────────────────┘
```

#### 3.5.3 Storage 类设计

```cpp
class Storage : public QObject {
    Q_OBJECT
public:
    static Storage& instance();

    bool init(const QString& dbPath);
    void close();

    // 读操作（UI 线程直接调用）
    TranslationRecord getTranslation(int64_t msgRowid, const QString& lang);
    bool hasTranslation(int64_t msgRowid, const QString& lang);

    // 写操作（投递到写线程）
    void setTranslationAsync(int64_t msgRowid, const QString& lang,
                             const QString& text, ...);
    void clearLangTranslationsAsync(const QString& lang);

signals:
    void translationStored(int64_t msgRowid, const QString& lang);

private:
    void writerThreadLoop();            // 写线程主循环
    void flushWriteBatch();             // write-behind flush

    sqlite3* m_readDb;                  // 只读连接（UI 线程）
    sqlite3* m_writeDb;                 // 只写连接（写线程）
    QThread* m_writerThread;
    QMutex m_queueMutex;
    QWaitCondition m_queueCond;
    std::vector<std::function<void()>> m_writeQueue;
};
```

> **为什么双连接**: SQLite 同一文件可用多个连接。读连接 `SQLITE_OPEN_READONLY`，写连接 `SQLITE_OPEN_READWRITE`。WAL 模式下，读连接不阻塞写连接，写连接不阻塞读连接。

#### 3.5.4 Read-Write 分离示例

```cpp
// UI 线程：读翻译缓存（直接查，不阻塞）
TranslationRecord Storage::getTranslation(int64_t msgRowid, const QString& lang) {
    auto stmt = prepare(m_readDb,
        "SELECT translated_text, source_lang FROM translations "
        "WHERE message_rowid=?1 AND target_lang=?2");
    bindInt64(stmt, 1, msgRowid);
    bindText(stmt, 2, lang);
    if (step(stmt) == SQLITE_ROW) {
        return { msgRowid, lang, columnText(stmt, 0), columnText(stmt, 1) };
    }
    return {};
}

// UI 线程：投递写请求（立即返回）
void Storage::setTranslationAsync(...) {
    QMutexLocker lock(&m_queueMutex);
    m_writeQueue.push_back([=] {
        auto stmt = prepare(m_writeDb,
            "INSERT OR REPLACE INTO translations "
            "(message_rowid, target_lang, translated_text, source_lang) "
            "VALUES (?1, ?2, ?3, ?4)");
        bindInt64(stmt, 1, msgRowid);
        bindText(stmt, 2, lang);
        bindText(stmt, 3, text);
        bindTextOrNull(stmt, 4, sourceLang);
        step(stmt);
    });
    m_queueCond.wakeOne();
}
```

#### 3.5.5 Write-behind 批处理

累积少量写入后在事务中批量提交，避免每条 INSERT 单独开事务：

```cpp
void Storage::flushWriteBatch() {
    std::vector<std::function<void()>> batch;
    {
        QMutexLocker lock(&m_queueMutex);
        batch.swap(m_writeQueue);  // 取出所有待处理任务
    }
    if (batch.empty()) return;

    sqlite3_exec(m_writeDb, "BEGIN IMMEDIATE", nullptr, nullptr, nullptr);
    for (auto& task : batch) task();
    sqlite3_exec(m_writeDb, "COMMIT", nullptr, nullptr, nullptr);
}
```

触发策略：
- 每收到 N 条写请求（例如 N=16）
- 或每 100ms 定时器
- 或 writer thread 空闲时立即 flush

#### 3.5.6 translations 表的适用性

| 操作 | 线程 | 原因 |
|------|------|------|
| `SELECT` 翻译 | UI 线程直读 | PK 查找 < 0.1ms，WAL 不阻塞 |
| `INSERT` 翻译 | 投递写队列 | 保证写串行，UI 零等待 |
| 批量清空 | 投递写队列 | 写队列串行执行 |
| 切换语种 | 投递写队列 | DELETE 在写线程执行 |

#### 3.5.7 与 TG Desktop 的差异

| 项目 | TG Desktop | qltox |
|------|-----------|-------|
| 写队列 | 显式 single writer queue | 同上 |
| WAL | 是 | 是 |
| Write-behind | Storage::Local::beforeCommit() | flushWriteBatch() + 定时器 |
| 双连接 | 读/写分离（SQLCipher） | 读/写分离 |
| 连接池 | 无（单连接读+单连接写） | 同上 |
| 加密 | SQLCipher 256-bit AES | 无加密（tox 协议无要求） |

## 4. message.db 表结构

### 4.1 建表 SQL

```sql
-- 会话元数据（联系人/群聊/会议）
CREATE TABLE IF NOT EXISTS channels (
    chanid             TEXT PRIMARY KEY,   -- "friend:0", "group:1", "conference:2"
    proto_type         TEXT DEFAULT 'tox', -- "tox", "matrix"
    last_message_rowid INTEGER NOT NULL DEFAULT 0,
    last_read_rowid    INTEGER NOT NULL DEFAULT 0,
    unread_count       INTEGER DEFAULT 0,
    pinned_order       INTEGER DEFAULT 0,  -- 0=未置顶, >0=置顶排序
    draft_text         TEXT DEFAULT '',
    muted              INTEGER DEFAULT 0,
    created_at         TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- 消息表
CREATE TABLE IF NOT EXISTS messages (
    rowid       INTEGER PRIMARY KEY AUTOINCREMENT,
    event_id    TEXT UNIQUE,                 -- Matrix $event_id，用于去重
    chanid      TEXT NOT NULL REFERENCES channels(chanid),
    -- 消息主体（纯文本 JSON，对同步透明）
    data        TEXT NOT NULL,
    -- 媒体字段（反范式化，避免 JOIN）
    etype       INTEGER DEFAULT 0,          -- 0=text, 1=image, 2=file, 3=video, 4=gif, 5=audio
    sender_name TEXT DEFAULT '',
    sender_nick TEXT DEFAULT '',
    peer_number INTEGER DEFAULT -1,
    sender_pubkey TEXT DEFAULT '',           -- 发送者公钥（签名验证身份）
    signature   TEXT DEFAULT '',             -- 消息签名
    avatar_url  TEXT DEFAULT '',
    time_text   TEXT DEFAULT '',
    ip_address  TEXT DEFAULT '',
    category    TEXT DEFAULT '',
    caption     TEXT DEFAULT '',
    media_url   TEXT,                        -- MXC URL
    media_mime  TEXT,                        -- "image/png" / "video/mp4"
    media_width INTEGER DEFAULT 0,
    media_height INTEGER DEFAULT 0,
    file_name   TEXT,                        -- "photo.jpg"
    file_size   INTEGER DEFAULT 0,          -- 字节数
    duration_sec INTEGER DEFAULT 0,         -- 视频/音频时长
    local_path  TEXT,                        -- 已下载到本地的完整路径
    gif_path    TEXT DEFAULT '',
    thumbnail_key TEXT,                      -- cache.db 中的缩略图 key
    cache_tag   INTEGER DEFAULT 0,          -- 0=none, 1=avatar, 2=image, 3=gif, 4=video, 5=file
    send_state  INTEGER DEFAULT 0,          -- 0=unknown, 1=sent, 2=delivered, 3=read, 4=failed
    reply_to_rowid INTEGER DEFAULT 0,       -- 回复目标 rowid, 0=无
    edited      INTEGER DEFAULT 0,
    deleted_at  TIMESTAMP,
    forwarded_from TEXT DEFAULT '',
    mention     INTEGER DEFAULT 0,
    created_at  TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- messages 索引
CREATE INDEX IF NOT EXISTS idx_messages_chanid
    ON messages(chanid, rowid DESC);
CREATE INDEX IF NOT EXISTS idx_messages_media_url
    ON messages(media_url) WHERE media_url IS NOT NULL;
CREATE INDEX IF NOT EXISTS idx_messages_send_state
    ON messages(chanid, send_state);

-- trigram tokenizer，同时支持 CJK 和拉丁（需 SQLite ≥ 3.34.0）
CREATE VIRTUAL TABLE IF NOT EXISTS messages_fts USING fts5(
    content,
    tokenize='trigram case_sensitive 0',
    content='messages',
    content_rowid='rowid'
);

CREATE TRIGGER IF NOT EXISTS messages_fts_insert
AFTER INSERT ON messages BEGIN
    INSERT INTO messages_fts(rowid, content)
    VALUES (NEW.rowid, NEW.data);
END;

CREATE TRIGGER IF NOT EXISTS messages_fts_delete
AFTER DELETE ON messages BEGIN
    INSERT INTO messages_fts(messages_fts, rowid, content)
    VALUES('delete', OLD.rowid, OLD.content);
END;

CREATE TRIGGER IF NOT EXISTS messages_fts_update
AFTER UPDATE ON messages BEGIN
    INSERT INTO messages_fts(messages_fts, rowid, content)
    VALUES('delete', OLD.rowid, OLD.content);
    INSERT INTO messages_fts(rowid, content)
    VALUES (NEW.rowid, NEW.data);
END;

-- 参与者信息缓存
CREATE TABLE IF NOT EXISTS peers (
    chanid        TEXT NOT NULL,
    peer_number   INTEGER NOT NULL,
    public_key    TEXT DEFAULT '',
    name          TEXT DEFAULT '',
    nickname      TEXT DEFAULT '',
    avatar_url    TEXT DEFAULT '',
    status_text   TEXT DEFAULT '',
    status_str    TEXT DEFAULT '',
    user_status   TEXT DEFAULT '',
    peer_ip       TEXT DEFAULT '',
    role          INTEGER DEFAULT 0,
    role_str      TEXT DEFAULT '',
    is_self       INTEGER DEFAULT 0,
    last_seen     INTEGER DEFAULT 0,
    status        INTEGER DEFAULT 0,
    updated_at    TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (chanid, peer_number)
);

-- 收藏表
CREATE TABLE IF NOT EXISTS bookmarks (
    id             INTEGER PRIMARY KEY AUTOINCREMENT,
    message_rowid  INTEGER NOT NULL,
    chanid         TEXT NOT NULL REFERENCES channels(chanid),
    note           TEXT DEFAULT '',
    tag            TEXT DEFAULT '',
    created_at     TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE(message_rowid)
);

-- 发送队列（待发送/发送中/失败）
CREATE TABLE IF NOT EXISTS pending_messages (
    id             INTEGER PRIMARY KEY AUTOINCREMENT,
    chanid         TEXT NOT NULL,
    peer_number    INTEGER DEFAULT -1,
    data           TEXT NOT NULL,
    etype          INTEGER DEFAULT 0,
    message_text   TEXT DEFAULT '',
    media_url      TEXT DEFAULT '',
    file_name      TEXT DEFAULT '',
    file_size      INTEGER DEFAULT 0,
    retry_count    INTEGER DEFAULT 0,
    max_retries    INTEGER DEFAULT 3,
    last_error     TEXT DEFAULT '',
    status         INTEGER DEFAULT 0,    -- 0=pending, 1=in_flight, 2=failed
    created_at     TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
CREATE INDEX IF NOT EXISTS idx_pending_status ON pending_messages(status);

-- 消息表情回应
CREATE TABLE IF NOT EXISTS reactions (
    message_rowid  INTEGER NOT NULL REFERENCES messages(rowid),
    emoji          TEXT NOT NULL,
    sender_name    TEXT DEFAULT '',
    created_at     TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (message_rowid, emoji, sender_name)
);
CREATE INDEX IF NOT EXISTS idx_reactions_message ON reactions(message_rowid);

-- 消息翻译缓存
CREATE TABLE IF NOT EXISTS translations (
    message_rowid    INTEGER NOT NULL REFERENCES messages(rowid) ON DELETE CASCADE,
    target_lang      TEXT NOT NULL,           -- "zh-CN", "en-US"
    translated_text  TEXT NOT NULL,           -- 翻译后的纯文本
    translated_entities TEXT,                 -- 可选：样式实体 JSON（TG entity 模型）
    source_lang      TEXT,                    -- 检测到的源语种
    provider         TEXT DEFAULT 'builtin',  -- 翻译提供方
    created_at       TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (message_rowid, target_lang)
);

-- Schema 版本表（用于迁移）
CREATE TABLE IF NOT EXISTS schema_version (
    version     INTEGER PRIMARY KEY,
    applied_at  TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
INSERT OR IGNORE INTO schema_version(version) VALUES(3);
```

> pending 消息不混入历史，由 ChatView 在底部单独渲染（类似 Telegram 输入框上方的"等待中"区域）。发送成功后转为 messages 并追加到历史，发送失败保留在 pending_messages 表并标记 status=2。

### 4.2 消息 data 字段设计

`data` 字段存储 JSON 格式（与 Go 端 events 表一致），对同步逻辑透明：

```json
// 文本消息
{
    "type": "friend_message",
    "friend_id": 0,
    "direction": "received",
    "message": "hello"
}

// 图片消息
{
    "type": "friend_message",
    "friend_id": 0,
    "direction": "received",
    "message": "",
    "media_url": "mxc://example.org/abc123",
    "media_mime": "image/png",
    "width": 800,
    "height": 600
}

// 文件消息
{
    "type": "group_message",
    "group_number": 1,
    "peer_number": 2,
    "direction": "received",
    "message": "see attached",
    "media_url": "mxc://example.org/def456",
    "file_name": "document.pdf",
    "file_size": 1024000
}
```

### 4.3 索引策略

| 索引 | 用途 |
|------|------|
| `idx_messages_chanid(chanid, rowid DESC)` | 按通道加载消息历史（分页查询） |
| `idx_messages_media_url(media_url) WHERE media_url IS NOT NULL` | 按媒体 URL 查询（去重/检查已缓存） |
| `idx_messages_send_state(chanid, send_state)` | 按发送状态过滤（如查找失败消息） |
| `idx_reactions_message(message_rowid)` | 按消息查询表情回应 |
| `channels(chanid)` (PK 自带) | 按 chanid 查会话元数据 |
| `channels(pinned_order)` | 获取置顶排序列表 |
| `messages_fts` (trigram FTS5) | 全文搜索，支持 CJK 和拉丁 |
| `translations(message_rowid)` (PK 自带) | 按消息查询所有语种的翻译 |

### 4.4 查询示例

```sql
-- 加载最近 50 条消息
SELECT * FROM messages
WHERE chanid = 'friend:0'
ORDER BY rowid DESC
LIMIT 50;

-- 加载更早的消息（滚动翻页）
SELECT * FROM messages
WHERE chanid = 'friend:0' AND rowid < 10000
ORDER BY rowid DESC
LIMIT 50;

-- 加载新消息（下拉刷新）
SELECT * FROM messages
WHERE chanid = 'friend:0' AND rowid > 10100
ORDER BY rowid ASC;

-- 获取某个 channel 更早的一条消息（用于恢复滚动位置）
SELECT * FROM messages
WHERE chanid = 'friend:0' AND rowid = 9999;

-- 按发送状态过滤（查找发送失败的消息）
SELECT * FROM messages
WHERE chanid = 'friend:0' AND send_state = 4
ORDER BY rowid DESC;

-- 获取某个会话未读数
SELECT COUNT(*) FROM messages
WHERE chanid = 'friend:0' AND send_state != 0 AND rowid > (
    SELECT COALESCE(last_read_rowid, 0) FROM channels WHERE chanid = 'friend:0'
);

-- 获取置顶联系人列表
SELECT chanid, pinned_order FROM channels
WHERE pinned_order > 0
ORDER BY pinned_order ASC;

-- 全文搜索消息
SELECT rowid, * FROM messages
WHERE rowid IN (
    SELECT rowid FROM messages_fts
    WHERE messages_fts MATCH '搜索关键词'
)
ORDER BY rowid DESC LIMIT 50;

-- 写入翻译缓存
INSERT OR REPLACE INTO translations
    (message_rowid, target_lang, translated_text, translated_entities, source_lang, provider)
VALUES (12345, 'zh-CN', '你好世界', '[{"type":"bold","offset":0,"length":2}]', 'en', 'libre');

-- 读取翻译缓存
SELECT translated_text, translated_entities, source_lang
FROM translations
WHERE message_rowid = 12345 AND target_lang = 'zh-CN';

-- 清空某个语种的所有翻译（切换目标语种时）
DELETE FROM translations WHERE target_lang = 'zh-CN';

-- 清空所有翻译缓存
DELETE FROM translations;
```

## 5. cache.db 表结构

### 5.1 建表 SQL

```sql
-- 文件缓存表（≤1MB 的小文件）
CREATE TABLE IF NOT EXISTS cache (
    key         TEXT PRIMARY KEY,       -- "avatar_<hash>", "thumb_<hash>", "file_<hash>"
    data        BLOB NOT NULL,          -- 文件二进制
    mime_type   TEXT DEFAULT '',
    tag         INTEGER DEFAULT 0,      -- kImageCacheTag=0x01 等
    access_time INTEGER NOT NULL,        -- Unix timestamp, LRU 驱逐用
    size        INTEGER NOT NULL,        -- 文件大小（字节）
    created_at  TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- 中型文件路径表（1MB–30MB，文件本身在 filesystem）
CREATE TABLE IF NOT EXISTS file_refs (
    key         TEXT PRIMARY KEY,       -- "file_<hash>"
    file_path   TEXT NOT NULL,          -- "~/.cache/toxhttpd/files/<hash>.ext"
    mime_type   TEXT DEFAULT '',
    tag         INTEGER DEFAULT 0,
    access_time INTEGER NOT NULL,
    size        INTEGER NOT NULL,
    created_at  TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- 用于 LRU 驱逐的索引
CREATE INDEX IF NOT EXISTS idx_cache_tag ON cache(tag);
CREATE INDEX IF NOT EXISTS idx_cache_access ON cache(access_time);
CREATE INDEX IF NOT EXISTS idx_filerefs_tag ON file_refs(tag);
CREATE INDEX IF NOT EXISTS idx_filerefs_access ON file_refs(access_time);
```

### 5.2 Cache Tag 枚举

```cpp
// 对应 TG 的 Data::kImageCacheTag 等
enum CacheTag : uint8 {
    kAvatarCacheTag    = 0x01,   // 头像
    kImageCacheTag     = 0x02,   // 图片
    kGifCacheTag       = 0x03,   // GIF 动画
    kVideoCacheTag     = 0x04,   // 视频
    kFileCacheTag      = 0x05,   // 文档/文件
};
```

### 5.3 缓存 key 规范

key 格式：`<type>_<hash>`，其中：

- type: `avatar` / `thumb` / `file`
- hash: SHA256 of MXC URL 的前 32 字符（MXC URL 本身已经内容寻址，
  但用 SHA256 作为 key 可以统一处理，长度固定）

```
avatar_a1b2c3d4e5f6...   → 头像
thumb_a1b2c3d4...         → 缩略图
file_a1b2c3d4...          → 完整文件（inline BLOB 或 file_refs 路径）
```

### 5.4 缓存大小阈值

```cpp
// TG 参考值：kMaxFileInMemory = 10MB, kUseBigFilesFrom = 30MB
// 我们的调整：
constexpr int64_t kMaxSmallFileSize    = 1 * 1024 * 1024;     // ≤1MB → cache.db inline BLOB
constexpr int64_t kMediumFileThreshold = 30 * 1024 * 1024;    // 1MB–30MB → filesystem + file_refs 路径
```

为什么用 1MB 而不是 TG 的 10MB：
- 我们的场景主要是聊天图片（通常 100KB–500KB）和头像（几 KB），1MB 够用
- BLOB 大于 1MB 会影响 SQLite 性能（每行过大导致页分裂）
- 1MB 也是 SQLite 官方推荐的内嵌 BLOB 合理上限

### 5.5 访问时间更新

每次通过 `loadMedia()` 读取缓存时，必须同步更新 `access_time` 字段，
否则 LRU 驱逐无法正确识别热点数据：

```sql
UPDATE cache SET access_time = ? WHERE key = ?;
UPDATE file_refs SET access_time = ? WHERE key = ?;
```

## 6. Write-behind Batching（Postbox 模式）

### 6.1 为什么需要 write-behind

如果每次收到消息都立刻写 SQLite：

```
收到10条消息 → 10次 INSERT → 10次 fsync → ~10ms 阻塞 → UI 卡
```

Write-behind 模式：

```
收到10条消息 → 内存列表 append → UI 立即响应
              → beforeCommit() 时一条 BEGIN + 10×INSERT + COMMIT
              → 1次 fsync → <1ms
```

### 6.2 Storage 类设计

```cpp
class Storage {
public:
    static Storage& instance();

    // 初始化
    bool init(const QString& dataDir, const QString& cacheDir);

    // ——— 消息操作（主线程调用，只改内存） ———

    // 追加消息（纯文本）
    int64_t appendMessage(const QString& chanid, const ChatElement& msg);
    // 追加消息（含媒体）
    int64_t appendMessage(const QString& chanid, const ChatElement& msg,
                          const QByteArray& thumbnailBlob);
    // 标记消息已下载（更新 localPath）
    void updateMediaPath(const QString& chanid, int64_t rowid,
                         const QString& localPath);
    // 删除消息
    void deleteMessage(const QString& chanid, int64_t rowid);

    // ——— 消息查询（直接读 DB，不走缓存） ———
    // 加载某个 channel 最近 N 条消息
    std::vector<ChatElement> loadMessages(const QString& chanid,
                                          int limit = 50);
    // 从某个 rowid 之前加载（向上翻页）
    std::vector<ChatElement> loadMessagesBefore(const QString& chanid,
                                                 int64_t beforeRowid,
                                                 int limit = 50);
    // 切换 channel 时加载
    std::vector<ChatElement> switchChannel(const QString& chanid);

    // ——— 媒体缓存操作 ———
    // 存图片到缓存
    bool storeMedia(const QString& key, const QByteArray& data,
                    const QString& mimeType, CacheTag tag);
    // 从缓存加载
    QByteArray loadMedia(const QString& key);
    // 检查是否已缓存
    bool hasMedia(const QString& key);
    // 请求下载（异步，返回 thumb 优先）
    void requestMedia(const QString& mxcUrl, CacheTag tag);

    // ——— 草稿 ———
    void saveDraft(const QString& chanid, const QString& text);
    QString loadDraft(const QString& chanid);

    // ——— 维护 ———
    void commit();               // 显式 flush
    int64_t stats(CacheTag tag); // 按 tag 统计缓存大小
    void evict(int64_t targetSize); // 手动驱逐

    // 清理
    void close();

private:
    Storage(); // singleton

    // 写队列（QThread）：
    struct PendingWrite {
        enum Type { InsertMessage, UpdateMessage, DeleteMessage,
                    StoreMedia, UpdateAccessTime, Evict, SaveDraft };
        Type type;
        // ... 泛型数据
    };
    QList<PendingWrite> m_pendingWrites;
    QTimer* m_commitTimer;
    QMutex m_pendingMutex;

    // flush 线程
    void flushPendingWrites();   // 在 QThread 中执行
    void scheduleCommit();       // 主线程调用，启动延迟 timer

    // 内部 DB helpers
    bool execQuery(const char* sql, ...);
    int64_t insertMessageRow(const QString& chanid, const ChatElement& msg,
                             const QByteArray* thumbnailBlob);
};
```

### 6.3 写入流程

```
主线程：
MainWindow::handleEvents → 收到消息
    → appendMessage(chanid, msg)
        → m_pendingWrites.append({InsertMessage, chanid, msg})
        → scheduleCommit()  // QTimer::singleShot(200ms, this, flushPendingWrites)

定时器触发：
    → flushPendingWrites()
        → beginTransaction()
        → for each pending write:
            INSERT / UPDATE / DELETE
        → commitTransaction()
        → clear pending list
```

### 6.4 读流程（直接读 DB）

```cpp
std::vector<ChatElement> Storage::loadMessages(const QString& chanid, int limit) {
    sqlite3_stmt* stmt = prepare(
        "SELECT rowid, data, etype, media_url, media_mime, "
        "media_width, media_height, file_name, file_size, "
        "duration_sec, local_path, thumbnail_key "
        "FROM messages WHERE chanid = ? "
        "ORDER BY rowid DESC LIMIT ?"
    );
    bindText(stmt, 1, chanid);
    bindInt(stmt, 2, limit);

    std::vector<ChatElement> result;
    while (step(stmt) == SQLITE_ROW) {
        ChatElement msg;
        msg.time = columnText(stmt, 0); // 从 data JSON 解析
        // ... 映射 SQLite 列到 ChatElement
        result.push_back(msg);
    }
    finalize(stmt);

    // 倒序（因为 SQL 是 DESC）
    std::reverse(result.begin(), result.end());
    return result;
}
```

### 6.5 事务合并

对于高频写入的场景（如批量收到 50 条历史消息），全部 append 后再一次性 commit：

```cpp
void Storage::appendMessageBatch(const QString& chanid,
                                  const std::vector<ChatElement>& msgs) {
    QMutexLocker lock(&m_pendingMutex);
    for (const auto& msg : msgs) {
        m_pendingWrites.append({InsertMessage, chanid, msg});
    }
    // 由后台 QTimer 触发 flush，不阻塞主线程
    scheduleCommit();
}
```

## 7. 缓存上限与清理策略

### 7.1 清理范围

| 数据库 | 自动 LRU 驱逐 | 用户手动清理 | 被动删除 |
|--------|--------------|-------------|---------|
| message.db | ❌ 永久保留 | ❌ 不提供 | ✅ 删好友/群组时 clearChannel() |
| cache.db   | ✅ 达 200MB 或 300 天 | ✅ 按 tag 选择清除 | ❌ 自动处理 |

- **自动 LRU 驱逐**：后台定时器检查上限，超出则按 access_time 删最旧文件
- **用户手动清理**：设置页提供"清除头像缓存""清除图片缓存""清除所有缓存"按钮
- **被动删除**：用户删除好友/群组时连带清空对应消息和媒体缓存

> **message.db 不受影响。** 以下驱逐逻辑仅针对 cache.db。

### 7.2 自动驱逐配置

```cpp
struct CacheSettings {
    int64_t totalSizeLimit   = 200 * 1024 * 1024;     // 200MB
    int64_t totalTimeLimit   = 30 * 24 * 60 * 60;     // 30天（秒）
};
```

### 7.3 LRU 驱逐算法

定时执行（QTimer 每 5 分钟 + 写入后检查）：

```cpp
void Storage::evictIfNeeded() {
    // cache.db 小文件驱逐
    int64_t totalSize = getTotalCacheSize();
    if (totalSize > m_settings.totalSizeLimit) {
        int64_t targetSize = m_settings.totalSizeLimit * 0.8;
        sqlite3_stmt* stmt = prepare(
            "DELETE FROM cache WHERE rowid IN ("
            "  SELECT rowid FROM cache"
            "  WHERE access_time < ? OR size >= ?"
            "  ORDER BY access_time ASC"
            "  LIMIT 100"
            ")"
        );
        bindInt64(stmt, 1, time(nullptr) - m_settings.totalTimeLimit);
        bindInt64(stmt, 2, m_settings.totalSizeLimit / 100);
        step(stmt);
        finalize(stmt);
    }
```

### 7.4 手动清理

```cpp
// 清空所有缓存
void Storage::clearAllCache();

// 按 tag 选择性清除
void Storage::clearCacheByTag(CacheTag tag); // 0=全部清除
```

### 7.5 频道数据删除

用户删除好友/群组时连带清理对应数据（`translations` 由 `ON DELETE CASCADE` 自动清理，无需显式 DELETE）：

```cpp
void Storage::clearChannel(const QString& chanid) {
    exec("DELETE FROM messages WHERE chanid = ?", chanid);
    exec("DELETE FROM bookmarks WHERE chanid = ?", chanid);
    exec("DELETE FROM reactions WHERE message_rowid IN "
         "(SELECT rowid FROM messages WHERE chanid = ?)", chanid);
    exec("DELETE FROM pending_messages WHERE chanid = ?", chanid);
    exec("DELETE FROM channels WHERE chanid = ?", chanid);
}
```

## 8. SQLite 版本要求与安装方案

### 8.1 版本要求

| 功能 | 最低版本 | 引入 |
|------|---------|------|
| WAL 模式 | 3.7.0 (2010) | 基础存储 |
| FTS5 + trigram CJK 搜索 | 3.34.0 (2020-12) | 全文搜索 |

### 8.2 方案总览

两种方案，`.pro` 编译时自动检测：

| 方案 | 条件 | 说明 |
|------|------|------|
| A：系统 libsqlite3 | 系统 SQLite ≥ 3.34.0 + FTS5 启用 | 优先使用 |
| B：Bundled Amalgamation | 系统 SQLite 过旧 | 静态编译 sqlite3.c |

主流 IM（Telegram、Signal、Chrome）均采用方案 B。

### 8.3 编译时自动检测

```qmake
# qltox.pro — SQLite 方案自动选择
system("echo 'CREATE VIRTUAL TABLE t USING fts5(c,tokenize=trigram);' | \
        cc -x c -include sqlite3.h -lsqlite3 - -o /dev/null 2>/dev/null") {
    message("System libsqlite3 >= 3.34.0 with FTS5, using system library")
    LIBS += -lsqlite3
    DEFINES += HAVE_SQLITE_FTS5
} else {
    message("System SQLite too old, using bundled sqlite3.c")
    INCLUDEPATH += sqlite3
    SOURCES += sqlite3/sqlite3.c
    DEFINES += SQLITE_ENABLE_FTS5
}
```

### 8.4 方案 A：系统 libsqlite3

```bash
# 安装
sudo apt install libsqlite3-dev          # Debian/Ubuntu
sudo dnf install libsqlite3x-devel       # RHEL/Fedora

# 验证
sqlite3 --version                                    # ≥ 3.34.0
sqlite3 :memory: "CREATE VIRTUAL TABLE t USING fts5(c, tokenize='trigram');"  # 无报错
```

### 8.5 方案 B：Bundled Amalgamation

```bash
# 1. 下载
wget https://www.sqlite.org/2026/sqlite-autoconf-3490100.tar.gz
tar xzf sqlite-autoconf-3490100.tar.gz

# 2. 提取到项目
cp sqlite-autoconf-3490100/sqlite3.c    qltox/sqlite3/
cp sqlite-autoconf-3490100/sqlite3.h    qltox/sqlite3/
cp sqlite-autoconf-3490100/sqlite3ext.h qltox/sqlite3/

# 3. 编译（自动检测到 bundled）
cd qltox && bash buildqt3.sh

# 验证 bundled
ldd qltox | grep sqlite     # 不应显示 libsqlite3.so
```

### 8.6 新增文件

| 文件 | 说明 |
|------|------|
| `qltox/sqlite3/sqlite3.c` | 融合源码（~8MB，从官网下载） |
| `qltox/sqlite3/sqlite3.h` | 头文件 |
| `qltox/sqlite3/sqlite3ext.h` | FTS5 扩展接口 |

### 8.7 运行时检测

```cpp
// Storage::init() 中调用
void checkSqliteFeatures() {
    // 1. 版本
    QString ver = query("SELECT sqlite_version()").toString();
    qDebug() << "SQLite version:" << ver;
    if (ver < "3.34.0")
        qWarning() << "SQLite < 3.34.0, trigram not available";

    // 2. FTS5
    bool fts5 = exec("CREATE VIRTUAL TABLE IF NOT EXISTS _t_fts USING fts5(c)")
                == SQLITE_OK;
    exec("DROP TABLE IF EXISTS _t_fts");
    if (!fts5) qWarning() << "FTS5 not available, message search disabled";

    // 3. Trigram
    bool tri = exec("CREATE VIRTUAL TABLE IF NOT EXISTS _t_tri USING fts5(c, tokenize='trigram')")
               == SQLITE_OK;
    exec("DROP TABLE IF EXISTS _t_tri");
    if (!tri) qWarning() << "Trigram not available, CJK search degraded";

    // 4. Triggers（content 同步）
    exec("CREATE VIRTUAL TABLE IF NOT EXISTS _t_ft2 USING fts5(c, content='_t_src', content_rowid='rowid')");
    exec("CREATE TABLE IF NOT EXISTS _t_src (rowid INTEGER PRIMARY KEY, c TEXT)");
    bool trigOk = exec("CREATE TRIGGER IF NOT EXISTS _t_trig AFTER INSERT ON _t_src BEGIN "
                       "INSERT INTO _t_ft2(rowid, c) VALUES (NEW.rowid, NEW.c); END;")
                  == SQLITE_OK;
    exec("DROP TRIGGER IF EXISTS _t_trig");
    exec("DROP TABLE IF EXISTS _t_ft2");
    exec("DROP TABLE IF EXISTS _t_src");
    if (!trigOk) qWarning() << "FTS5 content triggers not available";
}
```

### 8.8 启动日志示例

```
SQLite version: 3.45.1
FTS5: OK
Trigram tokenizer: OK
FTS5 content sync triggers: OK
→ 全部通过，使用系统 libsqlite3
```

```
SQLite version: 3.7.17
FTS5: NOT AVAILABLE - no such module: fts5
→ 需要升级 SQLite 或改用 bundled
```

### 8.9 `.gitignore` 建议

```
qltox/sqlite3/*.c
qltox/sqlite3/*.o
```

大文件不提交，由开发者手动下载或 CI 脚本自动获取。

## 9. LoadToCacheSetting 枚举

参考 TG 的 `LoadToCacheAsWell` / `LoadToCacheNotRequired`：

```cpp
enum LoadToCache {
    LoadToCacheAsWell,        // 正常下载 + 缓存（图片、GIF、文件）
    LoadToCacheNotRequired,   // 只需要显示，不需要缓存（头像缩略图等一次性的）
    LoadToCacheSkip,          // 流媒体，直接走临时文件，不长期缓存
};
```

在我们的场景中：
- 收到图片消息 → `LoadToCacheAsWell`：下载并存 cache.db
- 头像 → `LoadToCacheAsWell`（第一次下载后永久缓存）
- 视频 → `LoadToCacheSkip`（太大，stream 模式，播放完可丢弃）
- 链接预览缩略图 → `LoadToCacheAsWell`：存 thumbnail_key

## 10. 统计 + 设置 UI 映射

### 10.1 统计接口

```cpp
struct TagStats {
    int64_t totalSize = 0;     // 字节
    int count = 0;             // 文件数
};

struct StorageStats {
    TagStats byTag[6];         // 0=total, 1-5=各 tag
    int64_t messageCount = 0;
};

StorageStats Storage::getStats() {
    StorageStats stats;
    // cache.db 按 tag 汇总
    sql("SELECT tag, SUM(size), COUNT(*) FROM cache GROUP BY tag",
        [&](sqlite3_stmt* s) {
            int tag = columnInt(s, 0);
            stats.byTag[tag] = {columnInt64(s, 1), columnInt(s, 2)};
        });
    // message count
    stats.messageCount = queryInt("SELECT COUNT(*) FROM messages");
    return stats;
}
```

### 10.2 设置 UI（for future use）

预留接口，等待设置页面接入：

| UI 项 | 存储 | 实现 |
|-------|------|------|
| "缓存大小: XXX MB" | StorageStats | 显示 + 定期刷新 |
| "清除图片缓存" | `clearCacheByTag(kImageCacheTag)` | 异步清除 |
| "清除头像缓存" | `clearCacheByTag(kAvatarCacheTag)` | 同上 |
| "清除所有缓存" | `clearAllCache()` | 同上 |
| "自动清理" | `CacheSettings` 保存在 QSettings | BoolConfigItem |

## 11. 集成到现有的 ChatElement

### 11.1 消息接收流程（新）

```
收到消息（handleEvents）
    │
    ├──→ 构造 ChatElement
    │
    ├──→ Storage::appendMessage(chanid, msg)
    │       └── pendingWrites → 200ms 后 batch flush
    │
    ├──→ ChatView::appendMessage(msg)
    │       └── 如果含媒体 URL：
    │           ├── Storage::hasMedia(file_<hash>) → 有: loadMedia → setPixmap
    │           └── 无: emit downloadNeeded → ToxAPI::downloadMedia
    │
    └──→ 显示消息
```

### 11.2 切换 channel 流程（新）

```
onContactSelected(id, type)
    │
    ├──→ Storage::switchChannel("friend:0")
    │       └── loadMessages("friend:0", 50)
    │           └── 返回 vector<ChatElement>
    │
    ├──→ ChatView::clearMessages()
    │
    └──→ ChatView::restoreMessages(msgs)
```

### 11.3 媒体下载完成流程（新）

```
AvatarDownloadEvent (success)
    │
    ├──→ AvatarManager::inst().store(mxcUrl, pixmap, kAvatarSize)
    │
    └──→ Storage::storeMedia("avatar_<hash>", pixmapBytes, "image/png", kAvatarCacheTag)
```

### 11.4 启动初始化

```cpp
// main.cpp 或 MainWindow 构造函数
if (!Storage::instance().init(
        QDir::homePath() + "/.config/toxhttpd",
        QDir::homePath() + "/.cache/toxhttpd")) {
    qWarning("Storage init failed");
}
```

## 12. 文件改动清单

### 新增文件

| 文件 | 说明 | 预估行数 |
|------|------|----------|
| `storage.h` | Storage 类声明 + 内部结构体 | 150 |
| `storage.cpp` | Storage 实现 + DB 操作 + write-behind | 800 |
| `qltox.pro` | 添加 `storage.cpp` + `-lsqlite3` | +3 |

### 修改文件

| 文件 | 修改内容 |
|------|----------|
| `mainwindow.cpp` | 构造函数中调用 `Storage::init()`；切换 channel 时 `Storage::switchChannel()`；收到消息时 `Storage::appendMessage()`；下载完成后 `Storage::storeMedia()` |
| `chatwidget.h/cpp` | 内部调 `Storage` 读写；`appendMessage()` 可能需要异步加载 thumbnail |
| `chatview.cpp` | 不使用存储，只负责显示（ChatElement 不变） |
| `restapi.h/cpp` | 新增 `downloadMedia(mxcUrl)`（类似 `downloadAvatar`） |
| `eventpoller.h` | 新增 `ApiMediaDownload` 枚举 / `MediaDownloadEvent` |

### 不修改文件

| 文件 | 原因 |
|------|------|
| `contactlist.h/cpp` | 联系人列表与存储正交，不改 |
| `translator.h/cpp` | 无关 |
| `LimeStyle.*` | 无关 |
| `AvatarManager` | 只负责缩放+圆形裁剪，存储由 Storage 处理 |
## 13. 与现有 AvatarManager 的关系

`AvatarManager` 目前是内存缓存 + identicon fallback。与 `Storage` 的职责划分：

```
AvatarManager           Storage
    │                       │
    ├── get(mxcUrl)         ├── storeMedia(key, data)
    │   └── 内存缓存命中    │   └── 持久化到 cache.db
    ├── requestDownload     │
    ├── store(...pixmap)    ├── loadMedia(key)
    │   └── 存内存缓存      │   └── 从 cache.db 读取
    └── identicon fallback  │
                            └── evict()
                                └── 清理过期缓存
```

**AvatarManager 的 get() 流程改为**：
```
1. 查内存缓存 → 命中返回
2. 查 Storage cache.db → 命中返回 + 填充内存缓存
3. identicon fallback（无缓存时）
```

## 14. 实施步骤

### 第一阶段：基础设施（第 1–3 天）

1. `qltox.pro` 添加 `-lsqlite3`
2. 编写 `Database` 底层封装类（exec, prepare, bind, step, finalize）
3. 实现 WAL pragma 设置
4. 编译验证 Qt3/Qt4

### 第二阶段：message.db（第 4–7 天）

1. 设计并创建 message.db schema
2. 实现 `appendMessage()` + 内存 pending 队列
3. 实现 `loadMessages()` / `loadMessagesBefore()`
4. 实现 write-behind timer 和 flush
5. 集成到 `mainwindow.cpp` 消息接收处

### 第三阶段：cache.db（第 8–10 天）

1. 设计并创建 cache.db schema
2. 实现 `storeMedia()` / `loadMedia()` 
3. 实现缩略图缓存逻辑
4. 集成 `AvatarManager` 读取 Storage

### 第四阶段：测试+调试（5 天）

1. Qt3 编译测试
2. Qt4 编译测试
3. 发送消息 → 检查 message.db 是否写入
4. 重启客户端 → 检查消息是否恢复
5. 发送图片 → 检查 cache.db 是否缓存
6. 缓存满 → 检查 LRU 驱逐
7. 切换 channel → 检查消息是否从 DB 加载
8. 删除好友/群组 → 检查对应消息是否清除

## 15. 时间估算汇总

| 阶段 | 时间 |
|------|------|
| 第一阶段：基础设施（Database 封装 + WAL） | 3 天 |
| 第二阶段：message.db + write-behind | 4 天 |
| 第三阶段：cache.db + 媒体缓存 | 3 天 |
| 第四阶段：测试调试 | 5 天 |
| 缓冲 | 2 天 |
| **总计** | **17 天** |

## 16. 风险点

| 风险 | 严重度 | 应对 |
|------|--------|------|
| SQLite 在 Qt3 的旧编译环境下不可用或版本太老 | 高 | 检查 `/opt/qt338sh` 是否包含 libsqlite3；如无则用系统 libsqlite3（要求 ≥3.7.0 支持 WAL） |
| write-behind 并发问题（主线程写 pending + 子线程 flush） | 高 | `QMutexLocker` 保护 pending 列表；flush 在 QThread 中运行 |
| 写入时 UI 卡顿 | 中 | 确保 flush 在后台线程执行；主线程只操作内存 |
| 大 BLOB 写入性能 | 中 | 超过 1MB 的文件不走 cache.db inline BLOB，走 filesystem |
| 磁盘占满 | 低 | LRU 驱逐保证缓存不超限；启动时检查磁盘空间 |
| Qt3/Qt4 DB 相关 API 差异 | 中 | Database 封装层已隔离；`.pro` 区分 |

## 17. 验证方式

```bash
# 编译验证
cd qltox && bash buildqt3.sh && bash buildqt4.sh

# 运行后检查
ls -la ~/.config/toxhttpd/message.db   # 应有数据
ls -la ~/.cache/toxhttpd/cache.db       # 应有数据

# 命令行验证（发送一条消息后）
sqlite3 ~/.config/toxhttpd/message.db "SELECT COUNT(*) FROM messages;"
sqlite3 ~/.config/toxhttpd/message.db "SELECT chanid, etype, file_name FROM messages LIMIT 5;"

# 缓存验证
sqlite3 ~/.cache/toxhttpd/cache.db "SELECT key, size, tag FROM cache LIMIT 10;"

# 重启客户端
# 验证：之前打开的好友聊天 → 消息历史完整显示
# 验证：之前下载的图片/头像 → 立即显示（无需重新下载）
# 验证：设置中可以看到缓存统计
```
