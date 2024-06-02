/*
    SPDX-FileCopyrightText: 2017 Julian Wolff <wolff@julianwolff.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "converter.h"

#include <KLocalizedString>

#include <QDomDocument>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextFrame>
#include <QTextStream>

#include <core/action.h>

#include "debug_md.h"

extern "C" {
#include <mkdio.h>
}

#define PAGE_WIDTH 980
#define PAGE_HEIGHT 1307
#define PAGE_MARGIN 45
#define CONTENT_WIDTH (PAGE_WIDTH - 2 * PAGE_MARGIN)

using namespace Markdown;

static void recursiveRenameTags(QDomElement &elem)
{
    for (QDomNode node = elem.firstChild(); !node.isNull(); node = node.nextSibling()) {
        QDomElement child = node.toElement();
        if (!child.isNull()) {
            // Discount emits <del> tags for ~~ but Qt doesn't understand them and
            // removes them. Instead replace them with <s> tags which Qt does
            // understand.
            if (child.nodeName() == QStringLiteral("del")) {
                child.setTagName(QStringLiteral("s"));
            }
            recursiveRenameTags(child);
        }
    }
}

QString detail::fixupHtmlTags(QString &&html)
{
    QDomDocument dom;
    // Discount emits simplified HTML but QDomDocument will barf if everything isn't
    // inside a "root" node. Luckily QTextDocument ignores unknown tags so we can just
    // wrap the original HTML with a fake tag that'll be stripped off later.
    if (!dom.setContent(QStringLiteral("<ignored_by_qt>") + html + QStringLiteral("</ignored_by_qt>"))) {
        return std::move(html);
    }
    QDomElement elem = dom.documentElement();
    recursiveRenameTags(elem);
    // Don't add any indentation otherwise code blocks can gain indents.
    return dom.toString(-1);
}

Converter::Converter()
    : m_markdownFile(nullptr)
    , m_isFancyPantsEnabled(true)
{
}

Converter::~Converter()
{
    if (m_markdownFile) {
        fclose(m_markdownFile);
    }
}

QTextDocument *Converter::convert(const QString &fileName)
{
    if (m_markdownFile) {
        fclose(m_markdownFile);
    }
    m_markdownFile = fopen(fileName.toLocal8Bit().constData(), "rb");
    if (!m_markdownFile) {
        Q_EMIT error(i18n("Failed to open the document"), -1);
        return nullptr;
    }

    m_fileDir = QDir(fileName.left(fileName.lastIndexOf(QLatin1Char('/'))));

    QTextDocument *doc = convertOpenFile();
    QHash<QString, QTextFragment> internalLinks;
    QHash<QString, QTextBlock> documentAnchors;
    extractLinks(doc->rootFrame(), internalLinks, documentAnchors);

    for (auto linkIt = internalLinks.constBegin(); linkIt != internalLinks.constEnd(); ++linkIt) {
        auto anchorIt = documentAnchors.constFind(linkIt.key());
        if (anchorIt != documentAnchors.constEnd()) {
            const Okular::DocumentViewport viewport = calculateViewport(doc, anchorIt.value());
            Okular::GotoAction *action = new Okular::GotoAction(QString(), viewport);
            Q_EMIT addAction(action, linkIt.value().position(), linkIt.value().position() + linkIt.value().length());
        } else {
            qDebug() << "Could not find destination for" << linkIt.key();
        }
    }

    return doc;
}

void Converter::convertAgain()
{
    setDocument(convertOpenFile());
}

QTextDocument *Converter::convertOpenFile()
{
    int result = fseek(m_markdownFile, 0, SEEK_SET);
    if (result != 0) {
        Q_EMIT error(i18n("Failed to open the document"), -1);
        return nullptr;
    }

#if defined(MKD_NOLINKS)
    // on discount 2 MKD_NOLINKS is a define
    MMIOT *markdownHandle = mkd_in(m_markdownFile, 0);

    int flags = MKD_FENCEDCODE | MKD_GITHUBTAGS | MKD_AUTOLINK | MKD_TOC | MKD_IDANCHOR;
    if (!m_isFancyPantsEnabled) {
        flags |= MKD_NOPANTS;
    }
    if (!mkd_compile(markdownHandle, flags)) {
        Q_EMIT error(i18n("Failed to compile the Markdown document."), -1);
        return nullptr;
    }
#else
    // on discount 3 MKD_NOLINKS is an enum value
    MMIOT *markdownHandle = mkd_in(m_markdownFile, nullptr);

    mkd_flag_t *flags = mkd_flags();
    // These flags aren't bitflags, so they can't be | together
    mkd_set_flag_num(flags, MKD_FENCEDCODE);
    mkd_set_flag_num(flags, MKD_GITHUBTAGS);
    mkd_set_flag_num(flags, MKD_AUTOLINK);
    mkd_set_flag_num(flags, MKD_TOC);
    mkd_set_flag_num(flags, MKD_IDANCHOR);
    if (!m_isFancyPantsEnabled) {
        mkd_set_flag_num(flags, MKD_NOPANTS);
    }
    if (!mkd_compile(markdownHandle, flags)) {
        Q_EMIT error(i18n("Failed to compile the Markdown document."), -1);
        mkd_free_flags(flags);
        return nullptr;
    }
    mkd_free_flags(flags);
#endif

    char *htmlDocument;
    const int size = mkd_document(markdownHandle, &htmlDocument);

    const QString html = detail::fixupHtmlTags(QString::fromUtf8(htmlDocument, size));

    QTextDocument *textDocument = new QTextDocument;
    textDocument->setPageSize(QSizeF(PAGE_WIDTH, PAGE_HEIGHT));
    textDocument->setHtml(html);
    if (generator()) {
        textDocument->setDefaultFont(generator()->generalSettings()->font());
    }

    mkd_cleanup(markdownHandle);

    QTextFrameFormat frameFormat;
    frameFormat.setMargin(PAGE_MARGIN);

    QTextFrame *rootFrame = textDocument->rootFrame();
    rootFrame->setFrameFormat(frameFormat);

    convertImages(rootFrame, m_fileDir, textDocument);

    return textDocument;
}

void Converter::extractLinks(QTextFrame *parent, QHash<QString, QTextFragment> &internalLinks, QHash<QString, QTextBlock> &documentAnchors)
{
    for (QTextFrame::iterator it = parent->begin(); !it.atEnd(); ++it) {
        QTextFrame *textFrame = it.currentFrame();
        const QTextBlock textBlock = it.currentBlock();

        if (textFrame) {
            extractLinks(textFrame, internalLinks, documentAnchors);
        } else if (textBlock.isValid()) {
            extractLinks(textBlock, internalLinks, documentAnchors);
        }
    }
}

void Converter::extractLinks(const QTextBlock &parent, QHash<QString, QTextFragment> &internalLinks, QHash<QString, QTextBlock> &documentAnchors)
{
    for (QTextBlock::iterator it = parent.begin(); !it.atEnd(); ++it) {
        const QTextFragment textFragment = it.fragment();
        if (textFragment.isValid()) {
            const QTextCharFormat textCharFormat = textFragment.charFormat();
            if (textCharFormat.isAnchor()) {
                const QString href = textCharFormat.anchorHref();
                if (href.startsWith(QLatin1Char('#'))) { // It's an internal link, store it and we'll resolve it at the end
                    internalLinks.insert(href.mid(1), textFragment);
                } else {
                    Okular::BrowseAction *action = new Okular::BrowseAction(QUrl(textCharFormat.anchorHref()));
                    Q_EMIT addAction(action, textFragment.position(), textFragment.position() + textFragment.length());
                }

                const QStringList anchorNames = textCharFormat.anchorNames();
                for (const QString &anchorName : anchorNames) {
                    documentAnchors.insert(anchorName, parent);
                }
            }
        }
    }
}

void Converter::convertImages(QTextFrame *parent, const QDir &dir, QTextDocument *textDocument)
{
    for (QTextFrame::iterator it = parent->begin(); !it.atEnd(); ++it) {
        QTextFrame *textFrame = it.currentFrame();
        const QTextBlock textBlock = it.currentBlock();

        if (textFrame) {
            convertImages(textFrame, dir, textDocument);
        } else if (textBlock.isValid()) {
            convertImages(textBlock, dir, textDocument);
        }
    }
}

void Converter::convertImages(const QTextBlock &parent, const QDir &dir, QTextDocument *textDocument)
{
    for (QTextBlock::iterator it = parent.begin(); !it.atEnd(); ++it) {
        const QTextFragment textFragment = it.fragment();
        if (textFragment.isValid()) {
            const QTextCharFormat textCharFormat = textFragment.charFormat();
            if (textCharFormat.isImageFormat()) {
                QTextImageFormat format;

                const qreal specifiedHeight = textCharFormat.toImageFormat().height();
                const qreal specifiedWidth = textCharFormat.toImageFormat().width();

                QTextCursor cursor(textDocument);
                cursor.setPosition(textFragment.position(), QTextCursor::MoveAnchor);
                cursor.setPosition(textFragment.position() + textFragment.length(), QTextCursor::KeepAnchor);

                const QString imageFilePath = QDir::cleanPath(dir.absoluteFilePath(QUrl::fromPercentEncoding(textCharFormat.toImageFormat().name().toUtf8())));

                if (QFile::exists(imageFilePath)) {
                    cursor.removeSelectedText();
                    format.setName(imageFilePath);
                    const QImage img = QImage(format.name());

                    setImageSize(format, specifiedWidth, specifiedHeight, img.width(), img.height());

                    cursor.insertImage(format);
                } else if ((!textCharFormat.toImageFormat().property(QTextFormat::ImageAltText).toString().isEmpty())) {
                    cursor.insertText(textCharFormat.toImageFormat().property(QTextFormat::ImageAltText).toString());
                }
            }
        }
    }
}

void Converter::setImageSize(QTextImageFormat &format, const qreal specifiedWidth, const qreal specifiedHeight, const qreal originalWidth, const qreal originalHeight)
{
    qreal width = 0;
    qreal height = 0;

    const bool hasSpecifiedSize = specifiedHeight > 0 || specifiedWidth > 0;
    if (hasSpecifiedSize) {
        width = specifiedWidth;
        height = specifiedHeight;
        if (width == 0 && originalHeight > 0) {
            width = originalWidth * height / originalHeight;
        } else if (height == 0 && originalWidth > 0) {
            height = originalHeight * width / originalWidth;
        }
    } else {
        width = originalWidth;
        height = originalHeight;
    }

    if (width > CONTENT_WIDTH) {
        height = height * CONTENT_WIDTH / width;
        width = CONTENT_WIDTH;
    }
    format.setWidth(width);
    format.setHeight(height);
}
