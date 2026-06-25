# SQLite 媒体消息存储设计

## 1. 概述

### 1.1 目标

- 消息历史持久化：重启客户端后聊天记录不丢失
- 媒体文件缓存：图片、GIF、视频、文件下载后本地缓存
- 离线访问：已加载的消息和媒体离线可看
- 内存效率：只保留当前可见的消息在内存，历史消息按需从 SQLite 加载
- TG 级性能：write-behind 批量写入、WAL 并发读、LRU 自动驱逐
- 缓存有上限：默认 cache.db ≤200MB、big_cache.db ≤500MB，超出自动 LRU 驱逐至 80%

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
| big_cache.db | 500 MB | LRU 超出后驱逐至 80% | ✅ 同上 |

cache.db 上限选取理由：
- 主流聊天场景：按每天 50 条图片/表情，约 5MB/天 → 200MB ≈ 40 天会话
- 与系统磁盘空间占比：200MB 对现代设备极小，无需用户操心
- TG 桌面端默认 512MB，我们初次保守取 200MB，后续可调

超出上限行为：
- 每次写入缓存后检查 `getTotalCacheSize()` > 上限
- 触发 LRU 驱逐直到降至上限的 80%
- 驱逐顺序：超 30 天未访问 → 超 1 天且单文件 > 2MB → 最久未访问

## 2. 整体架构

### 2.1 三库架构（TG 参考）

```
~/.config/toxhttpd/
├── message.db          # 消息历史 + 媒体引用 + 草稿 (WAL 模式)
└── channel_000001.db   # 未来：按 channel 分库（可选）

~/.cache/toxhttpd/
├── cache.db            # 小文件缓存 (≤1MB inline BLOB, WAL 模式)
└── big_cache.db        # 大文件路径索引 (WAL 模式)
```

为什么要三个库：
- **message.db**：结构化数据，需要强一致性、外键、事务
- **cache.db**：缓存数据，可丢失，重点是快速读写、LRU 驱逐
- **big_cache.db**：大文件索引，只存路径，不存文件内容

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
    │   └── full ──→ ~/.cache/toxhttpd/files/<hash>.ext
    │               └── big_cache.db (key=file_<hash> → path)
    │
    ├── 大文件 (>30MB)
    │   └── full ──→ ~/.cache/toxhttpd/big/<hash>.ext
    │               └── big_cache.db (key=big_<hash> → path)
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
        ├── cache.db   ──→ SQLite + BLOB (小文件)
        └── big_cache.db ──→ SQLite + filesystem (大文件)
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
    for (auto* db : {m_messageDb, m_cacheDb, m_bigCacheDb})
        db->exec("PRAGMA wal_checkpoint(TRUNCATE)");
}
```

## 4. message.db 表结构

### 4.1 建表 SQL

```sql
-- 通道表
CREATE TABLE IF NOT EXISTS channels (
    chanid      TEXT PRIMARY KEY,   -- "friend:0", "group:1", "conference:2"
    last_rowid  INTEGER NOT NULL DEFAULT 0,
    unread      INTEGER NOT NULL DEFAULT 0,   -- 未读数（当前 session 覆盖）
    created_at  TIMESTAMP DEFAULT CURRENT_TIMESTAMP
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
    media_url   TEXT,                        -- MXC URL
    media_mime  TEXT,                        -- "image/png" / "video/mp4"
    media_width INTEGER DEFAULT 0,
    media_height INTEGER DEFAULT 0,
    file_name   TEXT,                        -- "photo.jpg"
    file_size   INTEGER DEFAULT 0,          -- 字节数
    duration_sec INTEGER DEFAULT 0,         -- 视频/音频时长
    local_path  TEXT,                        -- 已下载到本地的完整路径
    thumbnail_key TEXT,                      -- cache.db 中的缩略图 key
    cache_tag   INTEGER DEFAULT 0,          -- 0=none, 1=avatar, 2=image, 3=gif, 4=video, 5=file
    created_at  TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- 索引
CREATE INDEX IF NOT EXISTS idx_messages_chanid
    ON messages(chanid, rowid DESC);
CREATE INDEX IF NOT EXISTS idx_messages_media_url
    ON messages(media_url) WHERE media_url IS NOT NULL;

-- 草稿表
CREATE TABLE IF NOT EXISTS drafts (
    chanid      TEXT PRIMARY KEY REFERENCES channels(chanid),
    draft_text  TEXT NOT NULL DEFAULT '',
    updated_at  TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Schema 版本表（用于迁移）
CREATE TABLE IF NOT EXISTS schema_version (
    version     INTEGER PRIMARY KEY,
    applied_at  TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
INSERT OR IGNORE INTO schema_version(version) VALUES(1);
```

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

- type: `avatar` / `thumb` / `file` / `big`
- hash: SHA256 of MXC URL 的前 32 字符（MXC URL 本身已经内容寻址，
  但用 SHA256 作为 key 可以统一处理，长度固定）

```
avatar_a1b2c3d4e5f6...   → 头像
thumb_a1b2c3d4...         → 缩略图
file_a1b2c3d4...          → 完整文件（≤1MB 或 路径引用）
big_a1b2c3d4...           → 大文件路径引用
```

### 5.4 缓存大小阈值

```cpp
// TG 参考值：kMaxFileInMemory = 10MB, kUseBigFilesFrom = 30MB
// 我们的调整：
constexpr int64_t kMaxSmallFileSize    = 1 * 1024 * 1024;     // ≤1MB → cache.db inline BLOB
constexpr int64_t kMediumFileThreshold = 30 * 1024 * 1024;    // 1MB–30MB → filesystem + cache.db 路径
constexpr int64_t kBigFileThreshold    = 30 * 1024 * 1024;    // ≥30MB → filesystem + big_cache.db
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

`big_cache.db` 同理：

```sql
UPDATE big_cache SET access_time = ? WHERE key = ?;
```

## 6. big_cache.db 表结构

### 6.1 建表 SQL

```sql
-- 大文件索引表
CREATE TABLE IF NOT EXISTS big_cache (
    key         TEXT PRIMARY KEY,       -- "big_<hash>"
    file_path   TEXT NOT NULL,          -- 绝对路径
    mime_type   TEXT DEFAULT '',
    tag         INTEGER DEFAULT 0,
    access_time INTEGER NOT NULL,
    size        INTEGER NOT NULL,
    created_at  TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX IF NOT EXISTS idx_bigcache_tag ON big_cache(tag);
CREATE INDEX IF NOT EXISTS idx_bigcache_access ON big_cache(access_time);
```

### 6.2 文件系统目录结构

```
~/.cache/toxhttpd/
├── files/          # 中型文件 (1MB–30MB)
│   ├── a1/
│   │   └── a1b2c3d4e5f6....jpg
│   └── b2/
│       └── b2c3d4e5f6....gif
├── big/            # 大文件 (>30MB)
│   ├── c3/
│   │   └── c3d4e5f6....mp4
│   └── d4/
│       └── d4e5f6....pdf
└── cache.db        # 小文件缓存
```

子目录用 hash 前两个字符分片，避免单目录内有太多文件。

## 7. Write-behind Batching（Postbox 模式）

### 7.1 为什么需要 write-behind

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

### 7.2 Storage 类设计

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

### 7.3 写入流程

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

### 7.4 读流程（直接读 DB）

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

### 7.5 事务合并

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

## 8. 缓存上限与清理策略

### 8.1 清理范围

| 数据库 | 自动 LRU 驱逐 | 用户手动清理 | 被动删除 |
|--------|--------------|-------------|---------|
| message.db | ❌ 永久保留 | ❌ 不提供 | ✅ 删好友/群组时 clearChannel() |
| cache.db   | ✅ 达 200MB 或 30 天 | ✅ 按 tag 选择清除 | ❌ 自动处理 |
| big_cache.db | ✅ 达 500MB 或 7 天 | ✅ 全部清除 | ❌ 自动处理 |

- **自动 LRU 驱逐**：后台定时器检查上限，超出则按 access_time 删最旧文件
- **用户手动清理**：设置页提供"清除头像缓存""清除图片缓存""清除所有缓存"按钮
- **被动删除**：用户删除好友/群组时连带清空对应消息和媒体缓存

> **message.db 不受影响。** 以下驱逐逻辑仅针对 cache.db 和 big_cache.db。

### 8.2 自动驱逐配置

```cpp
struct CacheSettings {
    int64_t totalSizeLimit   = 200 * 1024 * 1024;     // 200MB
    int64_t totalTimeLimit   = 30 * 24 * 60 * 60;     // 30天（秒）
    int64_t bigTotalSizeLimit = 500 * 1024 * 1024;     // 500MB
    int64_t bigTotalTimeLimit = 7 * 24 * 60 * 60;      // 7天（大文件占空间）
};
```

### 8.3 LRU 驱逐算法

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

    // big_cache.db 大文件驱逐（先删文件，再删索引）
    int64_t bigTotalSize = getBigCacheSize();
    if (bigTotalSize > m_settings.bigTotalSizeLimit) {
        int64_t targetSize = m_settings.bigTotalSizeLimit * 0.8;
        sqlite3_stmt* stmt = prepare(
            "SELECT key, file_path FROM big_cache"
            "  WHERE access_time < ?"
            "  ORDER BY access_time ASC LIMIT 100"
        );
        bindInt64(stmt, 1, time(nullptr) - m_settings.bigTotalTimeLimit);
        while (step(stmt) == SQLITE_ROW) {
            QFile::remove(columnText(stmt, 1));            // 删文件
            exec("DELETE FROM big_cache WHERE key = ?",
                 columnText(stmt, 0));                     // 删索引
        }
        finalize(stmt);
    }
}
```

### 8.4 手动清理

```cpp
// 清空所有缓存
void Storage::clearAllCache();

// 按 tag 选择性清除
void Storage::clearCacheByTag(CacheTag tag); // 0=全部清除
```

### 8.5 频道数据删除

用户删除好友/群组时连带清理对应数据：

```cpp
void Storage::clearChannel(const QString& chanid) {
    // 1. 删除消息
    exec("DELETE FROM messages WHERE chanid = ?", chanid);
    // 2. 删除频道记录
    exec("DELETE FROM channels WHERE chanid = ?", chanid);
    // 3. 删除草稿
    exec("DELETE FROM drafts WHERE chanid = ?", chanid);
}
```

### 8.6 文件系统清理

当 `big_cache.db` 中驱逐一条记录时，同时删除文件系统中的文件：

```cpp
void Storage::removeBigFile(const QString& key) {
    QString filePath = getBigFilePath(key);
    QFile::remove(filePath);
    exec("DELETE FROM big_cache WHERE key = ?", key);
}
```

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
    TagStats bigFiles;
    int64_t messageCount = 0;

    // 总缓存大小
    int64_t totalCacheSize() const {
        int64_t sum = 0;
        for (int i = 0; i < 6; ++i) sum += byTag[i].totalSize;
        return sum + bigFiles.totalSize;
    }
};

StorageStats Storage::getStats() {
    StorageStats stats;
    // cache.db 按 tag 汇总
    sql("SELECT tag, SUM(size), COUNT(*) FROM cache GROUP BY tag",
        [&](sqlite3_stmt* s) {
            int tag = columnInt(s, 0);
            stats.byTag[tag] = {columnInt64(s, 1), columnInt(s, 2)};
        });
    // big_cache.db 汇总
    sql("SELECT SUM(size), COUNT(*) FROM big_cache",
        [&](sqlite3_stmt* s) {
            stats.bigFiles = {columnInt64(s, 0), columnInt(s, 1)};
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

## 12. 集成到现有的 ChatElement

### 12.1 消息接收流程（新）

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

### 12.2 切换 channel 流程（新）

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

### 12.3 媒体下载完成流程（新）

```
AvatarDownloadEvent (success)
    │
    ├──→ AvatarManager::inst().store(mxcUrl, pixmap, kAvatarSize)
    │
    └──→ Storage::storeMedia("avatar_<hash>", pixmapBytes, "image/png", kAvatarCacheTag)
```

### 12.4 启动初始化

```cpp
// main.cpp 或 MainWindow 构造函数
if (!Storage::instance().init(
        QDir::homePath() + "/.config/toxhttpd",
        QDir::homePath() + "/.cache/toxhttpd")) {
    qWarning("Storage init failed");
}
```

## 13. 文件改动清单

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
## 14. 与现有 AvatarManager 的关系

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

## 15. 实施步骤

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

### 第四阶段：big_cache.db + 驱逐（第 11–13 天）

1. 设计并创建 big_cache.db schema
2. 实现文件系统目录管理
3. 实现 LRU 驱逐 + 定时器
4. 实现统计接口

### 第六阶段：测试+调试（5 天）

1. Qt3 编译测试
2. Qt4 编译测试
3. 发送消息 → 检查 message.db 是否写入
4. 重启客户端 → 检查消息是否恢复
5. 发送图片 → 检查 cache.db 是否缓存
6. 发送大文件 → 检查 big_cache.db 是否索引 + 文件落盘
7. 缓存满 → 检查 LRU 驱逐
8. 切换 channel → 检查消息是否从 DB 加载
9. 删除好友/群组 → 检查对应消息是否清除

## 16. 时间估算汇总

| 阶段 | 时间 |
|------|------|
| 第一阶段：基础设施（Database 封装 + WAL） | 3 天 |
| 第二阶段：message.db + write-behind | 4 天 |
| 第三阶段：cache.db + 媒体缓存 | 3 天 |
| 第四阶段：big_cache.db + LRU 驱逐 | 3 天 |
| 第六阶段：测试调试 | 5 天 |
| 缓冲 | 2 天 |
| **总计** | **20 天** |

## 17. 风险点

| 风险 | 严重度 | 应对 |
|------|--------|------|
| SQLite 在 Qt3 的旧编译环境下不可用或版本太老 | 高 | 检查 `/opt/qt338sh` 是否包含 libsqlite3；如无则用系统 libsqlite3（要求 ≥3.7.0 支持 WAL） |
| write-behind 并发问题（主线程写 pending + 子线程 flush） | 高 | `QMutexLocker` 保护 pending 列表；flush 在 QThread 中运行 |
| 写入时 UI 卡顿 | 中 | 确保 flush 在后台线程执行；主线程只操作内存 |
| 大 BLOB 写入性能 | 中 | 超过 1MB 的文件不走 cache.db inline BLOB，走 filesystem |
| 磁盘占满 | 低 | LRU 驱逐保证缓存不超限；启动时检查磁盘空间 |
| Qt3/Qt4 DB 相关 API 差异 | 中 | Database 封装层已隔离；`.pro` 区分 |

## 18. 验证方式

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
