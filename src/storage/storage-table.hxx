#ifndef __MYG_SQL_STORAGE_TABLE_H__
#define __MYG_SQL_STORAGE_TABLE_H__

#include "base/mtb-object.hxx"
#include "base/mtb-system.hxx"
#include "base/sql-value.hxx"
#include "base/util/mtb-id-allocator.hxx"
#include <cstddef>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <memory>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace mygsql {
using MTB::owned;
using MTB::Object;

struct StorageTypeItem {
    std::string_view name; // 索引名称
    Value::Type      type; // 索引类型
    bool   is_primary = false; // 是否为主键
    /* 下面的字段在传入时不用填 */
    uint32_t   offset = 0; // 索引在实际内存中的偏移量, 作为输入参数时填0即可.
}; // struct StorageTypeItem

class StorageTable: public MTB::Object {
public:
    friend class Entry;
    // 文件映射器
    using FileMapperT   = std::unique_ptr<MTB::FileMapper>;
    // 类型索引列表
    using TypeItemMapT  = std::unordered_map<std::string_view, StorageTypeItem*>;
    using TypeItemListT = std::deque<StorageTypeItem>;
    using EntryAllocator= std::unique_ptr<MTB::IDAllocator>;
    // 类型遍历函数
    class Entry;
    using EntryTraverseRWFunc   = std::function<void(Entry &)>;     // 读遍历
    using EntryTraverseReadFunc = std::function<void(Entry const&)>;// 读写遍历

    class Entry: public MTB::Object {
    public:
        using ValuePtrT = owned<Value>;
        friend class StorageTable;
    public:
        Entry(StorageTable *table_part): _table(*table_part){}
        Entry(StorageTable const &table, int index);
        Entry(Entry const &another);

        size_t length() const;    // 对应内存单元的长度
        ValuePtrT get(std::string_view name) const;          // 根据名称取值
        ValuePtrT getFromIndex(size_t index) const;          // 根据索引取值
        bool set(std::string_view name, Value const &value); // 根据名称设置值
        bool set(std::string_view name, int32_t value);
        bool set(std::string_view name, std::string_view value);
        bool isAllocated() const { return *((int32_t*)get()) != 0; }
        /** @fn get_header_index() const
         * @brief getter:header_index 当前条目相对于StorageTable的整数索引 */
        uint32_t get_header_index() const { return _header_index; }
        uint32_t get_header_offset() const {
            return _header_offset;
        }
    private:
        StorageTable const &_table; // 所属实例
        uint32_t    _header_offset; // 当前条目的偏移量. 不直接使用指针的原因是, 文件的首地址会变.
        uint32_t     _header_index; // 当前条目的整数索引

        MTB::pointer get() const; // 获取内存单元的首地址
    }; // class Entry

public:
    /** @fn StorageTable(string_view sd, string_view name)
     *  @brief 打开一个名称为name的StorageTable */
    StorageTable(std::string_view storage_directory, std::string_view name);

    /** @fn StorageTable(string_view sd, string_view name, StorageTypeItem [])
     *  @brief 创建一个名称为`name`的StorageTable, 类型列表为`type_items` */
    StorageTable(std::string_view cwd, std::string_view name,
                 TypeItemListT const& type_items);
    
    /** @brief getter:验证这个类是否有错误 */
    bool has_error() const { return _has_error; }

    /** @brief getter:类型列表 */
    TypeItemListT const &get_type_item_list() const {
        return _type_item_list;
    }
    /** @brief getter:条目长度 */
    size_t get_entry_size() const { return _entry_size; }

    /** @brief getter:名称 */
    std::string_view get_name() const { return _name; }

    /** @fn getType(string name)
     * @brief 根据字段`name`的名称查找`name`的类型信息
     * @return 字段信息指针, 没找到或出现错误则返回`nullptr`. */
    const StorageTypeItem *getType(std::string_view name) const;

    /** @fn getTypeIndex(string name)
     * @brief 根据字段name的名称查找name是第几个字段。由于查询引擎经常会把一个条目
     *        的值列表当成线性表传来传去，需要频繁定位某一个名称到底是第几个column，
     *        所以新增这一个函数。
     * @return 返回的索引，从0开始。倘若返回负值，就是找不到。 */
    int32_t getTypeIndex(std::string_view name) const;

    /** @fn getPrimaryIndex() const
     * @brief 查找该表的索引类
     * @return StorageTypeItem 索引类型结构体，包括{名称,类型,是否为主键(true),偏移量} */
    const StorageTypeItem *getPrimaryIndex() const;

    size_t get_primary_index_order() const {
        return _primary_index_order;
    }

    /** @fn getPrimaryKey() const
     * @brief  查找主索引
     * @return 主索引的名称 */
    const std::string_view getPrimaryKey() const {
        return getPrimaryIndex()->name;
    }

    /** Entry相关操作 */
    Entry allocateEntry();    ///< 创建一个空条目

    /** @fn appendEntry
     * @brief 申请一个条目, 然后把写入条目值 */
    Entry appendEntry(std::vector<owned<Value>> const &value_list);

    /** @fn appendEntry
     * @brief 申请一个条目, 然后把写入条目值 */
    Entry appendEntry(std::unordered_map<std::string_view, Value*> const &value_list);

    /** @fn deleteEntry
     * @brief 根据条目本身删除一个条目。这个函数比较安全。 */
    bool deleteEntry(Entry *entry);

    /** @fn deleteEntryByID
     * @brief 根据条目的ID删除一个条目。因为IDAllocator的删除效率是O(1),
     *        所以这个删除函数很快。但是你要小心删错东西。 */
    bool deleteEntryByID(int32_t id);

    /** @fn deleteEntryByPrimaryKey
     * @brief 删除所有首要索引为`key`的条目。 */
    bool deleteEntryByPrimaryKey(Value *value);
    
    /** @fn traverseReadEntries
     * @brief 遍历每一个条目,然后读取它 */
    void traverseReadEntries(EntryTraverseReadFunc fn) const;

    /** @fn traverseRWEntries
     * @brief 遍历每一个条目,然后调用读写函数 */
    void traverseRWEntries(EntryTraverseRWFunc fn);

    /** @fn eraseAndMakeUnavailable
     * @brief 清除这张表所有的文件，执行后这张表不可用。 */
    void eraseAndMakeUnavailable();
private:
    FileMapperT   _entry_mapper;   // 条目列表的文件映射器
    FileMapperT   _index_mapper;   // 索引列表的文件映射器
    TypeItemMapT  _type_item_map;  // 类型索引
    TypeItemListT _type_item_list; // 类型列表
    std::string   _name;           // 名称
    uint32_t      _entry_size;     // 条目大小
    uint32_t      _entry_list_num; // 已分配与未分配的所有条目个数
    uint32_t _entry_allocated_num; // 已分配的条目个数
    uint32_t _primary_index_order; // 主索引的次序
    std::filesystem::path _work_dir; // 工作目录
    EntryAllocator _entry_allocator; // 条目分配器
    // 类型描述对象的字符缓冲区。解决类型描述对象没有对名称的所有权的漏洞。
    std::string   _type_item_name_buffer;
    std::unordered_map<std::string_view, int32_t> _type_item_index_map;
    bool _has_error = false;     // 是否出错

    /** 加载函数 */
    void _loadIndexFile(std::string const &idx_path);
    void _loadEntryFile(std::string const &dat_path);
    void _createIndexFile(std::string const &idx_path);
    void _createEntryFile(std::string const &dat_path);
    void _dumpTypeItemNameBuffer(); // 保存类型对象列表的名称到私有缓冲区，防止UAF问题
    void _initKeyIndexMap();        // 加载column名称-column顺序的映射表

    /** 其他私有方法 */
    MTB::pointer _getEntryStartMemory() const noexcept;
    MTB::pointer _getEntryMemory(size_t index) const noexcept;
    size_t       _getEntryOffset(size_t index) const noexcept;
};// class StorageTable

} // namespace mygsql

#endif