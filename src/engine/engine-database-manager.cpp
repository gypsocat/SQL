#include "engine-database-manager.hxx"
#include "base/mtb-object.hxx"
#include "engine/engine-database.hxx"
#include "storage/storage-database.hxx"
#include <string_view>

namespace mygsql::engine {

DataBaseManager::DataBaseManager(std::string_view path)
    : _storage_manager(path) {
    for (auto &i: _storage_manager.get_database_map()) {
        owned<DataBase> db = new DataBase(*(i.second.get()));
        _database_map.insert({i.first, std::move(db)});
    }
}

DataBase *DataBaseManager::createDataBase(std::string_view name)
{
    StorageDataBase *sdb = _storage_manager.createDataBase(name);
    if (sdb == nullptr)
        return nullptr;
    owned<DataBase> db = new DataBase(*sdb);
    DataBase *unowned_db = db.get();
    _database_map.insert({db->get_name(), std::move(db)});
    return unowned_db;
}

DataBase *DataBaseManager::getDataBase(std::string_view name) const
{
    if (!_database_map.contains(name))
        return nullptr;
    return _database_map.at(name).get();
}

bool DataBaseManager::dropDataBase(std::string_view name)
{
    if (!_database_map.contains(name))
        return false;
    _database_map.erase(name);
    return _storage_manager.dropDataBase(name);
}

} // namespace mygsql