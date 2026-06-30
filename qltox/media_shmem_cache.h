#ifndef MEDIA_SHMEM_CACHE_H
#define MEDIA_SHMEM_CACHE_H

#include <qstring.h>
#include <qpixmap.h>
#include <map>

class MediaShmemCache {
public:
    static MediaShmemCache& inst();
    QPixmap getThumb(const QString& mxcUrl);
    void putThumb(const QString& mxcUrl, const QPixmap& thumb);
    void clear();
private:
    MediaShmemCache() = default;
    std::map<QString, QPixmap> m_thumbCache;
};

#endif
