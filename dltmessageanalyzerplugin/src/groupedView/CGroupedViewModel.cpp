/**
 * @file    CGroupedViewModel.cpp
 * @author  vgoncharuk
 * @brief   Implementation of the CGroupedViewModel class
 */

#include "QDebug"
#include "QVector"
#include "QApplication"
#include "QDateTime"

#include "CGroupedViewModel.hpp"
#include "../log/CConsoleCtrl.hpp"

static const QString sRootItemName = "Root";

CGroupedViewModel::CGroupedViewModel(QObject *parent)
    : QAbstractItemModel(parent),
      mRegex(),
      mSortingColumn(eGroupedViewColumn::Messages),
      mSortOrder(Qt::SortOrder::DescendingOrder),
      mDuplicatesHandler(),
      mSortingHandler(),
      mFindHandler(),
      mAnalyzedValues()
{   
    mSortingHandler = [this](const QVector<tTreeItemPtr>& children,
                            const int& sortingColumn,
                            Qt::SortOrder sortingOrder) -> QVector<tTreeItemPtr>
    {
        tTreeItem::tChildrenVector result;

        switch(static_cast<eGroupedViewColumn>(sortingColumn))
        {
            case eGroupedViewColumn::SubString:
            {
                struct tComparator
                {
                    QString val;
                    bool operator< (const tComparator& rVal) const
                    {
                        return val.compare( rVal.val, Qt::CaseInsensitive ) < 0;
                    }
                };


                QMultiMap<tComparator, tTreeItemPtr> sortedChildrenMap;

                for(const auto& pChild : children)
                {
                    if(nullptr != pChild)
                    {
                        const QString& subString = pChild->data(sortingColumn).get<QString>();
                        tComparator comparator;
                        comparator.val = subString;
                        sortedChildrenMap.insert(comparator, pChild);
                    }
                }

                for(const auto& pChild : sortedChildrenMap)
                {
                    if(nullptr != pChild)
                    {
                        switch(sortingOrder)
                        {
                            case Qt::SortOrder::AscendingOrder:
                            {
                                result.push_back(pChild);
                            }
                                break;
                            case Qt::SortOrder::DescendingOrder:
                            {
                                result.push_front(pChild);
                            }
                                break;
                        }
                    }
                }
            }
                break;
            case eGroupedViewColumn::Messages:
            case eGroupedViewColumn::MessagesPerSecondAverage:
            case eGroupedViewColumn::Payload:
            case eGroupedViewColumn::PayloadPerSecondAverage:
            {
                QMultiMap<int, tTreeItemPtr> sortedChildrenMap;

                for(const auto& pChild : children)
                {
                    if(nullptr != pChild)
                    {
                        if(static_cast<eGroupedViewColumn>(sortingColumn) == eGroupedViewColumn::PayloadPerSecondAverage)
                        {
                            updateAverageValues(pChild, true, false);
                        }
                        else if(static_cast<eGroupedViewColumn>(sortingColumn) == eGroupedViewColumn::MessagesPerSecondAverage)
                        {
                            updateAverageValues(pChild, false, true);
                        }

                        int statistics = pChild->data(static_cast<int>(sortingColumn)).get<int>();
                        sortedChildrenMap.insert(statistics, pChild);
                    }
                }

                for(const auto& pChild : sortedChildrenMap)
                {
                    if(nullptr != pChild)
                    {
                        switch(sortingOrder)
                        {
                            case Qt::SortOrder::AscendingOrder:
                            {
                                result.push_back(pChild);
                            }
                                break;
                            case Qt::SortOrder::DescendingOrder:
                            {
                                result.push_front(pChild);
                            }
                                break;
                        }
                    }
                }
            }
                break;
            case eGroupedViewColumn::PayloadPercantage:
            case eGroupedViewColumn::MessagesPercantage:
            {
                QMultiMap<double, tTreeItemPtr> sortedChildrenMap;

                for(const auto& pChild : children)
                {
                    if(nullptr != pChild)
                    {
                        if(static_cast<eGroupedViewColumn>(sortingColumn) == eGroupedViewColumn::PayloadPercantage)
                        {
                            updatePercentageValues(pChild, true, false);
                        }
                        else if(static_cast<eGroupedViewColumn>(sortingColumn) == eGroupedViewColumn::MessagesPercantage)
                        {
                            updatePercentageValues(pChild, false, true);
                        }

                        double statistics = pChild->data(static_cast<int>(sortingColumn)).get<double>();
                        sortedChildrenMap.insert(statistics, pChild);
                    }
                }

                for(const auto& pChild : sortedChildrenMap)
                {
                    if(nullptr != pChild)
                    {
                        switch(sortingOrder)
                        {
                            case Qt::SortOrder::AscendingOrder:
                            {
                                result.push_back(pChild);
                            }
                                break;
                            case Qt::SortOrder::DescendingOrder:
                            {
                                result.push_front(pChild);
                            }
                                break;
                        }
                    }
                }
            }
                break;
            case eGroupedViewColumn::AfterLastVisible:
            case eGroupedViewColumn::Metadata:
            case eGroupedViewColumn::Last:
                break;
        }

        return result;
    };

    mDuplicatesHandler =
    [this](CTreeItem* pItem, const CTreeItem::tData& dataItems)
    {
        if(static_cast<int>(eGroupedViewColumn::Messages) < dataItems.size())
        {    
            auto messagesColumn = static_cast<int>(eGroupedViewColumn::Messages);
            auto payloadColumn = static_cast<int>(eGroupedViewColumn::Payload);

            // update of message and payload columns
            {
                pItem->getWriteableData(messagesColumn) = pItem->getWriteableData(messagesColumn).get<int>() + 1;
                pItem->getWriteableData(payloadColumn) = pItem->getWriteableData(payloadColumn).get<int>() + dataItems[payloadColumn].get<int>();
            }

            auto metadataColumn = static_cast<int>(eGroupedViewColumn::Metadata);

            //Important to update metadata in item before further usage.
            pItem->getWriteableData(metadataColumn) = dataItems[metadataColumn];

            auto metadataVariant = pItem->data(metadataColumn);

            auto metadata = metadataVariant.get<tGroupedViewMetadata>();
            const auto& timeStamp = metadata.timeStamp;
            const auto& msgId = metadata.msgId;

            if(mAnalyzedValues.prevMessageTimestamp == 0 || mAnalyzedValues.prevMessageId == msgId)
            {
                mAnalyzedValues.prevMessageTimestamp = timeStamp;
                mAnalyzedValues.prevMessageId = msgId;
            }
            else
            {
                if(false == mAnalyzedValues.perSecondStatisticsDiscarded)
                {
                    if(timeStamp < mAnalyzedValues.prevMessageTimestamp)
                    {
                        mAnalyzedValues.perSecondStatisticsDiscarded = true;

                        SEND_WRN(QString("Timestamps inconsistency between messages : previous message - %1 (%2); current message - %3 (%4). "
                                         "Average statistics will be discarded. "
                                         "Please, filter out specific ECU-s, app-s & search range to preserve consistency of analyzed data.")
                       .arg(mAnalyzedValues.prevMessageId)
                       .arg(mAnalyzedValues.prevMessageTimestamp)
                       .arg(msgId)
                       .arg(timeStamp));
                    }
                    else
                    {
                        mAnalyzedValues.prevMessageTimestamp = timeStamp;
                        mAnalyzedValues.prevMessageId = msgId;
                    }
                }
            }

            if(false == mAnalyzedValues.perSecondStatisticsDiscarded)
            {
                {
                    if(mAnalyzedValues.maxTime == 0u)
                    {
                        mAnalyzedValues.maxTime = timeStamp;
                    }
                    else
                    {
                        if(timeStamp > mAnalyzedValues.maxTime)
                        {
                            mAnalyzedValues.maxTime = timeStamp;
                        }
                    }
                }

                {
                    if(mAnalyzedValues.minTime == 0u)
                    {
                        mAnalyzedValues.minTime = timeStamp;
                    }
                    else
                    {
                        if(timeStamp < mAnalyzedValues.minTime)
                        {
                            mAnalyzedValues.minTime = timeStamp;
                        }
                    }
                }
            }
        }
    };

    mFindHandler = [](const CTreeItem* pItem, const CTreeItem::tData& key) -> CTreeItem::tFindItemResult
    {
        CTreeItem::tFindItemResult result;
        result.bFound = false;

        auto keyColumn = static_cast<int>(eGroupedViewColumn::SubString);

        if(keyColumn < key.size())
        {
            const auto& children = pItem->getChildren();
            const auto& targetKey = key[keyColumn];

            auto foundChild = children.find(targetKey);

            if(foundChild != children.end())
            {
                result.bFound = true;
                result.pItem = *foundChild;
            }
            else
            {
                result.key = targetKey;
            }
        }

        return result;
    };

    mpRootItem = new tTreeItem(nullptr, static_cast<int>(mSortingColumn),
                               mSortingHandler,
                               mDuplicatesHandler, mFindHandler);
    mpRootItem->appendColumn( getName(eGroupedViewColumn::SubString) );
    mpRootItem->appendColumn( getName(eGroupedViewColumn::Messages) );
    mpRootItem->appendColumn( getName(eGroupedViewColumn::MessagesPercantage) );
    mpRootItem->appendColumn( getName(eGroupedViewColumn::MessagesPerSecondAverage) );
    mpRootItem->appendColumn( getName(eGroupedViewColumn::Payload) );
    mpRootItem->appendColumn( getName(eGroupedViewColumn::PayloadPercantage) );
    mpRootItem->appendColumn( getName(eGroupedViewColumn::PayloadPerSecondAverage) );
}

void CGroupedViewModel::updateAverageValues(CTreeItem* pItem, bool updatePayload, bool updateMessages)
{
    if(false == mAnalyzedValues.perSecondStatisticsDiscarded)
    {
        auto delimiter = static_cast<int>(( mAnalyzedValues.maxTime - mAnalyzedValues.minTime ) / 10000);

        if(delimiter > 0)
        {
            if(true == updatePayload)
            {
                pItem->getWriteableData(static_cast<int>(eGroupedViewColumn::PayloadPerSecondAverage)) =
                        pItem->data(static_cast<int>(eGroupedViewColumn::Payload)).get<int>() / delimiter;
            }

            if(true == updateMessages)
            {
                pItem->getWriteableData(static_cast<int>(eGroupedViewColumn::MessagesPerSecondAverage)) =
                        pItem->data(static_cast<int>(eGroupedViewColumn::Messages)).get<int>() / delimiter;
            }
        }
    }
    else
    {
        if(true == updatePayload)
        {
            pItem->getWriteableData(static_cast<int>(eGroupedViewColumn::MessagesPerSecondAverage)) = 0;
        }

        if(true == updateMessages)
        {

            pItem->getWriteableData(static_cast<int>(eGroupedViewColumn::PayloadPerSecondAverage)) = 0;
        }
    }
}

void CGroupedViewModel::updatePercentageValues(CTreeItem* pItem, bool updatePayload, bool updateMessages)
{
    if(true == updatePayload)
    {
        pItem->getWriteableData(static_cast<int>(eGroupedViewColumn::PayloadPercantage)) =
                ( pItem->data(static_cast<int>(eGroupedViewColumn::Payload)).get<int>()
                / static_cast<double>(mAnalyzedValues.analyzedPayload) ) * 100;
    }

    if(true == updateMessages)
    {
        auto msgs = pItem->data(static_cast<int>(eGroupedViewColumn::Messages)).get<int>();
        auto msgsDelimiter = static_cast<double>(mAnalyzedValues.analyzedMessages);
        pItem->getWriteableData(static_cast<int>(eGroupedViewColumn::MessagesPercantage)) =
                msgs / msgsDelimiter * 100;
    }
}

CGroupedViewModel::~CGroupedViewModel()
{
    if(nullptr != mpRootItem)
    {
        delete mpRootItem;
        mpRootItem = nullptr;
    }
}

QModelIndex CGroupedViewModel::rootIndex() const
{
    return QModelIndex();
}

void CGroupedViewModel::updateView()
{
    emit dataChanged( index(0,0), index ( rowCount(), columnCount()) );
    emit layoutChanged();
}

QModelIndex CGroupedViewModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent))
        return QModelIndex();

    tTreeItemPtr pParentItem;

    if (parent == rootIndex())
        pParentItem = mpRootItem;
    else
        pParentItem = static_cast<tTreeItemPtr>(parent.internalPointer());

    tTreeItem *childItem = pParentItem->child(row);
    if (childItem)
        return createIndex(row, column, childItem);
    return QModelIndex();
}

QModelIndex CGroupedViewModel::parent(const QModelIndex &index) const
{
    if (!index.isValid())
        return QModelIndex();

    tTreeItem *childItem = static_cast<tTreeItemPtr>(index.internalPointer());
    tTreeItem *parentItem = childItem->getParent();

    if (parentItem == mpRootItem)
        return rootIndex();
    else
        return createIndex(parentItem->row(), 0, parentItem);
}

int CGroupedViewModel::rowCount(const QModelIndex &parent) const
{
     tTreeItem *parentItem;

    if (parent == rootIndex())
        parentItem = mpRootItem;
    else
        parentItem = static_cast<tTreeItemPtr>(parent.internalPointer());

    parentItem->sort(static_cast<int>(mSortingColumn), mSortOrder, false);

    return parentItem->childCount();
}

int CGroupedViewModel::columnCount(const QModelIndex &) const
{
    return static_cast<int>(eGroupedViewColumn::Last);
}

QVariant CGroupedViewModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    QVariant result;

    if (role == Qt::DisplayRole)
    {
        tTreeItem *pItem = static_cast<tTreeItemPtr>(index.internalPointer());

        if(index.column() == static_cast<int>(eGroupedViewColumn::PayloadPerSecondAverage) ||
           index.column() == static_cast<int>(eGroupedViewColumn::MessagesPerSecondAverage))
        {
            if(index.column() == static_cast<int>(eGroupedViewColumn::MessagesPerSecondAverage))
            {
                const_cast<CGroupedViewModel*>(this)->updateAverageValues(pItem, false, true);
            }
            else if(index.column() == static_cast<int>(eGroupedViewColumn::PayloadPerSecondAverage))
            {
                const_cast<CGroupedViewModel*>(this)->updateAverageValues(pItem, true, false);
            }
        }

        if(index.column() == static_cast<int>(eGroupedViewColumn::MessagesPerSecondAverage) ||
           index.column() == static_cast<int>(eGroupedViewColumn::PayloadPerSecondAverage))
        {
            if(index.column() == static_cast<int>(eGroupedViewColumn::MessagesPerSecondAverage))
            {
                const_cast<CGroupedViewModel*>(this)->updatePercentageValues(pItem, false, true);
            }
            else if(index.column() == static_cast<int>(eGroupedViewColumn::PayloadPerSecondAverage))
            {
                const_cast<CGroupedViewModel*>(this)->updatePercentageValues(pItem, true, false);
            }
        }

        result = toQVariant(pItem->data(index.column()));
    }
    else if (role == Qt::TextAlignmentRole)
    {
        if( static_cast<int>(eGroupedViewColumn::Messages) == index.column() ||
            static_cast<int>(eGroupedViewColumn::MessagesPercantage) == index.column() ||
            static_cast<int>(eGroupedViewColumn::MessagesPerSecondAverage) == index.column() ||
            static_cast<int>(eGroupedViewColumn::Payload) == index.column() ||
            static_cast<int>(eGroupedViewColumn::PayloadPercantage) == index.column() ||
            static_cast<int>(eGroupedViewColumn::PayloadPerSecondAverage) == index.column())
        {
            result = Qt::AlignCenter;
        }
    }

    return result;
}

Qt::ItemFlags CGroupedViewModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;

    return QAbstractItemModel::flags(index);
}

QVariant CGroupedViewModel::headerData(int section, Qt::Orientation orientation,
                               int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
        return toQVariant(mpRootItem->data(section));

    return QVariant();
}

void CGroupedViewModel::addMatches( tFoundMatches& matches, bool update )
{
    if(mpRootItem)
    {   
        if(false == matches.empty())
        {
            tTreeItem::tDataVec dataVec;

            {
                CTreeItem::tData data;
                data.push_back(sRootItemName); /*SubString*/
                data.push_back(tDataItem(1)); /*Messages*/
                data.push_back(tDataItem(0)); /*MessagesPercantage*/
                data.push_back(tDataItem(0)); /*MessagesPerSecond*/
                data.push_back(tDataItem(static_cast<int>(matches[0].msgSizeBytes))); /*Payload*/
                data.push_back(tDataItem(0)); /*PayloadPercantage*/
                data.push_back(tDataItem(0)); /*PayloadPerSecondAverage*/
                data.push_back(tDataItem()); /*AfterLastVisible*/
                data.push_back(tDataItem(tGroupedViewMetadata(matches[0].timeStamp, matches[0].msgId))); /*Metadata*/
                dataVec.push_back( data );

                mAnalyzedValues.analyzedMessages += 1;
                mAnalyzedValues.analyzedPayload += matches[0].msgSizeBytes;
            }

            for(const auto& match : matches)
            {
                CTreeItem::tData data;
                data.push_back(*match.pMatchStr); /*SubString*/
                data.push_back(tDataItem(1)); /*Messages*/
                data.push_back(tDataItem(0)); /*MessagesPercantage*/
                data.push_back(tDataItem(0)); /*MessagesPerSecond*/
                data.push_back(tDataItem(static_cast<int>(match.msgSizeBytes))); /*Payload*/
                data.push_back(tDataItem(0)); /*PayloadPercantage*/
                data.push_back(tDataItem(0)); /*PayloadPerSecondAverage*/
                data.push_back(tDataItem()); /*AfterLastVisible*/
                data.push_back(tDataItem(tGroupedViewMetadata(match.timeStamp, match.msgId))); /*Metadata*/
                dataVec.push_back( data );
            }

            mpRootItem->addData( dataVec );

            if(true == update)
            {
                updateView();
            }
        }
    }
}

void CGroupedViewModel::resetData()
{
    beginResetModel();
    if(mpRootItem)
        delete mpRootItem;
    mpRootItem = new tTreeItem(nullptr,
                               static_cast<int>(mSortingColumn),
                               mSortingHandler,
                               mDuplicatesHandler,
                               mFindHandler);
    mpRootItem->appendColumn( getName(eGroupedViewColumn::SubString) );
    mpRootItem->appendColumn( getName(eGroupedViewColumn::Messages) );
    mpRootItem->appendColumn( getName(eGroupedViewColumn::MessagesPercantage) );
    mpRootItem->appendColumn( getName(eGroupedViewColumn::MessagesPerSecondAverage) );
    mpRootItem->appendColumn( getName(eGroupedViewColumn::Payload) );
    mpRootItem->appendColumn( getName(eGroupedViewColumn::PayloadPercantage) );
    mpRootItem->appendColumn( getName(eGroupedViewColumn::PayloadPerSecondAverage) );
    mAnalyzedValues = tAnalyzedValues();
    endResetModel();
    updateView();
}

QString CGroupedViewModel::exportToHTML()
{
    if(nullptr != mpRootItem)
    {
        mpRootItem->sort(static_cast<int>(mSortingColumn), mSortOrder, true);

        QString finalText;

        QString currentTime = QString().append("[").append(QDateTime::currentDateTime().toString()).append("]");

        finalText.append(QString("<!DOCTYPE html>\n"
                         "<html>\n"
                         "<head>\n"
                         "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
                         "<title>Trace spam analysis report %1</title>\n").arg(currentTime));
        finalText.append("<style>\n"
                         "ul, #myUL {\n"
                         "  list-style-type: none;\n"
                         "}\n"
                         "\n"
                         "#myUL {\n"
                         "  margin: 0;\n"
                         "  padding: 0;\n"
                         "}\n"
                         "\n"
                         ".box {\n"
                         "  cursor: pointer;\n"
                         "  -webkit-user-select: none; /* Safari 3.1+ */\n"
                         "  -moz-user-select: none; /* Firefox 2+ */\n"
                         "  -ms-user-select: none; /* IE 10+ */\n"
                         "  user-select: none;\n"
                         "}\n"
                         "\n"
                         ".box::before {\n"
                         "  content: \"\\2610\";\n"
                         "  color: black;\n"
                         "  display: inline-block;\n"
                         "  margin-right: 6px;\n"
                         "}\n"
                         "\n"
                         ".check-box::before {\n"
                         "color: dodgerblue;\n"
                         "}\n"
                         "\n"
                         ".nested {\n"
                         "  display: none;\n"
                         "}\n"
                         "\n"
                         ".active {\n"
                         "  display: block;\n"
                         "}\n"
                         "</style>\n"
                         "</head>\n"
                         "<body>\n");

        finalText.append( QString("\n<h2>Trace spam analysis report %1</h2>").arg(currentTime) );
        finalText.append( QString("\n<h3>Analysis based on regex: \"").append(mRegex).append("\"</h3>") );
        finalText.append("<ul id=\"myUL\">");

        auto preVisitFunction = [this, &finalText](tTreeItem* pItem)
        {
            if(nullptr != pItem->getParent())
            {
                if(0 != pItem->childCount())
                {
                    finalText.append("<li><span class=\"box\">");
                }

                for( int i = static_cast<int>(eGroupedViewColumn::SubString);
                     i < static_cast<int>(eGroupedViewColumn::AfterLastVisible);
                     ++i )
                {
                    if(i > 0)
                    {
                        finalText.append("\t|\t");
                    }

                    switch(static_cast<eGroupedViewColumn>(i))
                    {
                        case eGroupedViewColumn::SubString:
                        {
                            auto variantVal = pItem->data(i);
                            finalText.append(variantVal
                                             .get<QString>()
                                             .toHtmlEscaped());
                        }
                            break;
                        case eGroupedViewColumn::Messages:
                        case eGroupedViewColumn::MessagesPerSecondAverage:
                        case eGroupedViewColumn::Payload:
                        case eGroupedViewColumn::PayloadPerSecondAverage:
                        {

                            if(static_cast<eGroupedViewColumn>(i) == eGroupedViewColumn::PayloadPerSecondAverage)
                            {
                                updateAverageValues(pItem, true, false);
                            }
                            else if(static_cast<eGroupedViewColumn>(i) == eGroupedViewColumn::MessagesPerSecondAverage)
                            {
                                updateAverageValues(pItem, false, true);
                            }

                            auto variantVal = pItem->data(i);

                            finalText.append(getName(static_cast<eGroupedViewColumn>(i)))
                            .append(" : ")
                            .append(QString::number(variantVal
                            .get<int>()));
                        }
                            break;
                        case eGroupedViewColumn::PayloadPercantage:
                        case eGroupedViewColumn::MessagesPercantage:
                        {
                            if(static_cast<eGroupedViewColumn>(i) == eGroupedViewColumn::PayloadPercantage)
                            {
                                updatePercentageValues(pItem, true, false);
                            }
                            else if(static_cast<eGroupedViewColumn>(i) == eGroupedViewColumn::MessagesPercantage)
                            {
                                updatePercentageValues(pItem, false, true);
                            }

                            auto variantVal = pItem->data(i);

                            finalText.append(getName(static_cast<eGroupedViewColumn>(i)))
                            .append(" : ")
                            .append(QString::number(variantVal
                            .get<double>(), 'f', 3));
                        }
                            break;
                        case eGroupedViewColumn::AfterLastVisible:
                        case eGroupedViewColumn::Metadata:
                        case eGroupedViewColumn::Last:
                            break;
                    }
                }

                finalText.append("\n");

                if(0 != pItem->childCount())
                {
                    finalText.append("</span><ul class=\"nested\">");
                }
            }

            return true;
        };

        auto postVisitFunction = [&finalText](const tTreeItem* pItem)
        {
            if(nullptr != pItem->getParent())
            {
                if(0 != pItem->childCount())
                {
                    finalText.append("</ul>"
                                     "</li>");
                }
                else
                {
                    finalText.append("</li>");
                }
            }

            return true;
        };

        mpRootItem->visit( preVisitFunction, postVisitFunction );

        finalText.append("</ul>"
        ""
        "<script>\n"
        "var toggler = document.getElementsByClassName(\"box\");\n"
        "var i;\n"
        "\n"
        "for (i = 0; i < toggler.length; i++) {\n"
        "toggler[i].addEventListener(\"click\", function() {\n"
        "this.parentElement.querySelector(\".nested\").classList.toggle(\"active\");\n"
        "this.classList.toggle(\"check-box\");\n"
        "});\n"
        "}\n"
        "</script>\n"
        "\n"
        "</body>\n"
        "</html>\n");

        finalText.toHtmlEscaped();

        return finalText;
    }

    return QString();
}

void CGroupedViewModel::setUsedRegex(const QString& regex)
{
    mRegex = regex;
}

void CGroupedViewModel::sort(int column, Qt::SortOrder order)
{
    mSortingColumn = toGroupedViewColumn(column);
    mSortOrder = order;

    if(nullptr != mpRootItem)
    {
        mpRootItem->sort(static_cast<int>(mSortingColumn), mSortOrder, true);
    }

    updateView();
}