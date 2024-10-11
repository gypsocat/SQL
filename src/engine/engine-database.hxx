#ifndef __MYGL_ENGINE_DATABASE_H__
#define __MYGL_ENGINE_DATABASE_H__

#include "base/mtb-object.hxx"
#include "engine-table.hxx"
#include "storage/storage-database.hxx"
#include "storage/storage-table.hxx"
#include <string_view>
#include <unordered_map>

namespace mygsql::engine {
/** 使用引用计数的智能指针 */
using MTB::owned;
/** 使用引用计数的基类 */
using MTB::Object;

/** @class DataBase
 * @brief 与查询表配套、用于查询表的数据库表管理器 */
class DataBase: public MTB::Object {
public:
    /** 来自后端的定义 */
    using StorageTablePtrT = StorageDataBase::TablePtrT;
    using StorageTableMapT = StorageDataBase::TableMapT;
    /** 表存储项定义 */
    using TablePtrT = owned<Table>;
    using TableMapT = std::unordered_map<std::string_view, TablePtrT>;
public:
    /** 构造函数：从已经存在的存储池管理器构造 */
    DataBase(StorageDataBase &storage_database);

    /** getter: 依赖的存储池管理器指针 */
    StorageDataBase const &get_storage_database() const {
        return _storage_database;
    }
    /** getter: 表映射器，用于遍历 */
    TableMapT const &get_table_map() const { return _table_map; }
    /** getter: 名称 */
    std::string_view get_name() const {
        return _storage_database.get_name();
    }
    /** 创建一张新表，并返回没有所有权的Table指针。
     *  这个函数在运行时会先调用存储引擎中的表创建函数。 */
    Table *createTable(std::string_view name,
                       StorageTable::TypeItemListT const &type_item_list);
    /** 查找一张表 */
    Table *useTable(std::string_view name);
    /** 删除一张表，返回是否删除成功。倘若表不存在，会返回false.
     *  这个函数最后会调用存储引擎中的表删除函数，所以不要额外管理存储表。 */
    bool dropTable(std::string_view name);
private:
    StorageDataBase &_storage_database;
    TableMapT        _table_map;
}; // class DataBase

} // namespace mygsql::engine

#endif