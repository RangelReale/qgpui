#include "gamestores/Vdf.h"

namespace gamestores {

namespace {

class Parser
{
public:
    explicit Parser(const QString& text)
        : m_text(text)
    {}

    std::optional<VdfObject> parse(QString* errorMessage)
    {
        VdfObject root;
        if (!parseObject(root, /*topLevel=*/true)) {
            if (errorMessage)
                *errorMessage = m_error;
            return std::nullopt;
        }
        return root;
    }

private:
    bool atEnd() const { return m_pos >= m_text.size(); }
    QChar peek() const { return m_text.at(m_pos); }

    // Skips white space and // comments.
    void skipSpace()
    {
        while (!atEnd()) {
            if (peek().isSpace()) {
                ++m_pos;
            } else if (peek() == u'/' && m_pos + 1 < m_text.size() && m_text.at(m_pos + 1) == u'/') {
                while (!atEnd() && peek() != u'\n')
                    ++m_pos;
            } else {
                return;
            }
        }
    }

    // Skips a conditional such as [$WIN32] after a key or value.
    void skipConditional()
    {
        skipSpace();
        if (!atEnd() && peek() == u'[') {
            while (!atEnd() && peek() != u']')
                ++m_pos;
            if (!atEnd())
                ++m_pos;
        }
    }

    bool fail(const QString& message)
    {
        m_error = QStringLiteral("%1 at offset %2.").arg(message).arg(m_pos);
        return false;
    }

    // Reads a quoted or unquoted token at the current position.
    bool readToken(QString& out)
    {
        out.clear();
        if (peek() == u'"') {
            ++m_pos;
            while (true) {
                if (atEnd())
                    return fail(QStringLiteral("Unterminated string"));
                const QChar c = peek();
                ++m_pos;
                if (c == u'"')
                    return true;
                if (c == u'\\' && !atEnd()) {
                    const QChar e = peek();
                    ++m_pos;
                    if (e == u'n')
                        out += u'\n';
                    else if (e == u't')
                        out += u'\t';
                    else
                        out += e; // \\ and \" and anything else: the character itself.
                    continue;
                }
                out += c;
            }
        }
        while (!atEnd() && !peek().isSpace() && peek() != u'{' && peek() != u'}' && peek() != u'"')
            out += m_text.at(m_pos++);
        return true;
    }

    bool parseObject(VdfObject& out, bool topLevel)
    {
        while (true) {
            skipSpace();
            if (atEnd())
                return topLevel ? true : fail(QStringLiteral("Missing }"));
            if (peek() == u'}') {
                if (topLevel)
                    return fail(QStringLiteral("Unexpected }"));
                ++m_pos;
                return true;
            }
            if (peek() == u'{')
                return fail(QStringLiteral("Unexpected { without a key"));

            QString key;
            if (!readToken(key))
                return false;
            skipConditional();
            skipSpace();
            if (atEnd())
                return fail(QStringLiteral("Missing value for \"%1\"").arg(key));

            if (peek() == u'{') {
                ++m_pos;
                VdfObject child;
                if (!parseObject(child, /*topLevel=*/false))
                    return false;
                out.insert(key.toLower(), child);
            } else if (peek() == u'}') {
                return fail(QStringLiteral("Missing value for \"%1\"").arg(key));
            } else {
                QString value;
                if (!readToken(value))
                    return false;
                out.insert(key.toLower(), value);
            }
            skipConditional();
        }
    }

    const QString m_text;
    qsizetype m_pos = 0;
    QString m_error;
};

} // namespace

std::optional<VdfObject> parseVdf(const QByteArray& text, QString* errorMessage)
{
    return Parser(QString::fromUtf8(text)).parse(errorMessage);
}

QString vdfString(const VdfObject& object, const QStringList& path)
{
    if (path.isEmpty())
        return {};
    const VdfObject parent = vdfObject(object, path.first(path.size() - 1));
    const QVariant value = parent.value(path.last().toLower());
    return value.typeId() == QMetaType::QString ? value.toString() : QString();
}

VdfObject vdfObject(const VdfObject& object, const QStringList& path)
{
    VdfObject current = object;
    for (const QString& key : path) {
        const QVariant value = current.value(key.toLower());
        if (value.typeId() != QMetaType::QVariantMap)
            return {};
        current = value.toMap();
    }
    return current;
}

} // namespace gamestores
