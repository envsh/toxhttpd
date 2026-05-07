# SQLite events 表设计 - 消息同步库

## 一、需求概述

| 需求 | 说明 |
|------|------|
| 表名 | `events` |
| 自增主键 | `rowid` INTEGER PRIMARY KEY AUTOINCREMENT，从 **10000** 开始 |
| 通道标识 | `chanid` TEXT NOT NULL（表示好友/群组/会议） |
| 数据负载 | `data` TEXT NOT NULL（JSON 格式打包存储，对同步逻辑透明） |
| 索引 | `chanid` 带索引 |

---

## 二、SQLite 表结构设计

### 1. 建表 SQL

```sql
-- 创建 events 表
CREATE TABLE IF NOT EXISTS events (
    rowid     INTEGER PRIMARY KEY AUTOINCREMENT,
    chanid    TEXT NOT NULL,
    data      TEXT NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- 创建索引（chanid 单列索引，用于按通道查询）
CREATE INDEX IF NOT EXISTS idx_events_chanid ON events(chanid);

-- 可选：复合索引（按通道+时间查询优化）
CREATE INDEX IF NOT EXISTS idx_events_chanid_time ON events(chanid, created_at DESC);
```

### 2. 设置自增起始值为 10000

SQLite 的 `AUTOINCREMENT` 默认从 1 开始。要实现从 10000 开始，需操作内部表 `sqlite_sequence`：

```sql
-- 确保 sqlite_sequence 表存在（SQLite 会在首次 INSERT 时自动创建，但我们可以预创建）
CREATE TABLE IF NOT EXISTS sqlite_sequence (
    name TEXT PRIMARY KEY,
    seq  INTEGER
);

-- 设置 events 表的序列起始值（9999 表示下一个是 10000）
INSERT OR REPLACE INTO sqlite_sequence(name, seq) VALUES('events', 9999);
```

**原理**：当首次对 `events` 表执行不指定 `rowid` 的 INSERT 时，SQLite 会检查 `sqlite_sequence` 表中是否存在 `name='events'` 的记录：
- 存在：使用 `seq + 1` 作为新的 `rowid`（9999 + 1 = 10000）
- 不存在：从 1 开始

**验证方法**：
```sql
-- 插入测试数据（不指定 rowid）
INSERT INTO events(chanid, data) VALUES('test', '{"type":"test"}');

-- 查询结果应该是 rowid = 10000
SELECT rowid, chanid, data FROM events;

-- 清理测试数据
DELETE FROM events WHERE chanid = 'test';
```

---

## 三、chanid 设计建议

`chanid` 字段用于标识消息来自哪个通道（好友/群组/会议）。建议格式规范：

| 通道类型 | chanid 格式示例 | 说明 |
|-----------|------------------|------|
| 好友 | `friend:0` | 冒号分隔类型和 ID |
| 群组 | `group:1` | 使用群组号 |
| 会议 | `conference:2` | 使用会议号 |
| 直接数字 | `0` | 简化版，直接存 ID |

**推荐**：使用 `类型:ID` 格式，便于扩展和查询。

---

## 四、data JSON 格式（对同步透明）

同步逻辑只需按 `rowid` 顺序读取并传输 `data` 字段，无需解析。示例存储内容：

```json
// 好友消息
{"event_type":"friend_message","friend_id":0,"message":"hello","direction":"received"}

// 群组消息
{"event_type":"group_message","group_number":1,"peer_number":2,"message":"hi","direction":"sent"}

// 会议消息
{"event_type":"conference_message","conference_id":0,"message":"meeting","direction":"received"}
```

---

## 五、同步逻辑接口设计（参考 gomuks）

基于 `events` 表，同步逻辑可以这样工作：

### 1. 增量同步查询

```sql
-- 获取某通道在 last_rowid 之后的消息（类似 gomuks 的 after 参数）
SELECT rowid, data FROM events 
WHERE chanid = ? AND rowid > ? 
ORDER BY rowid ASC 
LIMIT ?;
```

### 2. 历史消息查询（分页）

```sql
-- 获取某通道在指定 rowid 之前的消息（类似 before 参数）
SELECT rowid, data FROM events 
WHERE chanid = ? AND rowid < ? 
ORDER BY rowid DESC 
LIMIT ?;
```

### 3. 长轮询支持

```sql
-- 等待新消息（简化版，实际可用 SELECT ... WHERE rowid > ? 轮询）
SELECT rowid, data FROM events 
WHERE chanid = ? AND rowid > ? 
ORDER BY rowid ASC;
```

---

## 六、与 gomuks 概念映射

| gomuks 概念 | toxhttpd 对应设计 | 说明 |
|--------------|-------------------|------|
| `event_id` | `rowid` | 事件唯一标识，自增整数 |
| `run_id` | （建议新增 `sessions` 表） | 后端进程/会话标识 |
| `last_received_event` | 客户端保存的 `last_rowid` | 最后收到的事件 ID |
| `data` 字段 | `data` TEXT | JSON 格式，对同步透明 |
| `timeline` | 应用层逻辑 | 按 `rowid` 顺序构建时间线 |

---

## 七、Go 代码实现规划

### 1. 修改 Server 结构（main.go:177-185）

```go
import "github.com/mattn/go-sqlite3"

type Server struct {
    tox                  *tox.Tox
    db                   *sql.DB  // 新增：SQLite 数据库连接
    eventQueue           *SQLiteEventQueue  // 替换原 EventQueue
    selfConnectionStatus string
    friendStatuses       map[uint32]string
    conferenceConnected map[uint32]bool
    mu                   sync.RWMutex
}
```

### 2. 创建 SQLiteEventQueue 结构（替代 EventQueue）

```go
// SQLiteEventQueue 是基于 SQLite 的事件队列
type SQLiteEventQueue struct {
    db *sql.DB
}

func NewSQLiteEventQueue(db *sql.DB) *SQLiteEventQueue {
    return &SQLiteEventQueue{db: db}
}
```

### 3. 初始化数据库函数（新增）

```go
func initDatabase(dbPath string) (*sql.DB, error) {
    // 创建数据目录
    os.MkdirAll(filepath.Dir(dbPath), 0700)

    // 打开 SQLite 数据库
    db, err := sql.Open("sqlite3", dbPath)
    if err != nil {
        return nil, fmt.Errorf("failed to open database: %w", err)
    }

    // 创建 events 表
    _, err = db.Exec(`
        CREATE TABLE IF NOT EXISTS events (
            rowid     INTEGER PRIMARY KEY AUTOINCREMENT,
            chanid    TEXT NOT NULL,
            data      TEXT NOT NULL,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        )
    `)
    if err != nil {
        return nil, fmt.Errorf("failed to create events table: %w", err)
    }

    // 创建索引
    _, err = db.Exec(`CREATE INDEX IF NOT EXISTS idx_events_chanid ON events(chanid)`)
    if err != nil {
        return nil, fmt.Errorf("failed to create index: %w", err)
    }

    // 设置自增起始值为 10000
    _, err = db.Exec(`
        INSERT OR REPLACE INTO sqlite_sequence(name, seq) VALUES('events', 9999)
    `)
    // 忽略错误（如果 sqlite_sequence 表还不存在，首次 INSERT 后会自动创建）
    
    return db, nil
}
```

### 4. 修改 Push 方法（替代原 EventQueue.Push）

```go
func (q *SQLiteEventQueue) Push(chanid string, eventType string, data string) (uint64, error) {
    result, err := q.db.Exec(
        "INSERT INTO events(chanid, data) VALUES(?, ?)",
        chanid, data,
    )
    if err != nil {
        return 0, fmt.Errorf("failed to push event: %w", err)
    }
    
    // 获取插入的 rowid
    rowid, err := result.LastInsertId()
    if err != nil {
        return 0, fmt.Errorf("failed to get rowid: %w", err)
    }
    
    return uint64(rowid), nil
}
```

### 5. 修改 PopAfter 方法（替代原 EventQueue.PopAfter）

```go
type Event struct {
    RowID     uint64 `json:"event_id"`
    ChanID    string `json:"chanid"`
    Data      string `json:"data"`
    Timestamp time.Time `json:"timestamp"`
}

func (q *SQLiteEventQueue) PopAfter(after uint64) ([]Event, error) {
    rows, err := q.db.Query(`
        SELECT rowid, chanid, data, created_at 
        FROM events 
        WHERE rowid > ?
        ORDER BY rowid ASC
    `, after)
    if err != nil {
        return nil, fmt.Errorf("failed to pop events: %w", err)
    }
    defer rows.Close()
    
    events := make([]Event, 0)
    for rows.Next() {
        var e Event
        if err := rows.Scan(&e.RowID, &e.ChanID, &e.Data, &e.Timestamp); err != nil {
            return nil, err
        }
        events = append(events, e)
    }
    return events, nil
}
```

### 6. 修改 DeleteEvent 方法

```go
func (q *SQLiteEventQueue) DeleteEvent(id uint64) error {
    _, err := q.db.Exec("DELETE FROM events WHERE rowid = ?", id)
    return err
}
```

### 7. 修改 NewServer 函数（main.go:187-222）

```go
func NewServer(udpEnabled bool) (*Server, error) {
    // ... 原有逻辑 ...

    // 初始化数据库
    db, err := initDatabase("data/events.db")
    if err != nil {
        return nil, fmt.Errorf("failed to init database: %w", err)
    }

    eventQueue := NewSQLiteEventQueue(db)

    server := &Server{
        tox:                  t,
        db:                   db,
        eventQueue:           eventQueue,
        selfConnectionStatus: "offline",
        friendStatuses:       make(map[uint32]string),
    }

    // ... 后续逻辑 ...
}
```

### 8. 修改回调中的事件推送（main.go 中的回调）

以 `handleConferenceMessage` 回调为例（原 main.go:380-389）：

```go
// 原代码（内存）
data, _ := json.Marshal(map[string]interface{}{
    "conference_number": groupNumber,
    "peer_number":      peerNumber,
    "message":          message,
})
s.eventQueue.Push("conference_message", string(data))

// 新代码（SQLite）
data, _ := json.Marshal(map[string]interface{}{
    "conference_number": groupNumber,
    "peer_number":      peerNumber,
    "message":          message,
})
chanid := fmt.Sprintf("conference:%d", groupNumber)
s.eventQueue.Push(chanid, "conference_message", string(data))
```

**需要修改的回调列表**：
- `handleConferenceMessage` (line 380-389)
- `handleGroupMessage` (line 391-400) → chanid = `group:X`
- `handleFriendMessage` (line 262-270) → chanid = `friend:X`
- `handleConferenceInvite` (line 369-378) → chanid = `conference:X`
- 其他推送事件的地方

---

## 八、迁移注意事项

1. **向后兼容**：新的 `Event` 结构增加了 `ChanID` 字段，但 JSON 序列化仍使用 `event_id` 字段名（保持客户端兼容）

2. **数据迁移**：如果已有内存中的事件需要保留（通常不会，因为重启会丢失），可以在初始化时插入。

3. **错误处理**：SQLite 操作需要添加适当的错误处理，特别是 `LastInsertId()` 在 SQLite 中需要驱动支持。

4. **编译标签**：`go-sqlite3` 需要 CGO 和 gcc，确保编译环境支持。

---

## 九、验证步骤

1. 启动服务器，检查数据库文件 `data/events.db` 是否创建
2. 检查 `sqlite_sequence` 表：`SELECT * FROM sqlite_sequence WHERE name='events';` 应返回 `seq = 9999`
3. 触发一个事件（如发送消息），检查 `events` 表：`SELECT rowid, chanid, data FROM events;` 第一条记录应为 `rowid = 10000`
4. 测试长轮询：`curl "http://localhost:8181/api/events?after=10000"` 应返回新事件
