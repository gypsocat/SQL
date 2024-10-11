#ifndef __MYG_SQL_ENGINE_TABLE_H__
#define __MYG_SQL_ENGINE_TABLE_H__

#include "base/mtb-exception.hxx"
#include "base/sql-value.hxx"
#include "storage/storage-table.hxx"
#include "base/mtb-object.hxx"
#include <cstddef>
#include <cstdint>
#include <deque>
#include <format>
#include <functional>
#include <list>
#include <map>
#include <string_view>
#include <vector>

namespace mygsql::engine {
using MTB::owned;
using MTB::Object;
class Table;
class TableEntry;

/** @brief 执行引擎的查询表条目 */
class TableEntry: public MTB::Object {
public:
    friend class Table;
    using ValuePtrT  = StorageTable::Entry::ValuePtrT;
    using ValueListT = std::vector<ValuePtrT>;

    /** @class ColumnUnmatchedException
     * @brief 字符串column或者下标column_index所表示的列不存在 */
    class ColumnUnmatchedException: public MTB::Exception {
    public:
        ColumnUnmatchedException(std::string_view column)
            : column(column), column_index(-1),
              MTB::Exception(MTB::ErrorLevel::CRITICAL,
                std::format("UnmatchedException: column {} doesn't exist",
                                column)){}
        ColumnUnmatchedException(size_t column)
            : column_index(column), column(std::format("{}", column)),
              MTB::Exception(MTB::ErrorLevel::CRITICAL,
                std::format("UnmatchedException: column index {} doesn't exist",
                                column)){}
        
        std::string column;
        size_t column_index;
    }; // class ColumnUnmatchedException
public:
    /** @brief 从值列表中新建一个查询表条目 */
    TableEntry(Table &table, ValueListT const &value_list);
    /** @brief 从存储表的条目中加载一个查询表条目 */
    TableEntry(Table &table, StorageTable::Entry const &storage_entry);

    /** 通过列名称获取该条目中某一列对应的值
     * @warning 注意，你不能直接调用entry对应的函数。你必须获取的是_value_list对象里的值 */
    Value* get(std::string_view key);
    /** 通过列名称设置该条目中某一列对应的值
     * @warning 注意，你不能直接调用Entry里对应的set函数。你必须设置_value_list里对应条目的值 */
    bool   set(std::string_view key, Value *value);
    bool   set(std::string_view key, std::string_view value);
    bool   set(std::string_view key, int32_t value);
    ValueListT const &get_value_list() const {
        return _value_list;
    }
    bool has_error() const { return _has_error; }

    /** 把_value_list的内容同步到_internal_storage_entry里。你需要调用
     *  _internal_storage_entry的set方法来把_value_list的值放进去 */
    void sync();

    /** 移除条目。移除后条目不可用。 */
    void removeAndMakeUnavailable();
private:
    bool                _has_error;
    Table              &_table;
    ValueListT          _value_list;
    StorageTable::Entry _internal_storage_entry;
}; // class TableEntry

/** @class Table
 * @brief 执行引擎的表，存放条目 */
class Table: public MTB::Object {
public:
    friend class TableEntry;
    /* 来自存储引擎的定义 */
    using StorageTableT = StorageTable*;
    using TypeItemListT = StorageTable::TypeItemListT; // 类型列表，存储名称、类型等等
    using TypeItemMapT  = StorageTable::TypeItemMapT;  // 用于快速查找的类型映射表
    /* 自己的类型定义 */
    using EntryPtrT  = owned<TableEntry>; // 查询表条目智能指针类型. 使用指针是防止可能的内存移动导致其他引用失效
    using EntryMapT  = std::map<Value*, EntryPtrT>; // 查询表的索引表类型，根据主键排序。如果没有主键，那这个索引表就不会被使用。
    using EntryListT = std::list<EntryPtrT>;        // 查询表的条目列表类型。
    // 检查值是否符合func的条件的函数类型。符合的话，就返回true.
    using ValueConditionCheckFunc = std::function<bool(Value*)>;
    /* 与外部交互的类型定义 */
    using EntrySelectListT = std::deque<EntryListT::iterator>;
public:
    /** 从已经加载的存储表初始化一个查询表。你需要分解步骤，并调用下面的私有表创建函数。 */
    Table(StorageTable &storage_table);
    ~Table() override {
        syncToStorageTable();
    }

    StorageTable const &get_storage_table() const {
        return *_storage_table;
    }
    StorageTable::TypeItemListT const &get_type_item_list() const {
        return _storage_table->get_type_item_list();
    }
    EntryMapT &entry_map() { return _entry_map; }
    EntryMapT const &get_entry_map() const {
        return _entry_map;
    }
    EntryListT const &get_entry_list() const {
        return _entry_list;
    }
    std::string_view get_name() const { return _name; }
    int32_t get_primary_key_index() const { return _primary_key_index; }
    bool has_primary_key_index() const { return (_primary_key_index != 0xFFFF'FFFF); }

    /** 根据值列表插入一个值。一个合法的值列表，每个值类型的次序就是type_item_list()的类型次序。
     *  你需要遍历TypeItemList, 检查列表在类型次序上是否合法；然后，如果有主键，要检查主键是否重复。
     *  最后，创建一个Entry对象，将其插入条目列表。如果有主键，就插入索引表。 */
    TableEntry *insert(TableEntry::ValueListT const &value_list);

    /** select语句的部分实现：选择所有值，返回一整个列表 */
    EntrySelectListT selectAll();
    /** select语句的部分实现：根据条件选择，得到一个列表 */
    EntrySelectListT selectByCondition(std::string_view   condition_column,
                                       TotalOrderRelation relation,
                                       Value             *condition_value);
    /** select语句的部分实现：选择所有值，返回值列表 */
    std::deque<Value*> selectAllValue(std::string_view column);
    /** select语句的部分实现：根据条件选择，得到值列表 */
    std::deque<Value*> selectValueByCondition(std::string_view   column,
                                              std::string_view   condition_column,
                                              TotalOrderRelation relation,
                                              Value             *condition_value);

    /** update语句，更新整张表。
     * @return 返回更新的条目数量 */
    size_t updateEntireTable(std::string_view column, Value *value);
    size_t updateTableByCondition(
            std::string_view column,     Value *value,
            /* condition */
            std::string_view condition_column,
            TotalOrderRelation relation, Value *condition_value);
    
    /** delete语句 */
    void clear() {
        for (auto &i: _entry_list) {
            _storage_table->deleteEntry(&i->_internal_storage_entry);
        }
        _entry_list.clear();
    }
    size_t deleteEntryByCondition(std::string_view   condition_column,
                                  TotalOrderRelation relation,
                                  Value              *condition_value);

    /** 把当前查询表的内容同步到存储表中。你需要遍历整个查询表的每个条目，检查
     *  条目的主键是否重复。然后调用条目的`sync()`成员函数。 */
    void syncToStorageTable();

    /** @brief 把{column, value_type, is_primary}三元组转换成一个类型描述对象。
     * @warning 要注意类型描述对象`StorageTypeItem`的`name`属性没有对字符串的所有权，
     *          你不能在使用它的时候销毁它指向的字符串对象。
     *          在StorageTable的构造过程中，StorageTable自己会维护一个Key缓冲区。 */
    static StorageTypeItem typeItemFromInput(
                                std::string_view column,
                                Value::Type      value_type,
                                bool             is_primary) {
        return StorageTypeItem{column, value_type, is_primary, 0};
    }
private:
    StorageTableT _storage_table; // 存储表
    EntryMapT     _entry_map;   // 索引表，根据主键排序。
    EntryListT    _entry_list;  // 条目列表
    std::string   _name;        // 表名称。初始化时可以从_storage_table读取。
    /** 表的状态 */
    int32_t   _primary_key_index;

    /* 表创建函数 */
    /** @brief 在创建表时使用，根据内置的StorageTable对象初始化自己。
     * 步骤包括：
     * - 检查有没有主键(Primary Key)
     * - 使用_storage_table.traverseReadEntries()+柯里化的方式遍历已经
     *   分配的存储条目(StorageTable::Entry)
     *   然后调用MTB::make_owned<TableEntry>(TableEntry构造函数的参数)
     *   创建一个查询条目(TableEntry)
     *   最后把这个查询条目插入条目列表 */
    void _initializeFromStorageTable();
    /** @brief 倘若有主键，就遍历条目列表，把条目插入索引表。 */
    void _loadEntryMap();
}; // class Table

} // namespace mygsql::engine

#endif
