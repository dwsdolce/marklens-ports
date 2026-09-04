#pragma once
#include <QWebEnginePage>

// Routes link clicks instead of letting the view navigate to them: external
// URLs to the system browser, other documents into the viewer.
class MarkdownPage : public QWebEnginePage {
    Q_OBJECT
public:
    using QWebEnginePage::QWebEnginePage;
    void setDocumentPath(const QString &path) { m_documentPath = path; }

signals:
    // The fragment travels with the path: links::documentRelativePath drops
    // it (the shared link contract pins that), but "setup.md#windows-shells"
    // has to land on the heading, not the top of the file.
    void openDocument(const QString &path, const QString &fragment);

protected:
    bool acceptNavigationRequest(const QUrl &url, NavigationType type, bool isMainFrame) override;

private:
    QString m_documentPath;
};
