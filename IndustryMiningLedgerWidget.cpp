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
#include <QtDebug>

#include <QRadioButton>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QPushButton>
#include <QGroupBox>
#include <QSettings>
#include <QLabel>
#include <QDate>

#include "LookupActionGroupModelConnector.h"
#include "AdjustableTableView.h"
#include "StationSelectButton.h"
#include "CacheTimerProvider.h"
#include "TypeLocationPairs.h"
#include "PriceTypeComboBox.h"
#include "WarningBarWidget.h"
#include "IndustrySettings.h"
#include "DateRangeWidget.h"
#include "ButtonWithTimer.h"
#include "EveDataProvider.h"
#include "RegionComboBox.h"
#include "ImportSettings.h"
#include "SSOMessageBox.h"
#include "TaskManager.h"
#include "FlowLayout.h"
#include "TimerTypes.h"

#include "IndustryMiningLedgerWidget.h"

namespace Evernus
{
    IndustryMiningLedgerWidget::IndustryMiningLedgerWidget(const CacheTimerProvider &cacheTimerProvider,
                                                           const EveDataProvider &dataProvider,
                                                           const MiningLedgerRepository &ledgerRepo,
                                                           const CharacterRepository &characterRepo,
                                                           TaskManager &taskManager,
                                                           ESIInterfaceManager &interfaceManager,
                                                           QByteArray clientId,
                                                           QByteArray clientSecret,
                                                           QWidget *parent)
        : CharacterBoundWidget{std::bind(&CacheTimerProvider::getLocalCacheTimer, &cacheTimerProvider, std::placeholders::_1, TimerType::MiningLedger),
                               std::bind(&CacheTimerProvider::getLocalUpdateTimer, &cacheTimerProvider, std::placeholders::_1, TimerType::MiningLedger),
                               ImportSettings::maxCharacterAgeKey,parent}
        , mDataProvider{dataProvider}
        , mTaskManager{taskManager}
        , mDetailsModel{mDataProvider, ledgerRepo}
        , mTypesModel{mDataProvider, ledgerRepo}
        , mDataFetcher{std::move(clientId), std::move(clientSecret), mDataProvider, characterRepo, interfaceManager}
    {
        const auto mainLayout = new QVBoxLayout{this};

        const auto toolBarLayout = new FlowLayout{};
        mainLayout->addLayout(toolBarLayout);

        auto &importBtn = getAPIImportButton();
        toolBarLayout->addWidget(&importBtn);

        QSettings settings;

        const auto sellStationPath = settings.value(IndustrySettings::miningLedgerSellStationKey).toList();
        mSellStation = EveDataProvider::getStationIdFromPath(sellStationPath);

        const auto tillDate = QDate::currentDate();
        const auto fromDate = tillDate.addDays(-7);

        mRangeFilter = new DateRangeWidget{this};
        toolBarLayout->addWidget(mRangeFilter);
        mRangeFilter->setRange(fromDate, tillDate);
        connect(mRangeFilter, &DateRangeWidget::rangeChanged, this, &IndustryMiningLedgerWidget::refresh);

        const auto importFromWeb = new QPushButton{QIcon{":/images/world.png"}, tr("Import data"), this};
        toolBarLayout->addWidget(importFromWeb);
        importFromWeb->setFlat(true);
        connect(importFromWeb, &QPushButton::clicked, this, &IndustryMiningLedgerWidget::importData);

        mImportForSourceBtn = new QRadioButton{tr("Import for mined regions"), this};
        toolBarLayout->addWidget(mImportForSourceBtn);
        mImportForSourceBtn->setChecked(true);

        const auto importForSelectedBtn = new QRadioButton{tr("Import for custom regions"), this};
        toolBarLayout->addWidget(importForSelectedBtn);

        mImportRegionsCombo = new RegionComboBox{mDataProvider, IndustrySettings::miningLedgerImportRegionsKey, this};
        toolBarLayout->addWidget(mImportRegionsCombo);
        mImportRegionsCombo->setEnabled(false);

        mSellStationBtn = new StationSelectButton{mDataProvider, sellStationPath, this};
        toolBarLayout->addWidget(mSellStationBtn);
        mSellStationBtn->setEnabled(false);
        connect(mSellStationBtn, &StationSelectButton::stationChanged,
                this, &IndustryMiningLedgerWidget::updateSellStation);

        connect(mImportForSourceBtn, &QRadioButton::toggled, this, [=](auto checked) {
            mImportRegionsCombo->setDisabled(checked);
            mSellStationBtn->setDisabled(checked);
        });

        toolBarLayout->addWidget(new QLabel{tr("Sell price type:"), this});

        mSellPriceTypeCombo = new PriceTypeComboBox{this};
        toolBarLayout->addWidget(mSellPriceTypeCombo);

        auto &warningBar = getWarningBarWidget();
        mainLayout->addWidget(&warningBar);

        const auto contentLayout = new QGridLayout{};
        mainLayout->addLayout(contentLayout);

        const auto detailsGroup = new QGroupBox{tr("Details"), this};
        contentLayout->addWidget(detailsGroup, 0, 0);

        const auto detailsGroupLayout = new QVBoxLayout{detailsGroup};

        mDetailsProxy.setSortRole(Qt::UserRole);
        mDetailsProxy.setSourceModel(&mDetailsModel);

        mTypesProxy.setSortRole(Qt::UserRole);
        mTypesProxy.setSourceModel(&mTypesModel);

        detailsGroupLayout->addWidget(createDataView(mDetailsModel, mDetailsProxy, QStringLiteral("industryMiningLedgerDetailsView")));

        const auto typesGroup = new QGroupBox{tr("Mined types"), this};
        contentLayout->addWidget(typesGroup, 0, 1);

        const auto typesGroupLayout = new QVBoxLayout{typesGroup};

        typesGroupLayout->addWidget(createDataView(mTypesModel, mTypesProxy, QStringLiteral("industryMiningLedgerTypesView")));

        connect(&mDataFetcher, &MarketOrderDataFetcher::orderStatusUpdated,
                this, &IndustryMiningLedgerWidget::updateOrderTask);
        connect(&mDataFetcher, &MarketOrderDataFetcher::orderImportEnded,
                this, &IndustryMiningLedgerWidget::endOrderTask);
        connect(&mDataFetcher, &MarketOrderDataFetcher::genericError,
                this, [=](const auto &text) {
            SSOMessageBox::showMessage(text, this);
        });
    }

    void IndustryMiningLedgerWidget::refresh()
    {
        Q_ASSERT(mRangeFilter != nullptr);

        const auto charId = getCharacterId();
        mDetailsModel.refresh(charId, mRangeFilter->getFrom(), mRangeFilter->getTo());
        mTypesModel.refresh(charId, mRangeFilter->getFrom(), mRangeFilter->getTo());
    }

    void IndustryMiningLedgerWidget::importData()
    {
        const auto types = mDetailsModel.getAllTypes();

        TypeLocationPairs toImport;
        toImport.reserve(types.size());

        if (mImportForSourceBtn->isChecked())
        {
            const auto systems = mDetailsModel.getAllSolarSystems();
            for (const auto system : systems)
            {
                for (const auto type : types)
                    toImport.insert(std::make_pair(type, mDataProvider.getSolarSystemRegionId(system)));
            }
        }
        else
        {
            const auto regions = mImportRegionsCombo->getSelectedRegionList();
            for (const auto region : regions)
            {
                for (const auto type : types)
                    toImport.insert(std::make_pair(type, region));
            }
        }

        if (!mDataFetcher.hasPendingOrderRequests())
            mOrderTask = mTaskManager.startTask(tr("Making %1 order requests...").arg(toImport.size()));

        mDataFetcher.importData(toImport, getCharacterId());
    }

    void IndustryMiningLedgerWidget::updateSellStation(const QVariantList &path)
    {
        QSettings settings;
        settings.setValue(IndustrySettings::miningLedgerSellStationKey, path);

        mSellStation = EveDataProvider::getStationIdFromPath(path);
    }

    void IndustryMiningLedgerWidget::updateOrderTask(const QString &text)
    {
        mTaskManager.updateTask(mOrderTask, text);
    }

    void IndustryMiningLedgerWidget::endOrderTask(const MarketOrderDataFetcher::OrderResultType &orders, const QString &error)
    {
        Q_ASSERT(orders);

        if (error.isEmpty())
        {
            emit updateExternalOrders(*orders);
        }

        mTaskManager.endTask(mOrderTask, error);
    }

    void IndustryMiningLedgerWidget::handleNewCharacter(Character::IdType id)
    {
        qDebug() << "Switching character to" << id;
        refresh();
    }

    void IndustryMiningLedgerWidget::createLookupActions(QAbstractItemView &view,
                                                         ModelWithTypes &model,
                                                         const QSortFilterProxyModel &proxy)
    {
        new LookupActionGroupModelConnector{model, proxy, view, this};
    }

    QWidget *IndustryMiningLedgerWidget::createDataView(ModelWithTypes &model,
                                                        QSortFilterProxyModel &proxy,
                                                        const QString &name)
    {
        const auto view = new AdjustableTableView{name, this};
        view->setModel(&proxy);
        view->setSortingEnabled(true);
        view->setAlternatingRowColors(true);
        view->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        view->setContextMenuPolicy(Qt::ActionsContextMenu);
        view->restoreHeaderState();
        createLookupActions(*view, model, proxy);

        return view;
    }
}
