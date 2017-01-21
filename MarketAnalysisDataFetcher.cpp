/**
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <algorithm>

#include <boost/scope_exit.hpp>

#include <QSettings>
#include <QDebug>

#include "ImportSettings.h"
#include "MarketAnalysisDataFetcher.h"
#include "SSOUtils.h"

namespace Evernus
{
    MarketAnalysisDataFetcher::MarketAnalysisDataFetcher(QByteArray clientId,
                                                         QByteArray clientSecret,
                                                         const EveDataProvider &dataProvider,
                                                         const CharacterRepository &characterRepo,
                                                         QObject *parent)
        : QObject{parent}
        , mDataProvider{dataProvider}
        , mESIManager{std::move(clientId), std::move(clientSecret), mDataProvider, characterRepo}
        , mEveCentralManager{mDataProvider}
    {
        connect(&mESIManager, &ESIManager::error, this, &MarketAnalysisDataFetcher::genericError);
    }

    bool MarketAnalysisDataFetcher::hasPendingOrderRequests() const noexcept
    {
        return !mOrderCounter.isEmpty();
    }

    bool MarketAnalysisDataFetcher::hasPendingHistoryRequests() const noexcept
    {
        return !mHistoryCounter.isEmpty();
    }

    void MarketAnalysisDataFetcher::importData(const ExternalOrderImporter::TypeLocationPairs &pairs,
                                               const MarketOrderRepository::TypeLocationPairs &ignored)
    {
        mPreparingRequests = true;
        BOOST_SCOPE_EXIT(this_) {
            this_->mPreparingRequests = false;
        } BOOST_SCOPE_EXIT_END

        if (mOrderCounter.isEmpty())
        {
            mOrders = std::make_shared<OrderResultType::element_type>();
            mOrderCounter.resetBatch();
        }

        if (mHistoryCounter.isEmpty())
        {
            mHistory = std::make_shared<HistoryResultType::element_type>();
            mHistoryCounter.resetBatch();
        }

        QSettings settings;
        const auto webImporter = static_cast<ImportSettings::WebImporterType>(
            settings.value(ImportSettings::webImportTypeKey, static_cast<int>(ImportSettings::webImportTypeDefault)).toInt());
        const auto marketImportType = static_cast<ImportSettings::MarketOrderImportType>(
            settings.value(ImportSettings::marketOrderImportTypeKey, static_cast<int>(ImportSettings::marketOrderImportTypeDefault)).toInt());
        auto useWholeMarketImport = marketImportType == ImportSettings::MarketOrderImportType::Whole;

        if (!useWholeMarketImport && marketImportType == ImportSettings::MarketOrderImportType::Auto)
            useWholeMarketImport = SSOUtils::useWholeMarketImport(pairs, mDataProvider);

        if (useWholeMarketImport)
        {
            std::unordered_set<uint> regions;
            for (const auto &pair : pairs)
            {
                if (ignored.find(pair) != std::end(ignored))
                    continue;

                mHistoryCounter.incCount();

                mESIManager.fetchMarketHistory(pair.second, pair.first, [=](auto &&history, const auto &error) {
                    processHistory(pair.second, pair.first, std::move(history), error);
                });

                regions.insert(pair.second);
            }

            mOrderCounter.setCount(regions.size());

            for (const auto region : regions)
            {
                mESIManager.fetchMarketOrders(region, [=](std::vector<ExternalOrder> &&orders, const QString &error) {
                    orders.erase(std::remove_if(std::begin(orders), std::end(orders), [&](const auto &order) {
                        return pairs.find(std::make_pair(order.getTypeId(), order.getRegionId())) == std::end(pairs);
                    }), std::end(orders));

                    processOrders(std::move(orders), error);
                });
            }
        }
        else
        {
            for (const auto &pair : pairs)
            {
                if (ignored.find(pair) != std::end(ignored))
                    continue;

                mOrderCounter.incCount();
                mHistoryCounter.incCount();

                if (webImporter == ImportSettings::WebImporterType::EveCentral)
                {
                    mEveCentralManager.fetchMarketOrders(pair.second, pair.first, [=](auto &&orders, const auto &error) {
                        processOrders(std::move(orders), error);
                    });
                }
                else
                {
                    mESIManager.fetchMarketOrders(pair.second, pair.first, [=](auto &&orders, const auto &error) {
                        processOrders(std::move(orders), error);
                    });
                }

                mESIManager.fetchMarketHistory(pair.second, pair.first, [=](auto &&history, const auto &error) {
                    processHistory(pair.second, pair.first, std::move(history), error);
                });
            }
        }

        qDebug() << "Making" << mOrderCounter.getCount() << mHistoryCounter.getCount() << "order and history requests...";

        emit orderStatusUpdated(tr("Waiting for %1 order server replies...").arg(mOrderCounter.getCount()));
        emit historyStatusUpdated(tr("Waiting for %1 history server replies...").arg(mHistoryCounter.getCount()));
    }

    void MarketAnalysisDataFetcher::processOrders(std::vector<ExternalOrder> &&orders, const QString &errorText)
    {
        if (mOrderCounter.advanceAndCheckBatch())
            emit orderStatusUpdated(tr("Waiting for %1 order server replies...").arg(mOrderCounter.getCount()));

        qDebug() << mOrderCounter.getCount() << "orders remaining; error:" << errorText;

        if (!errorText.isEmpty())
        {
            mAggregatedOrderErrors << errorText;

            if (mOrderCounter.isEmpty())
            {
                emit orderImportEnded(mOrders, mAggregatedOrderErrors.join("\n"));
                mAggregatedOrderErrors.clear();
            }

            return;
        }

        mOrders->reserve(mOrders->size() + orders.size());
        mOrders->insert(std::end(*mOrders),
                        std::make_move_iterator(std::begin(orders)),
                        std::make_move_iterator(std::end(orders)));

        if (mOrderCounter.isEmpty() && !mPreparingRequests)
        {
            emit orderImportEnded(mOrders, mAggregatedOrderErrors.join("\n"));
            mAggregatedOrderErrors.clear();
        }
    }

    void MarketAnalysisDataFetcher
    ::processHistory(uint regionId, EveType::IdType typeId, std::map<QDate, MarketHistoryEntry> &&history, const QString &errorText)
    {
        if (mHistoryCounter.advanceAndCheckBatch())
            emit historyStatusUpdated(tr("Waiting for %1 history server replies...").arg(mHistoryCounter.getCount()));

        qDebug() << mHistoryCounter.getCount() << "history remaining; error:" << errorText;

        if (!errorText.isEmpty())
        {
            mAggregatedHistoryErrors << errorText;

            if (mHistoryCounter.isEmpty())
            {
                emit historyImportEnded(mHistory, mAggregatedHistoryErrors.join("\n"));
                mAggregatedHistoryErrors.clear();
            }

            return;
        }

        (*mHistory)[regionId][typeId] = std::move(history);

        if (mHistoryCounter.isEmpty() && !mPreparingRequests)
        {
            emit historyImportEnded(mHistory, mAggregatedHistoryErrors.join("\n"));
            mAggregatedHistoryErrors.clear();
        }
    }
}
