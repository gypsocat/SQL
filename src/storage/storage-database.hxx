#ifndef __MYG_SQL_DATABASE_H__
#define __MYG_SQL_DATABASE_H__

#include "base/mtb-object.hxx"
#include "storage-table.hxx"
#include <string_view>

namespace mygsql {
using MTB::owned;
using MTB::unowned;
using MTB::Object;

/** @class StorageDataBase
 *  @brief "数据库"存储单元抽象出的类 */
class StorageDataBase: public MTB::Object {
public:
    using TablePtrT = owned<StorageTable>;
    using TableMapT = std::unordered_map<std::string_view, TablePtrT>;
    using TypeItemListT = StorageTable::TypeItemListT;
public:
    StorageDataBase(std::string_view work_directory, std::string_view name);

    /** @fn createTable(name, StorageTypeItem{is_allocated, type, col_name}[])
     * @brief `create table`的实现. 根据类型信息列表创建一张表
     * @return 创建的表的指针.失败则返回nullptr. */
    unowned<StorageTable> createTable(std::string_view name, TypeItemListT type_items);
    /** @fn get(name)
     * @brief `select ... from {table}`的部分实现, 实现选择一张表 */
    unowned<StorageTable> get(std::string_view const name) const {
        if (!_table_map.contains(name))
            return nullptr;
        return _table_map.at(name).get();
    }
    /** @fn dropTable(name)
     * @brief `drop table`语句的实现.根据名称删除表. */
    bool dropTable(std::string_view const name);

    void eraseAndMakeUnavailable();

    /** @fn get_name()
     * @brief getter: 名称 */
    std::string_view get_name() const {
        return _name;
    }
    /** @fn get_table_map()
     * @brief getter: 存储单元的哈希表, 你可以使用这个getter遍历整张DataBase. */
    TableMapT const &get_table_map() const {
        return _table_map;
    }
    bool has_error() const { return _has_error; }
protected:
    TableMapT       _table_map;
    std::string     _name;
    std::filesystem::path _work_dir;
    bool            _has_error;

    void _loadTables();
}; // class DataBase


} // namespace mygsql

#endif
