/**
 * @file    CSearchResultModel.hpp
 * @author  vgoncharuk
 * @brief   Declaration of the CSearchResultModel class
 */

#pragma once

#include "QList"
#include "QPair"
#include "QModelIndex"
#include "QWidget"

#include "common/Definitions.hpp"

#include "../api/ISearchResultModel.hpp"

class CSearchResultModel : public QAbstractTableModel, public ISearchResultModel
{
    Q_OBJECT

public:

    CSearchResultModel(QObject *parent=nullptr);

    // implementation of the ISearchResultModel
    void setFile(const tDLTFileWrapperPtr& pFile) override;
    void updateView(const int& fromRow = 0) override;
    void resetData() override;
    int getFileIdx( const QModelIndex& idx ) const override;
    std::pair<bool, tIntRange> addNextMessageIdxVec(const tFoundMatchesPack& foundMatchesPack) override;
    std::pair<int /*rowNumber*/, QString /*diagramContent*/> getUMLDiagramContent() const override;
    // implementation of the ISearchResultModel ( END )

    int rowCount(const QModelIndex &parent) const override;
    int columnCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    const tFoundMatchesPackItem& getFoundMatchesItemPack( const QModelIndex& modelIndex ) const;

    void setUML_Applicability( const QModelIndex& index, bool checked );

    QString getStrValue(const int& row, const eSearchResultColumn& column) const;

private:

    tQStringPtr getDataStrFromMsg(const tMsgId& msgId, const tDLTMsgWrapperPtr &pMsg, eSearchResultColumn field) const;

private:

    tFoundMatchesPack mFoundMatchesPack;
    tDLTFileWrapperPtr mpFile;
};