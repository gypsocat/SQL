#include "engine-database.hxx"
#include "base/mtb-object.hxx"
#include "engine/engine-table.hxx"
#include "storage/storage-database.hxx"
#include "storage/storage-table.hxx"
#include <string_view>

namespace mygsql::engine {
using MTB::owned;

DataBase::DataBase(StorageDataBase &storage_database)
    : _storage_database(storage_database) {
    for (auto &i: storage_database.get_table_map()) {
        Table *table = new Table(*i.second.get());
        _table_map.insert({i.first, table});
    }
}

Table *DataBase::createTable(std::string_view name,
                             StorageTable::TypeItemListT const &type_item_list)
{
    StorageTable *storage_table = _storage_database.createTable(name, type_item_list);
    if (storage_table == nullptr)
        return nullptr;
    owned<Table> table = new Table(*storage_table);
    Table *ret = table.get();
    _table_map.insert({table->get_name(), std::move(table)});
    return ret;
}

Table *DataBase::useTable(std::string_view name)
{
    if (!_table_map.contains(name))
        return nullptr;
    return _table_map.at(name).get();
}

bool DataBase::dropTable(std::string_view name)
{
    if (!_table_map.contains(name))
        return false;
    _table_map.erase(name);
    return _storage_database.dropTable(name);
}

} // namespace mygsql