#include "media_shmem_cache.h"

MediaShmemCache& MediaShmemCache::inst() {
    static MediaShmemCache s;
    return s;
}

QPixmap MediaShmemCache::getThumb(const QString& mxcUrl) {
    auto it = m_thumbCache.find(mxcUrl);
    return (it != m_thumbCache.end()) ? it->second : QPixmap();
}

void MediaShmemCache::putThumb(const QString& mxcUrl, const QPixmap& thumb) {
    m_thumbCache[mxcUrl] = thumb;
}

void MediaShmemCache::clear() {
    m_thumbCache.clear();
}
