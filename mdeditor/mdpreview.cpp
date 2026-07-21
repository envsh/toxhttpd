#include "mdpreview.h"

extern "C" {
#include "md4c.h"
#include "md4c-html.h"
}

struct HtmlBuffer {
    QString html;
};

static void processOutput(const MD_CHAR* data, MD_SIZE size, void* userdata) {
    HtmlBuffer* buf = static_cast<HtmlBuffer*>(userdata);
    buf->html.append(QString::fromUtf8(data, size));
}

MdPreview::MdPreview(QWidget* parent)
    : QTextBrowser(parent) {
    setReadOnly(true);
}

void MdPreview::setMarkdown(const QString& markdown) {
#ifdef QT3_BUILD
    QByteArray utf8 = markdown.utf8();
    // Qt3 QString::utf8() 末尾可能带 NUL 字节，md4c 会将其当 MD_TEXT_NULLCHAR
    // 输出 U+FFFD (�)，去掉避免预览尾部出现乱码替换字符
    while (!utf8.isEmpty() && utf8.at(utf8.size() - 1) == '\0') {
        utf8.truncate(utf8.size() - 1);
    }
#else
    QByteArray utf8 = markdown.toUtf8();
#endif

    HtmlBuffer buf;
    buf.html = "<html><head><style>"
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
        "</style></head><body>";

    unsigned flags = MD_FLAG_TABLES | MD_FLAG_TASKLISTS |
                     MD_FLAG_STRIKETHROUGH | MD_FLAG_PERMISSIVEAUTOLINKS;

#ifdef QT3_BUILD
    md_html(utf8.data(), utf8.size(),
            processOutput, &buf, flags, 0);
#else
    md_html(utf8.constData(), utf8.size(),
            processOutput, &buf, flags, 0);
#endif

    buf.html += "</body></html>";

#ifdef QT3_BUILD
    setTextFormat(Qt::RichText);
    setText(buf.html);
#else
    setHtml(buf.html);
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
