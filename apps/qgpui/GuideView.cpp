#include "GuideView.h"

#include <QColor>
#include <QFontInfo>
#include <QList>
#include <QPalette>
#include <QTextBlock>
#include <QTextBrowser>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextFragment>
#include <QTextList>

namespace {

// The site's colours (QuestPorts, app/assets/css/main.css, .guide-content).
const QColor kCard(15, 21, 35);         // the card behind the guide
const QColor kHeading(248, 250, 252);   // h1, h2 and bold text in list items
const QColor kAccent(14, 165, 233);     // h3, inline code and links
const QColor kText(148, 163, 184);      // paragraphs and lists
const QColor kCodeBack(19, 26, 42);     // inline code
const QColor kCodeBlockBack(24, 32, 51); // code blocks; the site draws a border, which Qt cannot

struct Span
{
    int start;
    int length;
    QTextCharFormat format;
};

bool isCodeBlock(const QTextBlock& block)
{
    const QTextBlockFormat format = block.blockFormat();
    return format.hasProperty(QTextFormat::BlockCodeFence) || format.hasProperty(QTextFormat::BlockCodeLanguage)
        || format.nonBreakableLines();
}

bool hasFixedPitchText(const QTextBlock& block)
{
    for (auto it = block.begin(); !it.atEnd(); ++it) {
        if (it.fragment().isValid() && !it.fragment().text().trimmed().isEmpty())
            return QFontInfo(it.fragment().charFormat().font()).fixedPitch();
    }
    return true;
}

// Qt 6.8's Markdown importer mishandles a fenced code block inside a list item: the code block
// stays a list item, with its own bullet, and the next item takes the code block's format. Take
// the code block out of the list, indented like the item's text, and give the next item back
// its plain format. The importer sets the item's text in a proportional font, which tells the
// two apart.
void repairListCodeBlocks(QTextDocument* document, QTextCursor& cursor)
{
    for (QTextBlock block = document->begin(); block.isValid(); block = block.next()) {
        QTextList* list = block.textList();
        if (!list || !isCodeBlock(block))
            continue;
        QTextBlockFormat format = block.blockFormat();
        cursor.setPosition(block.position());
        if (hasFixedPitchText(block)) {
            list->remove(block);
            format.setObjectIndex(-1);
            format.setIndent(list->format().indent());
        } else {
            format.clearProperty(QTextFormat::BlockCodeFence);
            format.clearProperty(QTextFormat::BlockCodeLanguage);
            format.clearProperty(QTextFormat::BlockNonBreakableLines);
        }
        cursor.setBlockFormat(format);
    }
}

} // namespace

void renderGuide(QTextDocument* document, const QString& markdown)
{
    document->setMarkdown(markdown, QTextDocument::MarkdownDialectGitHub);

    QTextCursor cursor(document);
    cursor.beginEditBlock();
    repairListCodeBlocks(document, cursor);

    QList<Span> spans;
    for (QTextBlock block = document->begin(); block.isValid(); block = block.next()) {
        QTextBlockFormat blockFormat = block.blockFormat();
        const int heading = blockFormat.headingLevel();
        const bool code = isCodeBlock(block);
        const bool listItem = block.textList() != nullptr;

        if (heading > 0) {
            blockFormat.setTopMargin(18);
            blockFormat.setBottomMargin(9);
        } else if (code) {
            blockFormat.setBackground(kCodeBlockBack);
            blockFormat.setLeftMargin(blockFormat.leftMargin() + 4);
        } else {
            blockFormat.setLineHeight(150, QTextBlockFormat::ProportionalHeight);
            blockFormat.setBottomMargin(listItem ? 4 : 10);
        }
        cursor.setPosition(block.position());
        cursor.setBlockFormat(blockFormat);

        // Qt draws a "*" list with circles; the site draws discs.
        if (QTextList* list = block.textList()) {
            QTextListFormat listFormat = list->format();
            if (listFormat.style() == QTextListFormat::ListCircle || listFormat.style() == QTextListFormat::ListSquare) {
                listFormat.setStyle(QTextListFormat::ListDisc);
                list->setFormat(listFormat);
            }
        }

        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            if (!fragment.isValid())
                continue;
            QTextCharFormat format;
            const QTextCharFormat current = fragment.charFormat();
            if (heading > 0) {
                format.setForeground(heading >= 3 ? kAccent : kHeading);
                format.setFontWeight(QFont::Bold);
            } else if (current.isAnchor()) {
                format.setForeground(kAccent);
                format.setFontUnderline(true);
            } else if (code) {
                format.setForeground(kHeading);
            } else if (current.fontFixedPitch()) {
                format.setForeground(kAccent);
                format.setBackground(kCodeBack);
            } else if (listItem && current.fontWeight() >= QFont::Bold) {
                format.setForeground(kHeading);
            } else {
                format.setForeground(kText);
            }
            spans.append({fragment.position(), fragment.length(), format});
        }
    }

    // Apply after the walk: merging formats splits and joins fragments.
    for (const Span& span : spans) {
        cursor.setPosition(span.start);
        cursor.setPosition(span.start + span.length, QTextCursor::KeepAnchor);
        cursor.mergeCharFormat(span.format);
    }
    cursor.endEditBlock();
}

QTextBrowser* makeGuideView(const QString& markdown, QWidget* parent)
{
    auto* view = new QTextBrowser(parent);
    view->setOpenExternalLinks(true);
    view->setStyleSheet(QStringLiteral("QTextBrowser { background: %1; border: 1px solid rgb(30, 41, 59); }")
                            .arg(kCard.name()));
    // List numbers and bullets take the palette's text colour, not the item's format.
    QPalette palette = view->palette();
    palette.setColor(QPalette::Text, kText);
    palette.setColor(QPalette::Base, kCard);
    view->setPalette(palette);
    view->document()->setDocumentMargin(16);
    renderGuide(view->document(), markdown);
    // The view's own cursor moves past the text as it is set, and the view would open scrolled to
    // the end: put it back at the start.
    view->moveCursor(QTextCursor::Start);
    return view;
}
