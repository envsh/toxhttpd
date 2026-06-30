# 缩略图缓存方案 vs Telegram Desktop

## 概述

本文档对比 qltox 当前的缩略图（thumbnail）缓存实现与 Telegram Desktop 的参考设计，记录差异及改进方向。

---

## TG 缩略图策略要点

1. **独立缩略图传输**：每条媒体消息附带一个独立的 `thumbnail`（~90x90 JPEG），与全尺寸图片分开下载
2. **Memory-only 缩略图**：`PhotoData::thumb` 是 `QPixmap` 仅存内存，全尺寸 `PhotoData::full` 仅存磁盘
3. **后台线程解码**：图片解码和缩放都在 `threaded` loader 中完成
4. **渐进式展示**：下载缩略图后立即显示模糊版本 → 全尺寸下载后替换
5. **缓存键分离**：`"thumb_{id}"` / `"full_{id}"` 独立管理

---

## 逐项对比

### 一致的点 ✅

| 维度 | TG | qltox 当前 |
|------|-----|-----------|
| 缩略图在内存，全尺寸在磁盘 | `PhotoData::thumb` = QPixmap | `ChatElement::scaledDisplay` = QPixmap |
| 全尺寸异步加载 | `PhotoViewer` 打开时后台线程加载 + 事件回调 | `DiskLoadEvent` + `cacheDbAsync::loadMedia` |
| 切换聊天保留缩略图 | History 保留 `PhotoData` | `m_messageCache` 含 `ChatElement`，`scaledDisplay` 随之保留 |
| 每个消息独立布局缓存 | `HistoryItem::width()/height()` | `ChatElement::cachedWidth/height`（TG-style） |

### 不一致的点 ❌

#### 1. 主线程缩略图生成

```
当前：customEvent（主线程） → loadFromData → makeScaledThumb → scaledDisplay
TG：  threaded loader（后台） → decode → resize → postEvent → scaledDisplay
```

`decodeWebP` 涉及 `dlopen` + CPU 密集型解码，主线程调用可能卡 UI。

#### 2. 无独立缩略图下载入口

`eventpoller.h:108` 已有 `thumbnailUrl` 字段，但从未使用。

```
当前：下载全尺寸图片 → 解码 → 缩略图
TG： 先下载 90x90 缩略图 → 即时显示 → 双击原图时再下载全尺寸
```

大文件（4K 照片、视频预览图）场景浪费带宽和内存。

#### 3. 缩略图尺寸不一致

下载回调用 `chatWidget->width() * 70 / 100` 估算容器宽度：
```cpp
el.scaledDisplay = makeScaledThumb(tmp, el.mediaWidth, el.mediaHeight,
                                   chatWidget->width() * 70 / 100);
```

`paintThumbnail()` 中 dw/dh 基于 `imgRect.width()`（精确值）。两者不匹配时每帧 rescale：
```cpp
// chatview.cpp:307-323
if (thumb.width() == dw && thumb.height() == dh) {
    p.drawPixmap(ox, oy, thumb);       // 精确匹配 → 直接画
} else {
    QPixmap scaled = thumb.scaled(...); // 不匹配 → 每帧缩放
}
```

#### 4. 无渐进式加载

TG 流程：模糊缩略图（blurhash/thumb） → 清晰全尺寸替换。

当前：灰色占位框 + `"W × H"` 文字 → 下载完成后突然出现缩略图。

#### 5. 无独立缓存键

全尺寸和缩略图共用 `"file_{mxcId}"` 键：
```cpp
// storage.cpp
std::string mediaCacheKey(const char* prefix, const QString& mxcUrl) {
    // prefix="file" 结果 "file_abcdefg"
}
```

即使只需要缩略图（历史列表预览），也得加载全尺寸才能生成。

---

## 改进优先级

| 优先级 | 问题 | 方案 | 涉及文件 |
|--------|------|------|---------|
| **P0** | 主线程解码/缩放 | 将 `makeScaledThumb` 移到 `WriteQueue` 或新增 `ThumbWorker` 线程，完成后 `postEvent` | `mainwindow.cpp`、新增 `thumb_worker.h` |
| **P1** | 缩略图尺寸不一致 | 下载回调不生成 `scaledDisplay`，由 `paint()` 惰性生成（保持 `scaledDisplay.isNull()` 检查），或 `relayout()` 时用精确 `viewWidth` 生成 | `chatview.cpp` `paint()` |
| **P2** | 支持 `thumbnailUrl` | 消息到达时先检查 `thumbnailUrl`，若存在则优先下载 thumb → `scaledDisplay`，全尺寸点开再下 | `mainwindow.cpp` `handleEvents` |
| **P3** | 独立缓存键 | 缩略图 `"thumb_{mxcId}"` vs 全尺寸 `"full_{mxcId}"` | `storage.cpp` `mediaCacheKey`、`mainwindow.cpp` |
| **P4** | 渐进式模糊占位 | 缩略图下载完成前显示模糊化/低质量版本 | `chatview.cpp` `paint()` |

---

## 文件位置索引

| 文件 | 关键行 | 说明 |
|------|--------|------|
| `chatview.h` | L61 | `QPixmap scaledDisplay` — 唯一内存缓存的缩略图 |
| `chatview.cpp` | L19-41 | `makeScaledThumb()` — 缩略图生成函数 |
| `chatview.cpp` | L287-324 | `paintThumbnail()` — 绘制缩略图（含尺寸不匹配时的 rescale fallback） |
| `chatview.cpp` | L1221 | `paint()` 中直接使用 `scaledDisplay` |
| `mainwindow.cpp` | L492-501 | 下载回调中生成 `scaledDisplay`（主线程） |
| `mainwindow.cpp` | L2287-2305 | `onOpenFullSizeImage` — 异步加载全尺寸 |
| `eventpoller.h` | L108 | `thumbnailUrl` 字段（当前未使用） |
| `eventpoller.h` | L31 + L173-182 | `DiskLoadReadyType` + `DiskLoadEvent` |
| `storage.cpp` | L9-25 | `mediaCacheKey()` — 只支持 `"file_"` 前缀 |
