#ifndef __MYG_SQL_ENGINE_DATABASE_MANAGER_H__
#define __MYG_SQL_ENGINE_DATABASE_MANAGER_H__

#include "base/mtb-object.hxx"
#include "engine-database.hxx"
#include "storage/storage-manager.hxx"
#include <string_view>
#include <unordered_map>

namespace mygsql::engine {
using MTB::owned;

/** @class DataBaseManager
 * @brief 数据库管理器, 用于存放查找的数据库 */
class DataBaseManager: public MTB::Object {
public:
    using DataBasePtrT = owned<DataBase>;
    using DataBaseMapT = std::unordered_map<std::string_view, DataBasePtrT>;
public:
    DataBaseManager(std::string_view storage_path);
    DataBase *createDataBase(std::string_view name);
    DataBase *getDataBase(std::string_view name) const;
    bool dropDataBase(std::string_view name);

    DataBaseMapT const &get_database_map() const {
        return _database_map;
    }
    StorageManager const &get_storage_manager() const {
        return _storage_manager;
    }
    StorageManager &storage_manager() {
        return _storage_manager;
    }
private:
    StorageManager  _storage_manager;
    DataBaseMapT    _database_map;
}; // class DataBaseManager


} // namespace mygsql

#endif