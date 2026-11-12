#pragma once

#include <QString>

class QTextBrowser;
class QTextDocument;
class QWidget;

// Fills `document` with a port's installation guide (Markdown, GitHub dialect) in the colours
// and spacing that the QuestPorts site gives its "Step-by-Step Installation Guide" card.
void renderGuide(QTextDocument* document, const QString& markdown);

// A read-only view of the guide on the site's dark card, with links that open in the browser.
QTextBrowser* makeGuideView(const QString& markdown, QWidget* parent);
