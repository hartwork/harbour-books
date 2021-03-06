/*
 * Copyright (C) 2015-2016 Jolla Ltd.
 * Contact: Slava Monich <slava.monich@jolla.com>
 *
 * You may use this file under the terms of the BSD license as follows:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Jolla Ltd nor the names of its contributors
 *     may be used to endorse or promote products derived from this
 *     software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "BooksPageWidget.h"
#include "BooksImageProvider.h"
#include "BooksTextStyle.h"
#include "BooksDefs.h"

#include "bookmodel/FBTextKind.h"
#include "image/ZLQtImageManager.h"
#include "ZLStringUtil.h"

#include "HarbourDebug.h"

#include <QPainter>

static const QString IMAGE_URL("image://%1/%2");

// ==========================================================================
// BooksPageWidget::Data
// ==========================================================================

class BooksPageWidget::Data {
public:
    Data(shared_ptr<ZLTextModel> aModel, int aWidth, int aHeight) :
        iModel(aModel), iPaintContext(aWidth, aHeight) {}

    bool paint(QPainter* aPainter);

public:
    shared_ptr<BooksTextView> iView;
    shared_ptr<ZLTextModel> iModel;
    BooksPaintContext iPaintContext;
};

bool BooksPageWidget::Data::paint(QPainter* aPainter)
{
    if (!iView.isNull()) {
        iPaintContext.beginPaint(aPainter);
        iView->paint();
        iPaintContext.endPaint();
        return true;
    }
    return false;
}

// ==========================================================================
// BooksPageWidget::ResetTask
// ==========================================================================

class BooksPageWidget::ResetTask : public BooksTask
{
public:
    ResetTask(shared_ptr<ZLTextModel> aModel,
        shared_ptr<ZLTextStyle> aTextStyle, int aWidth, int aHeight,
        const BooksMargins& aMargins, const BooksPos& aPosition);
    ~ResetTask();

    void performTask();

public:
    BooksPageWidget::Data* iData;
    shared_ptr<ZLTextStyle> iTextStyle;
    BooksMargins iMargins;
    BooksPos iPosition;
};

BooksPageWidget::ResetTask::ResetTask(shared_ptr<ZLTextModel> aModel,
    shared_ptr<ZLTextStyle> aTextStyle, int aWidth, int aHeight,
    const BooksMargins& aMargins, const BooksPos& aPosition) :
    iData(new BooksPageWidget::Data(aModel, aWidth, aHeight)),
    iTextStyle(aTextStyle),
    iMargins(aMargins),
    iPosition(aPosition)
{
}

BooksPageWidget::ResetTask::~ResetTask()
{
    delete iData;
}

void BooksPageWidget::ResetTask::performTask()
{
    if (!isCanceled()) {
        BooksTextView* view = new BooksTextView(iData->iPaintContext,
            iTextStyle, iMargins);
        if (!isCanceled()) {
            view->setModel(iData->iModel);
            if (!isCanceled()) {
                view->gotoPosition(iPosition);
                if (!isCanceled()) {
                    iData->iView = view;
                } else {
                    delete view;
                }
            }
        }
    }
}

// ==========================================================================
// BooksPageWidget::RenderTask
// ==========================================================================

class BooksPageWidget::RenderTask : public BooksTask {
public:
    RenderTask(shared_ptr<BooksPageWidget::Data> aData, int aWidth, int aHeight) :
        iData(aData), iWidth(aWidth), iHeight(aHeight), iImage(NULL) {}

    void performTask();

public:
    shared_ptr<BooksPageWidget::Data> iData;
    int iWidth;
    int iHeight;
    QImage iImage;
};

void BooksPageWidget::RenderTask::performTask()
{
    if (!isCanceled() && !iData.isNull() && !iData->iView.isNull() &&
        iWidth > 0 && iHeight > 0) {
        iImage = QImage(iWidth, iHeight, QImage::Format_RGB32);
        if (!isCanceled()) {
            QPainter painter(&iImage);
            iData->paint(&painter);
        }
    }
}

// ==========================================================================
// BooksPageWidget::FootnoteTask
// ==========================================================================

class BooksPageWidget::FootnoteTask : public BooksTask, ZLTextArea::Properties {
public:
    FootnoteTask(int aX, int aY, int aMaxWidth, int aMaxHeight,
        QString aPath, QString aLinkText, QString aRef,
        shared_ptr<ZLTextModel> aTextModel, shared_ptr<ZLTextStyle> aTextStyle,
        bool aInvertColors) :
        iTextModel(aTextModel), iTextStyle(aTextStyle),
        iInvertColors(aInvertColors), iX(aX), iY(aY),
        iMaxWidth(aMaxWidth), iMaxHeight(aMaxHeight),
        iRef(aRef), iLinkText(aLinkText), iPath(aPath) {}
    ~FootnoteTask();

    void performTask();

    // ZLTextArea::Properties
    shared_ptr<ZLTextStyle> baseStyle() const;
    ZLColor color(const std::string& aStyle) const;
    bool isSelectionEnabled() const;

public:
    shared_ptr<ZLTextModel> iTextModel;
    shared_ptr<ZLTextStyle> iTextStyle;
    bool iInvertColors;
    int iX;
    int iY;
    int iMaxWidth;
    int iMaxHeight;
    QString iRef;
    QString iLinkText;
    QString iPath;
    QImage iImage;
};

BooksPageWidget::FootnoteTask::~FootnoteTask()
{
}

shared_ptr<ZLTextStyle> BooksPageWidget::FootnoteTask::baseStyle() const
{
    return iTextStyle;
}

ZLColor BooksPageWidget::FootnoteTask::color(const std::string& aStyle) const
{
    return BooksPaintContext::realColor(aStyle, iInvertColors);
}

bool BooksPageWidget::FootnoteTask::isSelectionEnabled() const
{
    return false;
}

void BooksPageWidget::FootnoteTask::performTask()
{
    if (!isCanceled()) {
        // Determine the size of the footnote canvas
        ZLTextParagraphCursorCache cache;
        BooksPaintContext sizeContext(iMaxWidth, iMaxHeight);
        ZLTextAreaController sizeController(sizeContext, *this, &cache);
        ZLSize size;
        sizeController.setModel(iTextModel);
        sizeController.preparePaintInfo();
        sizeController.area().paint(&size);
        if (!size.isEmpty() && !isCanceled()) {
            // Now actually paint it
            size.myWidth = (size.myWidth + 3) & -4;
            HDEBUG("footnote size:" << size.myWidth << "x" << size.myHeight);
            cache.clear();
            BooksPaintContext paintContext(size.myWidth, size.myHeight);
            paintContext.setInvertColors(iInvertColors);
            ZLTextAreaController paintController(paintContext, *this, &cache);
            iImage = QImage(size.myWidth, size.myHeight, QImage::Format_RGB32);
            QPainter painter(&iImage);
            paintContext.beginPaint(&painter);
            paintContext.clear(iInvertColors ?
                BooksTextView::INVERTED_BACKGROUND :
                BooksTextView::DEFAULT_BACKGROUND);
            paintController.setModel(iTextModel);
            paintController.preparePaintInfo();
            paintController.area().paint();
            paintContext.endPaint();
        }
    }
}

// ==========================================================================
// BooksPageWidget::PressTask
// ==========================================================================

class BooksPageWidget::PressTask : public BooksTask {
public:
    PressTask(shared_ptr<BooksPageWidget::Data> aData, int aX, int aY) :
        iData(aData), iX(aX), iY(aY), iKind(REGULAR) {}

    void performTask();
    QString getLinkText(ZLTextWordCursor& aCursor);

public:
    shared_ptr<BooksPageWidget::Data> iData;
    int iX;
    int iY;
    QRect iRect;
    ZLTextKind iKind;
    std::string iLink;
    std::string iLinkType;
    std::string iImageId;
    QString iLinkText;
    QImage iImage;
};

QString BooksPageWidget::PressTask::getLinkText(ZLTextWordCursor& aCursor)
{
    QString text;
    while (!aCursor.isEndOfParagraph() && !isCanceled() &&
           aCursor.element().kind() != ZLTextElement::WORD_ELEMENT) {
        aCursor.nextWord();
    }
    while (!aCursor.isEndOfParagraph() && !isCanceled() &&
           aCursor.element().kind() == ZLTextElement::WORD_ELEMENT) {
        const ZLTextWord& word = (ZLTextWord&)aCursor.element();
        if (!text.isEmpty()) text.append(' ');
        text.append(QString::fromUtf8(word.Data, word.Size));
        aCursor.nextWord();
    }
    return text;
}

void BooksPageWidget::PressTask::performTask()
{
    if (!isCanceled()) {
        const BooksTextView& view = *iData->iView;
        const ZLTextArea& area = view.textArea();
        const ZLTextElementRectangle* rect = area.elementByCoordinates(iX, iY);
        if (rect && !isCanceled()) {
            iRect.setLeft(rect->XStart);
            iRect.setRight(rect->XEnd);
            iRect.setTop(rect->YStart);
            iRect.setBottom(rect->YEnd);
            iRect.translate(view.leftMargin(), view.topMargin());
            if (rect->Kind == ZLTextElement::WORD_ELEMENT) {
                ZLTextWordCursor cursor = area.startCursor();
                cursor.moveToParagraph(rect->ParagraphIndex);
                cursor.moveTo(rect->ElementIndex, 0);

                // Basically, the idea is that we are going backwards
                // looking for the START of the link. If we have crossed
                // the END of the linked element, it means that we have
                // missed it. We are recording which stop control elements
                // we have encountered, to avoid making assumptions which
                // ones are links and which are not. We rely on isHyperlink()
                // method to tell us the ultimate truth.
                bool stopped[NUM_KINDS];
                memset(stopped, 0, sizeof(stopped));
                while (!cursor.isStartOfParagraph() && !isCanceled()) {
                    cursor.previousWord();
                    const ZLTextElement& element = cursor.element();
                    if (element.kind() == ZLTextElement::CONTROL_ELEMENT) {
                        const ZLTextControlEntry& entry =
                            ((ZLTextControlElement&)element).entry();
                        ZLTextKind kind = entry.kind();
                        if (kind < NUM_KINDS && !entry.isStart()) {
                            stopped[kind] = true;
                        }
                        if (entry.isHyperlink()) {
                            if (entry.isStart() && !stopped[entry.kind()]) {
                                const ZLTextHyperlinkControlEntry& link =
                                    (ZLTextHyperlinkControlEntry&) entry;
                                iKind = kind;
                                iLink = link.label();
                                iLinkType = link.hyperlinkType();
                                iLinkText = getLinkText(cursor);
                                HDEBUG("link" << kind << iLinkText <<
                                    iLink.c_str());
                            }
                            return;
                        }
                    }
                }
            } else if (rect->Kind == ZLTextElement::IMAGE_ELEMENT) {
                ZLTextWordCursor cursor = area.startCursor();
                cursor.moveToParagraph(rect->ParagraphIndex);
                cursor.moveTo(rect->ElementIndex, 0);
                const ZLTextElement& element = cursor.element();
                HASSERT(element.kind() == ZLTextElement::IMAGE_ELEMENT);
                if (element.kind() == ZLTextElement::IMAGE_ELEMENT) {
                    const ZLTextImageElement& imageElement =
                        (const ZLTextImageElement&)element;
                    shared_ptr<ZLImageData> data = imageElement.image();
                    if (!data.isNull()) {
                        const QImage* image = ((ZLQtImageData&)(*data)).image();
                        if (image && !image->isNull()) {
                            iKind = IMAGE;
                            iImage = *image;
                            iImageId = imageElement.id();
                            HDEBUG("image element" << iImageId.c_str() <<
                                iImage.width() << iImage.height());
                        }
                    }
                }
            }
        }
    }
}

// ==========================================================================
// BooksPageWidget
// ==========================================================================

BooksPageWidget::BooksPageWidget(QQuickItem* aParent) :
    QQuickPaintedItem(aParent),
    iSettings(BooksSettings::sharedInstance()),
    iTaskQueue(BooksTaskQueue::defaultQueue()),
    iTextStyle(BooksTextStyle::defaults()),
    iResizeTimer(new QTimer(this)),
    iModel(NULL),
    iResetTask(NULL),
    iRenderTask(NULL),
    iPressTask(NULL),
    iLongPressTask(NULL),
    iFootnoteTask(NULL),
    iEmpty(false),
    iPage(-1)
{
    connect(iSettings.data(),
        SIGNAL(invertColorsChanged()),
        SLOT(onInvertColorsChanged()));
    setFillColor(qtColor(iSettings->invertColors() ?
        BooksTextView::INVERTED_BACKGROUND :
        BooksTextView::DEFAULT_BACKGROUND));
    setFlag(ItemHasContents, true);
    iResizeTimer->setSingleShot(true);
    iResizeTimer->setInterval(0);
    connect(iResizeTimer, SIGNAL(timeout()), SLOT(onResizeTimeout()));
    connect(this, SIGNAL(widthChanged()), SLOT(onWidthChanged()));
    connect(this, SIGNAL(heightChanged()), SLOT(onHeightChanged()));
}

BooksPageWidget::~BooksPageWidget()
{
    HDEBUG("page" << iPage);
    if (iResetTask) iResetTask->release(this);
    if (iRenderTask) iRenderTask->release(this);
    if (iPressTask) iPressTask->release(this);
    if (iLongPressTask) iLongPressTask->release(this);
    if (iFootnoteTask) iFootnoteTask->release(this);
}

void BooksPageWidget::setModel(BooksBookModel* aModel)
{
    if (iModel != aModel) {
        if (iModel) iModel->disconnect(this);
        iModel = aModel;
        if (iModel) {
#if HARBOUR_DEBUG
            if (iPage >= 0) {
                HDEBUG(iModel->title() << iPage);
            } else {
                HDEBUG(iModel->title());
            }
#endif // HARBOUR_DEBUG
            iTextStyle = iModel->textStyle();
            iPageMark = iModel->pageMark(iPage);
            connect(iModel, SIGNAL(bookModelChanged()),
                SLOT(onBookModelChanged()));
            connect(iModel, SIGNAL(destroyed()),
                SLOT(onBookModelDestroyed()));
            connect(iModel, SIGNAL(pageMarksChanged()),
                SLOT(onBookModelPageMarksChanged()));
            connect(iModel, SIGNAL(loadingChanged()),
                SLOT(onBookModelLoadingChanged()));
            connect(iModel, SIGNAL(textStyleChanged()),
                SLOT(onTextStyleChanged()));
        } else {
            iPageMark.invalidate();
            iTextStyle = BooksTextStyle::defaults();
        }
        resetView();
        Q_EMIT modelChanged();
    }
}

void BooksPageWidget::onTextStyleChanged()
{
    HDEBUG(iPage);
    HASSERT(sender() == iModel);
    iTextStyle = iModel->textStyle();
    resetView();
}

void BooksPageWidget::onInvertColorsChanged()
{
    HDEBUG(iPage);
    HASSERT(sender() == iSettings);
    if (!iData.isNull() && !iData->iView.isNull()) {
        iData->iView->setInvertColors(iSettings->invertColors());
        scheduleRepaint();
    }
}

void BooksPageWidget::onBookModelChanged()
{
    HDEBUG(iModel->title());
    BooksLoadingSignalBlocker block(this);
    iPageMark = iModel->pageMark(iPage);
    resetView();
}

void BooksPageWidget::onBookModelDestroyed()
{
    HDEBUG("model destroyed");
    HASSERT(iModel == sender());
    BooksLoadingSignalBlocker block(this);
    iModel = NULL;
    Q_EMIT modelChanged();
    resetView();
}

void BooksPageWidget::onBookModelPageMarksChanged()
{
    const BooksPos pos = iModel->pageMark(iPage);
    if (iPageMark != pos) {
        BooksLoadingSignalBlocker block(this);
        iPageMark = pos;
        HDEBUG("page" << iPage);
        resetView();
    }
}

void BooksPageWidget::onBookModelLoadingChanged()
{
    BooksLoadingSignalBlocker block(this);
    if (!iModel->loading()) {
        HDEBUG("page" << iPage);
        const BooksPos pos = iModel->pageMark(iPage);
        if (iPageMark != pos) {
            iPageMark = pos;
            resetView();
        }
    }
}

void BooksPageWidget::setPage(int aPage)
{
    if (iPage != aPage) {
        BooksLoadingSignalBlocker block(this);
        iPage = aPage;
        HDEBUG(iPage);
        const BooksPos pos = iModel->pageMark(iPage);
        if (iPageMark != pos) {
            iPageMark = pos;
            resetView();
        }
        resetView();
        Q_EMIT pageChanged();
    }
}

void BooksPageWidget::setLeftMargin(int aMargin)
{
    if (iMargins.iLeft != aMargin) {
        iMargins.iLeft = aMargin;
        HDEBUG(aMargin);
        resetView();
        Q_EMIT leftMarginChanged();
    }
}

void BooksPageWidget::setRightMargin(int aMargin)
{
    if (iMargins.iRight != aMargin) {
        iMargins.iRight = aMargin;
        HDEBUG(aMargin);
        resetView();
        Q_EMIT rightMarginChanged();
    }
}

void BooksPageWidget::setTopMargin(int aMargin)
{
    if (iMargins.iTop != aMargin) {
        iMargins.iTop = aMargin;
        HDEBUG(aMargin);
        resetView();
        Q_EMIT topMarginChanged();
    }
}

void BooksPageWidget::setBottomMargin(int aMargin)
{
    if (iMargins.iBottom != aMargin) {
        iMargins.iBottom = aMargin;
        HDEBUG(aMargin);
        resetView();
        Q_EMIT bottomMarginChanged();
    }
}

void BooksPageWidget::paint(QPainter* aPainter)
{
    if (!iImage.isNull()) {
        HDEBUG("page" << iPage);
        aPainter->drawImage(0, 0, iImage);
        iEmpty = false;
    } else if (iPage >= 0 && iPageMark.valid() && !iData.isNull()) {
        if (!iRenderTask) {
            HDEBUG("page" << iPage << "(scheduled)");
            scheduleRepaint();
        } else {
            HDEBUG("page" << iPage << "(not yet ready)");
        }
        iEmpty = true;
    } else {
        HDEBUG("page" << iPage << "(empty)");
        iEmpty = true;
    }
}

bool BooksPageWidget::loading() const
{
    return iPage >= 0 && (iResetTask || iRenderTask);
}

void BooksPageWidget::resetView()
{
    BooksLoadingSignalBlocker block(this);
    if (iResetTask) {
        iResetTask->release(this);
        iResetTask = NULL;
    }
    if (iPressTask) {
        iPressTask->release(this);
        iPressTask = NULL;
    }
    if (iLongPressTask) {
        iLongPressTask->release(this);
        iLongPressTask = NULL;
    }
    if (iFootnoteTask) {
        iFootnoteTask->release(this);
        iFootnoteTask = NULL;
    }
    iData.reset();
    if (iPage >= 0 && iPageMark.valid() &&
        width() > 0 && height() > 0 && iModel) {
        shared_ptr<ZLTextModel> textModel = iModel->bookTextModel();
        if (!textModel.isNull()) {
            iResetTask = new ResetTask(textModel, iTextStyle,
                width(), height(), iMargins, iPageMark);
            iTaskQueue->submit(iResetTask, this, SLOT(onResetTaskDone()));
            cancelRepaint();
        }
    }
    if (!iResetTask && !iEmpty) {
        update();
    }
}

void BooksPageWidget::cancelRepaint()
{
    BooksLoadingSignalBlocker block(this);
    if (iRenderTask) {
        iRenderTask->release(this);
        iRenderTask = NULL;
    }
}

void BooksPageWidget::scheduleRepaint()
{
    BooksLoadingSignalBlocker block(this);
    cancelRepaint();
    const int w = width();
    const int h = height();
    if (w > 0 && h > 0 && !iData.isNull() && !iData->iView.isNull()) {
        iData->iView->setInvertColors(invertColors());
        iRenderTask = new RenderTask(iData, w, h);
        iTaskQueue->submit(iRenderTask, this, SLOT(onRenderTaskDone()));
    } else {
        update();
    }
}

void BooksPageWidget::onResetTaskDone()
{
    BooksLoadingSignalBlocker block(this);
    HASSERT(sender() == iResetTask);
    iData = iResetTask->iData;
    iResetTask->iData = NULL;
    iResetTask->release(this);
    iResetTask = NULL;
    scheduleRepaint();
}

void BooksPageWidget::onRenderTaskDone()
{
    BooksLoadingSignalBlocker block(this);
    HASSERT(sender() == iRenderTask);
    iImage = iRenderTask->iImage;
    iRenderTask->release(this);
    iRenderTask = NULL;
    update();
}

void BooksPageWidget::onPressTaskDone()
{
    HASSERT(sender() == iPressTask);
    HDEBUG(iPressTask->iKind);

    PressTask* task = iPressTask;
    iPressTask = NULL;

    if (task->iKind != REGULAR) {
        Q_EMIT activeTouch(task->iX, task->iY);
    }

    task->release(this);
}

void BooksPageWidget::onFootnoteTaskDone()
{
    HASSERT(sender() == iFootnoteTask);

    FootnoteTask* task = iFootnoteTask;
    iFootnoteTask = NULL;
    if (!task->iImage.isNull()) {
        // Footnotes with normal and inverted background need to
        // have different ids so that the cached image with the wrong
        // background doesn't show up after we invert the colors
        static const QString NORMAL("n");
        static const QString INVERTED("i");
        static const QString FOOTNOTE_ID("footnote/%1#%2?p=%3&t=%4&s=%5x%6");
        QString id = FOOTNOTE_ID.arg(task->iPath, task->iRef).
            arg(iPage).arg(task->iInvertColors ? INVERTED : NORMAL).
            arg(task->iImage.width()).arg(task->iImage.height());
        QString url = IMAGE_URL.arg(BooksImageProvider::PROVIDER_ID, id);
        HDEBUG(url);
        BooksImageProvider::instance()->addImage(iModel, id, task->iImage);
        Q_EMIT showFootnote(task->iX, task->iY, task->iLinkText, url);
    }

    task->release(this);
}

void BooksPageWidget::onLongPressTaskDone()
{
    HASSERT(sender() == iLongPressTask);
    HDEBUG(iLongPressTask->iKind);

    PressTask* task = iLongPressTask;
    iLongPressTask = NULL;

    if (task->iKind == EXTERNAL_HYPERLINK) {
        static const std::string HTTP("http://");
        static const std::string HTTPS("https://");
        if (ZLStringUtil::stringStartsWith(task->iLink, HTTP) ||
            ZLStringUtil::stringStartsWith(task->iLink, HTTPS)) {
            QString url(QString::fromStdString(task->iLink));
            Q_EMIT browserLinkPressed(url);
        }
    } else if (task->iKind == INTERNAL_HYPERLINK) {
        if (iModel) {
            int page = iModel->linkToPage(task->iLink);
            if (page >= 0) {
                HDEBUG("link to page" << page);
                Q_EMIT jumpToPage(page);
            }
        }
    } else if (task->iKind == FOOTNOTE) {
        if (iModel && task->iLink.length() > 0) {
            std::string ref = task->iLink.substr(1);
            shared_ptr<ZLTextModel> note = iModel->footnoteModel(ref);
            BooksBook* book = iModel->book();
            if (!note.isNull() && book) {
                // Render the footnote
                HDEBUG("footnote" << ref.c_str());
                if (iFootnoteTask) iFootnoteTask->release(this);
                iFootnoteTask = new FootnoteTask(task->iX, task->iY,
                    width()*3/4, height()*10, book->path(), task->iLinkText,
                    QString::fromStdString(ref), note, iTextStyle,
                    iSettings->invertColors());
                iTaskQueue->submit(iFootnoteTask, this,
                    SLOT(onFootnoteTaskDone()));
            }
        }
    } else if (task->iKind == IMAGE) {
        // Make sure that the book path is mixed into the image id to handle
        // the case of different books having images with identical ids
        QString imageId = QString::fromStdString(task->iImageId);
        QString path;
        if (iModel) {
            BooksBook* book = iModel->book();
            if (book) {
                path = book->path();
                if (!path.isEmpty()) {
                    if (!imageId.contains(path)) {
                        QString old = imageId;
                        imageId = path + ":" + old;
                        HDEBUG(old << "-> " << imageId);
                    }
                }
            }
        }
        static const QString IMAGE_ID("image/%1");
        QString id = IMAGE_ID.arg(imageId);
        BooksImageProvider::instance()->addImage(iModel, id, task->iImage);
        Q_EMIT imagePressed(IMAGE_URL.arg(BooksImageProvider::PROVIDER_ID, id),
            task->iRect);
    }

    task->release(this);
}

void BooksPageWidget::updateSize()
{
    HDEBUG("page" << iPage << QSize(width(), height()));
    iImage = QImage();
    resetView();
}

void BooksPageWidget::onWidthChanged()
{
    HDEBUG((int)width());
    // Width change will probably be followed by height change
    iResizeTimer->start();
}

void BooksPageWidget::onHeightChanged()
{
    HDEBUG((int)height());
    if (iResizeTimer->isActive()) {
        // Height is usually changed after width, repaint right away
        iResizeTimer->stop();
        updateSize();
    } else {
        iResizeTimer->start();
    }
}

void BooksPageWidget::onResizeTimeout()
{
    // This can only happen if only width or height has changed. Normally,
    // width change is followed by height change and view is reset from the
    // setHeight() method
    updateSize();
}

void BooksPageWidget::handleLongPress(int aX, int aY)
{
    HDEBUG(aX << aY);
    if (!iResetTask && !iRenderTask && !iData.isNull()) {
        if (iLongPressTask) iLongPressTask->release(this);
        iLongPressTask = new PressTask(iData, aX, aY);
        iTaskQueue->submit(iLongPressTask, this, SLOT(onLongPressTaskDone()));
    }
}

void BooksPageWidget::handlePress(int aX, int aY)
{
    HDEBUG(aX << aY);
    if (!iResetTask && !iRenderTask && !iData.isNull()) {
        if (iPressTask) iPressTask->release(this);
        iPressTask = new PressTask(iData, aX, aY);
        iTaskQueue->submit(iPressTask, this, SLOT(onPressTaskDone()));
    }
}
