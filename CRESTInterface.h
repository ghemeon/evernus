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
#pragma once

#include <unordered_set>
#include <functional>
#include <vector>

#include <QNetworkAccessManager>
#include <QDateTime>
#include <QHash>
#include <QUrl>

#include "EveType.h"

class QJsonDocument;

namespace Evernus
{
    class CRESTInterface
        : public QObject
    {
        Q_OBJECT

    public:
        using Callback = std::function<void (QJsonDocument &&data, const QString &error)>;
        using EndpointMap = QHash<QString, QString>;

        static const QString crestUrl;

        using QObject::QObject;
        virtual ~CRESTInterface() = default;

        void fetchBuyMarketOrders(uint regionId, EveType::IdType typeId, const Callback &callback) const;
        void fetchSellMarketOrders(uint regionId, EveType::IdType typeId, const Callback &callback) const;
        void fetchMarketHistory(uint regionId, EveType::IdType typeId, const Callback &callback) const;

        void setEndpoints(EndpointMap endpoints);

    public slots:
        void updateTokenAndContinue(QString token, const QDateTime &expiry);
        void handleTokenError(const QString &error);

    private slots:
        void processSslErrors(const QList<QSslError> &errors);

    signals:
        void tokenRequested() const;

    private:
        using RegionOrderUrlMap = QHash<uint, QUrl>;

        static const QString regionsUrlName;
        static const QString itemTypesUrlName;

        mutable QNetworkAccessManager mNetworkManager;
        EndpointMap mEndpoints;

        mutable QString mAccessToken;
        mutable QDateTime mExpiry;

        mutable std::vector<std::function<void (const QString &)>> mPendingRequests;

        mutable RegionOrderUrlMap mRegionBuyOrdersUrls, mRegionSellOrdersUrls;
        mutable QHash<QPair<uint, QString>, std::vector<std::function<void (const QUrl &, const QString &)>>>
        mPendingRegionRequests;

        template<class T>
        void getRegionBuyOrdersUrl(uint regionId, T &&continuation) const;

        template<class T>
        void getRegionSellOrdersUrl(uint regionId, T &&continuation) const;

        template<class T>
        void getRegionOrdersUrl(uint regionId,
                                const QString &urlName,
                                RegionOrderUrlMap &map,
                                T &&continuation) const;

        template<class T>
        void getOrders(QUrl regionUrl, EveType::IdType typeId, T &&continuation) const;

        template<class T>
        void checkAuth(T &&continuation) const;

        template<class T>
        void fetchAccessToken(const T &continuation) const;

        template<class T>
        void asyncGet(const QUrl &url, const QByteArray &accept, T &&continuation) const;
        QJsonDocument syncGet(const QUrl &url, const QByteArray &accept) const;

        QNetworkRequest prepareRequest(const QUrl &url, const QByteArray &accept) const;
    };
}
