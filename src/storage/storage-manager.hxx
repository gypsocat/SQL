#ifndef __MYG_SQL_STORAGE_MANAGER_H__
#define __MYG_SQL_STORAGE_MANAGER_H__

#include "storage-database.hxx"
#include <string_view>

namespace mygsql {
using MTB::owned;
using MTB::Object;

/** @class StorageManager
 * @brief 存储管理器, 用于管理`StorageDataBase`对象 */
class StorageManager: public MTB::Object {
public:
    using DataBasePtrT = owned<StorageDataBase>;
    using DataBaseMapT = std::unordered_map<std::string_view, DataBasePtrT>;
public:
    StorageManager(std::string_view work_directory);

    /** @fn get(name)
     * @brief 索引函数, 你可以用它实现`use database`语句 */
    StorageDataBase* get(std::string_view name) const {
        if (!_database_map.contains(name))
            return nullptr;
        StorageDataBase *ret = _database_map.at(name).get();
        return ret;
    }
    /** @fn createDataBase(name)
     * @brief `create database`语句的实现 */
    StorageDataBase *createDataBase(std::string_view name);
    /** @fn dropDataBase(name)
     * @brief `drop database`语句的实现 */
    bool dropDataBase(std::string_view name);

    DataBaseMapT const &get_database_map() const {
        return _database_map;
    }
private:
    DataBaseMapT      _database_map;
    std::filesystem::path _work_dir;
}; // class StorageManager

extern unowned<StorageManager> global_storage_manager;

void InitStorageManager(std::string_view argv0);

} // namespace mygsql

#endif