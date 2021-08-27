/*
    SPDX-FileCopyrightText: 2006 Pino Toscano <pino@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef BOOKMARKLIST_H
#define BOOKMARKLIST_H

#include <qwidget.h>

#include "core/observer.h"

class QAction;
class QTreeWidget;
class QTreeWidgetItem;
class KTreeWidgetSearchLine;
class QUrl;
class BookmarkItem;
class FileItem;

namespace Okular
{
class Document;
}

class BookmarkList : public QWidget, public Okular::DocumentObserver
{
    Q_OBJECT

public:
    explicit BookmarkList(Okular::Document *document, QAction *addBookmarkAction, QWidget *parent = nullptr);
    ~BookmarkList() override;

    // inherited from DocumentObserver
    void notifySetup(const QVector<Okular::Page *> &pages, int setupFlags) override;

private Q_SLOTS:
    void slotFilterBookmarks(bool);
    void slotExecuted(QTreeWidgetItem *item);
    void slotChanged(QTreeWidgetItem *item);
    void slotContextMenu(const QPoint p);
    void slotBookmarksChanged(const QUrl &url);

private:
    void rebuildTree(bool filter);
    void goTo(BookmarkItem *item);
    void selectiveUrlUpdate(const QUrl &url, QTreeWidgetItem *&item);
    QTreeWidgetItem *itemForUrl(const QUrl &url) const;
    void contextMenuForBookmarkItem(const QPoint p, BookmarkItem *bmItem);
    void contextMenuForFileItem(const QPoint p, FileItem *fItem);

    Okular::Document *m_document;
    QTreeWidget *m_tree;
    KTreeWidgetSearchLine *m_searchLine;
    QAction *m_showBoomarkOnlyAction;
    QTreeWidgetItem *m_currentDocumentItem;
};

#endif
