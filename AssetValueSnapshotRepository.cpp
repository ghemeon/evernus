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

#include "AssetValueSnapshotRepository.h"

namespace Evernus
{
    QString AssetValueSnapshotRepository::getTableName() const
    {
        return "asset_value_snapshots";
    }

    QString AssetValueSnapshotRepository::getIdColumn() const
    {
        return "timestamp";
    }

    AssetValueSnapshot AssetValueSnapshotRepository::populate(const QSqlRecord &record) const
    {
        AssetValueSnapshot assetValueSnapshot{record.value("timestamp").value<AssetValueSnapshot::IdType>(), record.value("balance").toDouble()};
        assetValueSnapshot.setCharacterId(record.value("character_id").value<Character::IdType>());
        assetValueSnapshot.setNew(false);

        return assetValueSnapshot;
    }

    void AssetValueSnapshotRepository::create(const Repository<Character> &characterRepo) const
    {
        exec(QString{R"(CREATE TABLE IF NOT EXISTS %1 (
            timestamp DATETIME PRIMARY KEY,
            character_id BIGINT NOT NULL REFERENCES %2(%3) ON UPDATE CASCADE ON DELETE CASCADE,
            balance DOUBLE NOT NULL
        ))"}.arg(getTableName()).arg(characterRepo.getTableName()).arg(characterRepo.getIdColumn()));

        exec(QString{"CREATE INDEX IF NOT EXISTS %1_%2_index ON %1(character_id)"}.arg(getTableName()).arg(characterRepo.getTableName()));
    }

    QStringList AssetValueSnapshotRepository::getColumns() const
    {
        return QStringList{}
            << "timestamp"
            << "character_id"
            << "balance";
    }

    void AssetValueSnapshotRepository::bindValues(const AssetValueSnapshot &entity, QSqlQuery &query) const
    {
        query.bindValue(":timestamp", entity.getId());
        query.bindValue(":balance", entity.getBalance());
        query.bindValue(":character_id", entity.getCharacterId());
    }
}
