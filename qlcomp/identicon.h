#ifndef IDENTICON_H
#define IDENTICON_H

#include <qpixmap.h>
#include <qstring.h>

QPixmap generateIdenticon(const QString& seed, int size);

#endif
