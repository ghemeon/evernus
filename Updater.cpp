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
#include <QDesktopServices>
#include <QCoreApplication>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QDomDocument>
#include <QMessageBox>
#include <QSettings>
#include <QDebug>
#include <QUrl>

#include "CorpMarketOrderValueSnapshotRepository.h"
#include "MarketOrderValueSnapshotRepository.h"
#include "WalletJournalEntryRepository.h"
#include "WalletTransactionRepository.h"
#include "ExternalOrderRepository.h"
#include "MarketOrderRepository.h"
#include "UpdateTimerRepository.h"
#include "CacheTimerRepository.h"
#include "EvernusApplication.h"
#include "ImportSettings.h"
#include "PriceSettings.h"
#include "PathSettings.h"

#include "Updater.h"

namespace Evernus
{
    void Updater::performVersionMigration(const CacheTimerRepository &cacheTimerRepo,
                                          const UpdateTimerRepository &updateTimerRepo,
                                          const Repository<Character> &characterRepo,
                                          const ExternalOrderRepository &externalOrderRepo,
                                          const MarketOrderRepository &characterOrderRepo,
                                          const MarketOrderRepository &corporationOrderRepo,
                                          const WalletJournalEntryRepository &walletJournalRepo,
                                          const WalletJournalEntryRepository &corpWalletJournalRepo,
                                          const WalletTransactionRepository &walletTransactionRepo,
                                          const WalletTransactionRepository &corpWalletTransactionRepo,
                                          const MarketOrderValueSnapshotRepository &orderValueSnapshotRepo,
                                          const CorpMarketOrderValueSnapshotRepository &corpOrderValueSnapshotRepo) const
    {
        QSettings settings;

        const auto curVersion
            = settings.value(EvernusApplication::versionKey, QCoreApplication::applicationVersion()).toString().split('.');
        if (curVersion.size() != 2)
            return;

        const auto majorVersion = curVersion[0].toUInt();
        const auto minorVersion = curVersion[1].toUInt();

        qDebug() << "Update from" << majorVersion << "." << minorVersion;

        const auto dbBak = DatabaseUtils::backupDatabase(characterRepo.getDatabase());

        try
        {
            if (majorVersion < 2)
            {
                if (majorVersion == 0)
                {
                    if (minorVersion < 5)
                    {
                        if (minorVersion < 3)
                            migrateTo03();

                        migrateTo05(cacheTimerRepo, characterRepo, characterOrderRepo, corporationOrderRepo);
                    }
                }

                if (minorVersion < 16)
                {
                    if (minorVersion < 13)
                    {
                        if (minorVersion < 11)
                        {
                            if (minorVersion < 9)
                            {
                                if (minorVersion < 8)
                                    migrateTo18(externalOrderRepo);

                                migrateTo19(characterRepo,
                                            walletJournalRepo,
                                            corpWalletJournalRepo,
                                            walletTransactionRepo,
                                            corpWalletTransactionRepo);
                            }

                            migrateTo111(cacheTimerRepo, updateTimerRepo, characterRepo);
                        }

                        migrateTo113();
                    }

                    migrateTo116(orderValueSnapshotRepo, corpOrderValueSnapshotRepo);
                }
            }
        }
        catch (...)
        {
            QMessageBox::critical(nullptr, tr("Update"), tr(
                "An error occurred during the update process.\n"
                "Database backup was saved as %1. Please read online help how to deal with this situation.").arg(dbBak));

            throw;
        }
    }

    Updater &Updater::getInstance()
    {
        static Updater updater;
        return updater;
    }

    void Updater::checkForUpdates(bool quiet) const
    {
        if (mCheckingForUpdates)
            return;

        qDebug() << "Checking for updates...";

        mCheckingForUpdates = true;

        auto reply = mAccessManager.get(QNetworkRequest{QUrl{"http://evernus.com/latest_version.xml"}});
        connect(reply, &QNetworkReply::finished, this, [quiet, this] {
            finishCheck(quiet);
        });
    }

    void Updater::finishCheck(bool quiet) const
    {
        mCheckingForUpdates = false;

        auto reply = static_cast<QNetworkReply *>(sender());
        reply->deleteLater();

        const auto error = reply->error();
        if (error != QNetworkReply::NoError)
        {
            if (!quiet)
                QMessageBox::warning(nullptr, tr("Error"), tr("Error contacting update server: %1").arg(error));

            return;
        }

        QString docError;

        QDomDocument doc;
        if (!doc.setContent(reply, false, &docError))
        {
            if (!quiet)
                QMessageBox::warning(nullptr, tr("Error"), tr("Error parsing response from the update server: %1").arg(docError));

            return;
        }

        const auto nextVersionString = doc.documentElement().attribute("id");
        const auto nextVersion = nextVersionString.split('.');
        if (nextVersion.count() != 2)
        {
            if (!quiet)
                QMessageBox::warning(nullptr, tr("Error"), tr("Missing update version information!"));

            return;
        }

        const auto curVersion = QCoreApplication::applicationVersion().split('.');

        const auto nextMajor = nextVersion[0].toUInt();
        const auto curMajor = curVersion[0].toUInt();

        if ((nextMajor < curMajor) || (nextMajor == curMajor && nextVersion[1].toUInt() <= curVersion[1].toUInt()))
        {
            if (!quiet)
                QMessageBox::information(nullptr, tr("No update found"), tr("Your current version is up-to-date."));

            return;
        }

        const auto ret = QMessageBox::question(
            nullptr, tr("Update found"), tr("A new version is available: %1\nDo you wish to download it now?").arg(nextVersionString));
        if (ret == QMessageBox::Yes)
            QDesktopServices::openUrl(QUrl{"http://evernus.com/download"});
    }

    void Updater::migrateTo03() const
    {
        QSettings settings;
        settings.setValue(PriceSettings::autoAddCustomItemCostKey, PriceSettings::autoAddCustomItemCostDefault);
    }

    void Updater::migrateTo05(const CacheTimerRepository &cacheTimerRepo,
                              const Repository<Character> &characterRepo,
                              const MarketOrderRepository &characterOrderRepo,
                              const MarketOrderRepository &corporationOrderRepo) const
    {
        cacheTimerRepo.exec(QString{"DROP TABLE %1"}.arg(cacheTimerRepo.getTableName()));
        cacheTimerRepo.create(characterRepo);

        characterRepo.exec(QString{"ALTER TABLE %1 ADD COLUMN corporation_id INTEGER NOT NULL DEFAULT 0"}.arg(characterRepo.getTableName()));
        characterOrderRepo.exec(QString{"ALTER TABLE %1 ADD COLUMN corporation_id INTEGER NOT NULL DEFAULT 0"}.arg(characterOrderRepo.getTableName()));
        corporationOrderRepo.exec(QString{"ALTER TABLE %1 RENAME TO %1_temp"}.arg(corporationOrderRepo.getTableName()));

        corporationOrderRepo.dropIndexes(characterRepo);
        corporationOrderRepo.create(characterRepo);
        corporationOrderRepo.copyDataWithoutCorporationIdFrom(QString{"%1_temp"}.arg(corporationOrderRepo.getTableName()));
        corporationOrderRepo.exec(QString{"DROP TABLE %1_temp"}.arg(corporationOrderRepo.getTableName()));

        QMessageBox::information(nullptr, tr("Update"), tr(
            "This update requires re-importing all data.\n"
            "Please click on \"Import all\" after the update."));
    }

    void Updater::migrateTo18(const ExternalOrderRepository &externalOrderRepo) const
    {
        QMessageBox::information(nullptr, tr("Update"), tr("This update requires re-importing all item prices."));

        externalOrderRepo.exec(QString{"DROP TABLE %1"}.arg(externalOrderRepo.getTableName()));
        externalOrderRepo.create();
    }

    void Updater::migrateTo19(const Repository<Character> &characterRepo,
                              const WalletJournalEntryRepository &walletJournalRepo,
                              const WalletJournalEntryRepository &corpWalletJournalRepo,
                              const WalletTransactionRepository &walletTransactionRepo,
                              const WalletTransactionRepository &corpWalletTransactionRepo) const
    {
        QMessageBox::information(nullptr, tr("Update"), tr("This update requires re-importing all corporation transactions and journal."));

        corpWalletJournalRepo.exec(QString{"DROP TABLE %1"}.arg(corpWalletJournalRepo.getTableName()));
        corpWalletJournalRepo.create(characterRepo);
        corpWalletTransactionRepo.exec(QString{"DROP TABLE %1"}.arg(corpWalletTransactionRepo.getTableName()));
        corpWalletTransactionRepo.create(characterRepo);

        walletJournalRepo.exec(QString{"ALTER TABLE %1 ADD COLUMN corporation_id INTEGER NOT NULL DEFAULT 0"}.arg(walletJournalRepo.getTableName()));
        walletTransactionRepo.exec(QString{"ALTER TABLE %1 ADD COLUMN corporation_id INTEGER NOT NULL DEFAULT 0"}.arg(walletTransactionRepo.getTableName()));
    }

    void Updater::migrateTo111(const CacheTimerRepository &cacheTimerRepo,
                               const UpdateTimerRepository &updateTimerRepo,
                               const Repository<Character> &characterRepo) const
    {
        cacheTimerRepo.exec(QString{"DROP TABLE %1"}.arg(cacheTimerRepo.getTableName()));
        cacheTimerRepo.create(characterRepo);
        updateTimerRepo.exec(QString{"DROP TABLE %1"}.arg(updateTimerRepo.getTableName()));
        updateTimerRepo.create(characterRepo);
    }

    void Updater::migrateTo113() const
    {
        QSettings settings;
        settings.setValue(ImportSettings::ignoreCachedImportKey, false);
        settings.setValue(PriceSettings::combineCorpAndCharPlotsKey,
                          settings.value("rpices/combineCorpAndCharPlots", PriceSettings::combineCorpAndCharPlotsDefault));
    }

    void Updater::migrateTo116(const MarketOrderValueSnapshotRepository &orderValueSnapshotRepo,
                               const CorpMarketOrderValueSnapshotRepository &corpOrderValueSnapshotRepo) const
    {
        const auto updateShots = [](const auto &repo) {
            auto query = repo.prepare(QString{
                "UPDATE %1 SET buy_value = buy_value / 2, sell_value = sell_value / 2 WHERE timestamp >= ?"}.arg(repo.getTableName()));
            query.bindValue(0, QDateTime{QDate{2014, 9, 9}, QTime{0, 0}, Qt::UTC});

            Evernus::DatabaseUtils::execQuery(query);
        };

        updateShots(orderValueSnapshotRepo);
        updateShots(corpOrderValueSnapshotRepo);
    }
}
