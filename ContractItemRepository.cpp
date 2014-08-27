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

#include "ContractItemRepository.h"

namespace Evernus
{
    ContractItemRepository::ContractItemRepository(bool corp, const QSqlDatabase &db)
        : Repository{db}
        , mCorp{corp}
    {
    }

    QString ContractItemRepository::getTableName() const
    {
        return (mCorp) ? ("corp_contract_items") : ("contract_items");
    }

    QString ContractItemRepository::getIdColumn() const
    {
        return "id";
    }

    ContractItemRepository::EntityPtr ContractItemRepository::populate(const QSqlRecord &record) const
    {
        auto contractItem = std::make_shared<ContractItem>(record.value("id").value<ContractItem::IdType>());
        contractItem->setContractId(record.value("contract_id").value<Contract::IdType>());
        contractItem->setTypeId(record.value("type_id").value<EveType::IdType>());
        contractItem->setQuantity(record.value("quantity").toULongLong());
        contractItem->setIncluded(record.value("included").toBool());
        contractItem->setNew(false);

        return contractItem;
    }

    void ContractItemRepository::create(const Repository<Contract> &contractRepo) const
    {
        exec(QString{R"(CREATE TABLE IF NOT EXISTS %1 (
            id BIGINT PRIMARY KEY,
            contract_id BIGINT NOT NULL REFERENCES %2(%3) ON UPDATE CASCADE ON DELETE CASCADE,
            type_id INTEGER NOT NULL,
            quantity BIGINT NOT NULL,
            included TINYINT NOT NULL
        ))"}.arg(getTableName()).arg(contractRepo.getTableName()).arg(contractRepo.getIdColumn()));
    }

    QStringList ContractItemRepository::getColumns() const
    {
        return QStringList{}
            << "id"
            << "contract_id"
            << "type_id"
            << "quantity"
            << "included";
    }

    void ContractItemRepository::bindValues(const ContractItem &entity, QSqlQuery &query) const
    {
        if (entity.getId() != ContractItem::invalidId)
            query.bindValue(":id", entity.getId());

        query.bindValue(":contract_id", entity.getContractId());
        query.bindValue(":type_id", entity.getTypeId());
        query.bindValue(":quantity", entity.getQuantity());
        query.bindValue(":included", entity.isIncluded());
    }

    void ContractItemRepository::bindPositionalValues(const ContractItem &entity, QSqlQuery &query) const
    {
        if (entity.getId() != ContractItem::invalidId)
            query.addBindValue(entity.getId());

        query.addBindValue(entity.getContractId());
        query.addBindValue(entity.getTypeId());
        query.addBindValue(entity.getQuantity());
        query.addBindValue(entity.isIncluded());
    }
}
