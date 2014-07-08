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
#include <QSqlRecord>
#include <QSqlQuery>

#include "WalletSnapshotRepository.h"

namespace Evernus
{
    QString WalletSnapshotRepository::getTableName() const
    {
        return "wallet_snapshots";
    }

    QString WalletSnapshotRepository::getIdColumn() const
    {
        return "timestamp";
    }

    WalletSnapshot WalletSnapshotRepository::populate(const QSqlRecord &record) const
    {
        WalletSnapshot walletSnapshot{record.value("timestamp").value<WalletSnapshot::IdType>(), record.value("balance").toDouble()};
        walletSnapshot.setCharacterId(record.value("character_id").value<Character::IdType>());
        walletSnapshot.setNew(false);

        return walletSnapshot;
    }

    void WalletSnapshotRepository::create(const Repository<Character> &characterRepo) const
    {
        exec(QString{R"(CREATE TABLE IF NOT EXISTS %1 (
            timestamp DATETIME PRIMARY KEY,
            character_id BIGINT NOT NULL REFERENCES %2(%3) ON UPDATE CASCADE ON DELETE CASCADE,
            balance DOUBLE NOT NULL
        ))"}.arg(getTableName()).arg(characterRepo.getTableName()).arg(characterRepo.getIdColumn()));

        exec(QString{"CREATE INDEX IF NOT EXISTS %1_%2_index ON %1(character_id)"}.arg(getTableName()).arg(characterRepo.getTableName()));
    }

    WalletSnapshotRepository::SnapshotList WalletSnapshotRepository
    ::fetchRange(Character::IdType characterId, const QDateTime &from, const QDateTime &to) const
    {
        auto query = prepare(QString{"SELECT * FROM %1 WHERE character_id = ? AND timestamp BETWEEN ? AND ? ORDER BY timestamp ASC"}
            .arg(getTableName()));

        query.addBindValue(characterId);
        query.addBindValue(from);
        query.addBindValue(to);

        DatabaseUtils::execQuery(query);

        SnapshotList result;

        const auto size = query.size();
        if (size > 0)
            result.reserve(size);

        while (query.next())
            result.emplace_back(populate(query.record()));

        return result;
    }

    QStringList WalletSnapshotRepository::getColumns() const
    {
        return QStringList{}
            << "timestamp"
            << "character_id"
            << "balance";
    }

    void WalletSnapshotRepository::bindValues(const WalletSnapshot &entity, QSqlQuery &query) const
    {
        if (entity.getId() != WalletSnapshot::invalidId)
            query.bindValue(":timestamp", entity.getId());

        query.bindValue(":character_id", entity.getCharacterId());
        query.bindValue(":balance", entity.getBalance());
    }

    void WalletSnapshotRepository::bindPositionalValues(const WalletSnapshot &entity, QSqlQuery &query) const
    {
        if (entity.getId() != WalletSnapshot::invalidId)
            query.addBindValue(entity.getId());

        query.addBindValue(entity.getCharacterId());
        query.addBindValue(entity.getBalance());
    }
}
