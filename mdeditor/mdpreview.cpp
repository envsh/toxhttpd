#include "mdpreview.h"

extern "C" {
#include "md4c.h"
#include "md4c-html.h"
}
#include <stdlib.h>
#include <string.h>

struct HtmlBuffer {
    char* data;
    size_t size;
    size_t capacity;
};

static void htmlBufInit(HtmlBuffer* buf) {
    buf->capacity = 4096;
    buf->data = (char*)malloc(buf->capacity);
    buf->size = 0;
    buf->data[0] = '\0';
}

static void htmlBufAppend(HtmlBuffer* buf, const char* str, size_t len) {
    while (buf->size + len + 1 > buf->capacity) {
        buf->capacity *= 2;
        buf->data = (char*)realloc(buf->data, buf->capacity);
    }
    memcpy(buf->data + buf->size, str, len);
    buf->size += len;
    buf->data[buf->size] = '\0';
}

static void htmlBufAppendStr(HtmlBuffer* buf, const char* str) {
    htmlBufAppend(buf, str, strlen(str));
}

extern "C" {
static void processOutput(const MD_CHAR* data, MD_SIZE size, void* userdata) {
    HtmlBuffer* buf = static_cast<HtmlBuffer*>(userdata);
    htmlBufAppend(buf, data, size);
}
}

MdPreview::MdPreview(QWidget* parent)
    : QTextBrowser(parent) {
    setReadOnly(true);
}

void MdPreview::setMarkdown(const QString& markdown) {
#ifdef QT3_BUILD
    QByteArray utf8 = markdown.utf8();
    while (!utf8.isEmpty() && utf8.at(utf8.size() - 1) == '\0') {
        utf8.truncate(utf8.size() - 1);
    }
#else
    QByteArray utf8 = markdown.toUtf8();
#endif

    HtmlBuffer buf;
    htmlBufInit(&buf);

    htmlBufAppendStr(&buf,
        "<html><head><style>"
        "body{font-family:sans-serif;font-size:11pt;margin:8px;line-height:1.5;}"
        "h1{font-size:18pt;border-bottom:1px solid #ccc;padding-bottom:4px;}"
        "h2{font-size:16pt;border-bottom:1px solid #eee;padding-bottom:4px;}"
        "h3{font-size:14pt;}"
        "h4,h5,h6{font-size:12pt;}"
        "code{background:#f4f4f4;padding:2px 4px;border-radius:3px;font-family:monospace;}"
        "pre{background:#f4f4f4;padding:8px;border-radius:4px;overflow-x:auto;}"
        "pre code{background:none;padding:0;}"
        "blockquote{border-left:4px solid #ddd;margin:0;padding:4px 12px;color:#666;}"
        "table{border-collapse:collapse;margin:8px 0;}"
        "th,td{border:1px solid #ccc;padding:4px 8px;text-align:left;}"
        "th{background:#f8f8f8;}"
        "a{color:#0366d6;text-decoration:none;}"
        "a:hover{text-decoration:underline;}"
        "img{max-width:100%;}"
        "hr{border:none;border-top:1px solid #ccc;margin:12px 0;}"
        "ul,ol{margin:4px 0;padding-left:24px;}"
        "li{margin:2px 0;}"
        "</style></head><body>");

    unsigned flags = MD_FLAG_TABLES | MD_FLAG_TASKLISTS |
                     MD_FLAG_STRIKETHROUGH | MD_FLAG_PERMISSIVEAUTOLINKS;

#ifdef QT3_BUILD
    md_html(utf8.data(), utf8.size(),
            processOutput, &buf, flags, 0);
#else
    md_html(utf8.constData(), utf8.size(),
            processOutput, &buf, flags, 0);
#endif

    htmlBufAppendStr(&buf, "</body></html>");

    QString html = qFromUtf8(buf.data);
    free(buf.data);

#ifdef QT3_BUILD
    setTextFormat(Qt::RichText);
    setText(html);
#else
    setHtml(html);
#endif
}

void MdPreview::clearPreview() {
    clear();
}

void MdPreview::keyPressEvent(QKeyEvent* e) {
    if (e->key() == Qt::Key_F5) {
        return;
    }
    QTextBrowser::keyPressEvent(e);
}
