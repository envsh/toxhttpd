#include "avatar_manager.h"
#include "identicon.h"
#include <qpainter.h>
#ifdef QT3_BUILD
#include <qbitmap.h>
#include <qimage.h>
#else
#include <qpainterpath.h>
#endif

AvatarManager& AvatarManager::inst() {
    static AvatarManager mgr;
    return mgr;
}

static QString avatarSeed(const QString& name, int peer) {
    if (name.isEmpty())
        return QString::number(peer);
    return QString::number(peer) + "|" + name;
}

QPixmap AvatarManager::get(const QString& mxcUrl,
                           const QString& senderName, int peerNumber, int size) {
    if (!mxcUrl.isEmpty()) {
        auto it = m_cache.find(mxcUrl);
        if (it != m_cache.end())
            return it->second;
    }
    return generateIdenticon(avatarSeed(senderName, peerNumber), size);
}

bool AvatarManager::requestDownload(const QString& mxcUrl) {
    if (mxcUrl.isEmpty()) return false;
    if (m_cache.count(mxcUrl)) return false;
    if (m_pending.count(mxcUrl)) return false;
    m_pending.insert(mxcUrl);
    return true;
}

void AvatarManager::store(const QString& mxcUrl, const QPixmap& source, int size) {
    m_pending.erase(mxcUrl);
    if (source.isNull()) {
        return;
    }

#ifdef QT3_BUILD
    QPixmap scaled(source.convertToImage().smoothScale(size, size));
    QBitmap mask(scaled.size(), true);
    QPainter mp(&mask);
    mp.setBrush(Qt::color1);
    mp.setPen(Qt::NoPen);
    mp.drawEllipse(0, 0, size, size);
    mp.end();
    scaled.setMask(mask);
    m_cache[mxcUrl] = scaled;
#else
    QPixmap scaled = source.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QPixmap circle(scaled.size());
    circle.fill(Qt::transparent);
    QPainter p(&circle);
    p.setRenderHint(QPainter::Antialiasing);
    QPainterPath path;
    path.addEllipse(0, 0, size, size);
    p.setClipPath(path);
    p.drawPixmap(0, 0, scaled);
    p.end();
    m_cache[mxcUrl] = circle;
#endif
}

void AvatarManager::clear() {
    m_cache.clear();
    m_pending.clear();
}
