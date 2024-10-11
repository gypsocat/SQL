#include "storage-table.hxx"
#include "base/mtb-object.hxx"
#include "base/mtb-stl-accel.hxx"
#include "base/mtb-system.hxx"
#include "base/sql-value.hxx"
#include "base/util/mtb-id-allocator.hxx"
#include <cstdio>
#include <filesystem>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <endian.h>
#include <list>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mygsql {

using MTB::pointer;

static inline void mtb_ptr_advance(pointer &ptr, int offset)
{
    size_t &ptr_sz = *reinterpret_cast<size_t*>(&ptr);
    ptr_sz += offset;
}

constexpr size_t DataTypeGetSize(const Value::Type self)
{
    switch (self) {
    case Value::Type::INT:
        return 4;
    case Value::Type::STRING:
        return 260;
    default:
        return 0;
    }
}

constexpr uint32_t i32size   = DataTypeGetSize(Value::Type::INT);
constexpr uint32_t unit_size = i32size * 3;

struct IndexFile {
    struct IndexUnit {
        uint32_t    name_index;  // 名称字符串首地址所属的索引
        uint32_t    name_length; // 名称长度所属的索引
        Value::Type data_type;   // 数据类型

        /** 计算结果 */
        std::string_view name;   // 名称字符串
    }; // IndexUnit
public:
    /** 元数据 */
    uint32_t   index_size;
    uint32_t   primary_index;
    /** 索引区 */
    pointer    index_units_raw;

    /** 计算结果 */
    uint8_t   *string_area_raw;
    std::vector<IndexUnit> index_units;

    static IndexFile CreateFromTypeList(StorageTable::TypeItemListT const &item_list);
    static IndexFile LoadFromBuffer(pointer start)
    {
        IndexFile self;
        uint32_t *u32start = reinterpret_cast<uint32_t*>(start);
        
        self.index_size    = be32toh(u32start[0]);
        self.primary_index = be32toh(u32start[1]);
        mtb_ptr_advance(start, i32size * 2);
        self.index_units_raw = start;

        uint8_t *string_area = reinterpret_cast<uint8_t*>(start);
        string_area += self.index_size * unit_size;
        self.string_area_raw = string_area;

        /** read IndexUnit*/
        for (int i = 0; i < self.index_size; i++) {
            IndexUnit unit;
            u32start = reinterpret_cast<uint32_t*>(start);
            unit.name_index  = be32toh(u32start[0]);
            unit.name_length = be32toh(u32start[1]);
            unit.data_type   = Value::Type(be32toh(u32start[2]));
            unit.name = {(char*)(string_area + unit.name_index), unit.name_length};
            self.index_units.push_back(unit);

            mtb_ptr_advance(start, unit_size);
        }
        return self;
    }
    void saveToBuffer(pointer start)
    {
        int cur_index = 0;
        for (auto &i: index_units) {
            i.name_index  = cur_index;
            i.name_length = i.name.length();
            cur_index    += i.name_length;
        }

        uint32_t *u32start = reinterpret_cast<uint32_t*>(start);
        u32start[0] = htobe32(index_size);
        u32start[1] = htobe32(primary_index);

        uint32_t *u32unit     = u32start + 2;
        uint8_t  *string_area = reinterpret_cast<uint8_t*>(u32unit) + unit_size * index_size;
        for (auto &i: index_units) {
            u32unit[0] = htobe32(i.name_index);
            u32unit[1] = htobe32(i.name_length);
            u32unit[2] = htobe32(uint32_t(i.data_type));
            u32unit += 3;
            memcpy(string_area + i.name_index, i.name.data(), i.name_length);
        }
    }
    uint32_t get_storage_size() const {
        uint32_t size = i32size + i32size + index_units.size() * unit_size;
        for (auto &i: index_units)
            size += i.name.size();
        return size;
    }
}; // struct IndexFile

IndexFile IndexFile::CreateFromTypeList(StorageTable::TypeItemListT const &item_list)
{
    IndexFile ret{
        uint32_t(item_list.size()),
        0xFFFF'FFFF
    };
    for (int cnt = 0;
         auto &i: item_list) {
        ret.index_units.push_back({0, 0, i.type, i.name});
        if (i.is_primary == true && ret.primary_index == 0xFFFF'FFFF)
            ret.primary_index = cnt;
        cnt++;
    }
    return ret;
}

/* class StorageTable::Entry */
StorageTable::Entry::Entry(StorageTable const &table, int index)
    : _table(table), _header_index(index) {
    _header_offset = _header_index * table._entry_size;
}
StorageTable::Entry::Entry(Entry const &another)
    : _table(another._table),
      _header_index(another._header_index),
      _header_offset(another._header_offset){
}

size_t StorageTable::Entry::length() const {
    return _table.get_entry_size();
}
MTB::pointer StorageTable::Entry::get() const {
    pointer ret = _table._getEntryMemory(_header_index);
    mtb_ptr_advance(ret, i32size);
    return ret;
}
owned<Value> StorageTable::Entry::get(std::string_view name) const
{
    TypeItemMapT const& type_item_map = _table._type_item_map;
    if (!type_item_map.contains(name))
        return nullptr;

    auto &type_item = type_item_map.at(name);
    Value *ret    = nullptr;
    void  *target = _table._getEntryMemory(_header_index);
    mtb_ptr_advance(target, type_item->offset);

    switch (type_item->type) {
    case Value::Type::INT: {
        int32_t &i32raw = *reinterpret_cast<int32_t*>(target);
        ret = new IntValue(be32toh(i32raw));
    }   break;
    case Value::Type::STRING: {
        int32_t *pi32length_raw = reinterpret_cast<int32_t*>(target);
        int32_t i32length = int32_t(be32toh(*pi32length_raw));
        mtb_ptr_advance(target, i32size);
        std::string_view content {
            reinterpret_cast<char*>(target),
            size_t(i32length)
        };
        ret = new StringValue(content);
    }   break;
    default:
        return nullptr;
    } // switch (type_item->type)
    return owned<Value>(ret);
}
bool StorageTable::Entry::set(std::string_view name, int32_t value)
{
    TypeItemMapT const& type_item_map = _table._type_item_map;
    if (!type_item_map.contains(name))
        return false;
    StorageTypeItem *type_item = type_item_map.at(name);
    if (type_item->type != Value::Type::INT)
        return false;

    void *target = MTB::pointer((size_t)get() + type_item->offset - i32size);
    *reinterpret_cast<int32_t*>(target) = htobe32(value);
    return true;
}
bool StorageTable::Entry::set(std::string_view name, std::string_view value)
{
    if (value.length() > 256)
        return false;
    TypeItemMapT const& type_item_map = _table._type_item_map;
    if (!type_item_map.contains(name))
        return false;
    StorageTypeItem *type_item = type_item_map.at(name);
    if (type_item->type != Value::Type::STRING)
        return false;

    void *target = MTB::pointer((size_t)get() + type_item->offset - i32size);
    // 长度字段
    *reinterpret_cast<int32_t*>(target) = htobe32(value.length());
    // 字符串数据字段
    mtb_ptr_advance(target, i32size);
    memcpy(target, value.data(), value.length());
    return true;
}
bool StorageTable::Entry::set(std::string_view name, Value const &value)
{
    if (value.get_value_type() == Value::Type::INT)
        return set(name, ((IntValue&)value).value());
    else
        return set(name, reinterpret_cast<const StringValue*>(&value)->getString());
}
/* end class StorageTable::Entry */

/** class StorageTable */
StorageTable::StorageTable(std::string_view storage_directory, std::string_view name)
    : _name(name), _work_dir(storage_directory) {
    std::string idx_name(name), dat_name(name);
    idx_name.append(".idx");
    dat_name.append(".dat");

    if (!std::filesystem::exists(_work_dir)) {
        _has_error = true;
        return;
    } // 检查目标文件夹是否存在
    
    std::filesystem::path idx_path = _work_dir / idx_name;
    std::filesystem::path dat_path = _work_dir / dat_name;
    if (!std::filesystem::exists(idx_path) ||
        !std::filesystem::exists(dat_path)) {
        _has_error = true;
        return;
    }
    idx_name = idx_path.string();
    dat_name = dat_path.string();
    _loadIndexFile(idx_name);
    _loadEntryFile(dat_name);
    _dumpTypeItemNameBuffer();
    _initKeyIndexMap();
}
StorageTable::StorageTable(std::string_view storage_directory, std::string_view name,
                           TypeItemListT const& type_items)
    : _name(name),
      _work_dir(storage_directory),
      _type_item_list(type_items),
      _primary_index_order(0xFFFF'FFFF),
      _entry_allocator(std::make_unique<MTB::IDAllocator>()),
      _entry_allocated_num(0),
      _entry_list_num(0),
      _entry_size(0) {
    std::string idx_name(name), dat_name(name);
    idx_name.append(".idx");
    dat_name.append(".dat");

    if (!std::filesystem::exists(_work_dir)) {
        std::filesystem::create_directory(_work_dir);
    } // 检查目标文件夹是否存在

    std::filesystem::path idx_path = _work_dir / idx_name;
    std::filesystem::path dat_path = _work_dir / dat_name;
    if (std::filesystem::exists(idx_path) ||
        std::filesystem::exists(dat_path)) {
        _has_error = true;
        return;
    }
    idx_name = idx_path.string();
    dat_name = dat_path.string();
    int offset = 0;
    for (int cnt = 0;
         auto &item: _type_item_list) {
        item.offset = offset + i32size;
        _type_item_map.insert({item.name, &item});
        if (_primary_index_order == 0xFFFF'FFFF && item.is_primary == true)
            _primary_index_order = cnt;
        offset += DataTypeGetSize(item.type);
    }
    _entry_size = offset;
    _dumpTypeItemNameBuffer();
    _initKeyIndexMap();
    _createIndexFile(idx_path);
    _createEntryFile(dat_path);
}

/** private class StorageTable */
void StorageTable::_loadIndexFile(std::string const &path)
{
    /* 把索引文件映射到内存处理 */
    _index_mapper = std::unique_ptr<MTB::FileMapper>(MTB::CreateFileMapper(path));
    /* 读取映射以后的内存 */
    IndexFile index_file = IndexFile::LoadFromBuffer(_index_mapper->get());
    /* 遍历类型列表，构建`type_item_list`类型 - 偏移量列表 */
    uint32_t current_offset = DataTypeGetSize(Value::Type::INT); // 4 byte -- is_allocated
    for (auto &i: index_file.index_units) {
        if (_type_item_map.contains(i.name)) {
            _has_error = true; return;
        } // 检查是否有键名重复问题
        _type_item_list.push_back({
            i.name, i.data_type,
            false,
            current_offset 
        });
        auto &back = _type_item_list.back();
        _type_item_map.insert({back.name, &back});
        current_offset += DataTypeGetSize(i.data_type);
    }
    if (index_file.primary_index != 0xFFFF'FFFF)
        _type_item_list[index_file.primary_index].is_primary = true;
    _primary_index_order = index_file.primary_index;
    /* 条目大小信息, 读取条目文件用 */
    _entry_size = current_offset;
}

void StorageTable::_loadEntryFile(std::string const &path)
{
    constexpr size_t file_header_size = i32size;
    _entry_mapper = std::unique_ptr<MTB::FileMapper>{
                        MTB::CreateFileMapper(path)
                    };
    pointer start = _entry_mapper->get();
    _entry_list_num = be32toh(*(uint32_t*)start); // 文件头: entry个数,4字节
    /* 检查条目是否溢出 */
    size_t file_least_size = _entry_list_num * _entry_size + file_header_size;
    if (file_least_size > _entry_mapper->get_file_size()) {
        _has_error = true;
        return;
    }
    /* 加载条目 */
    bool8vec vec;
    for (int i = 0; i < _entry_list_num; i++) {
        pointer entry_begin_addr     = _getEntryMemory(i);
        // for (int j = 0; j < 8; j++) {
        //     printf("%02hhx ", ((char*)entry_begin_addr)[j]);
        // }printf("\n");
        uint32_t &entry_is_allocated = *reinterpret_cast<uint32_t*>(entry_begin_addr);
        /* 往后压入分配情况 */
        vec.push_back(MTB::bool8_t(entry_is_allocated));
        /* 维护已分配的ID个数列表 */
        _entry_allocated_num += (entry_is_allocated != 0) ? 1 : 0;
    }
    _entry_allocator = std::make_unique<MTB::IDAllocator>(vec);
}
void StorageTable::_createIndexFile(std::string const &path)
{
    IndexFile index_file = IndexFile::CreateFromTypeList(_type_item_list);
    _index_mapper = std::unique_ptr<MTB::FileMapper>(MTB::CreateFileMapper(path));
    size_t mapper_size = index_file.get_storage_size();
    while (_index_mapper->get_file_size() <= mapper_size)
        _index_mapper->resizeAppend();
    index_file.saveToBuffer(_index_mapper->get());
}
void StorageTable::_createEntryFile(std::string const &path)
{
    _entry_mapper = std::unique_ptr<MTB::FileMapper> {
                        MTB::CreateFileMapper(path)
                    };
    uint32_t &index_size = *reinterpret_cast<uint32_t*>(_entry_mapper->get());
    index_size = 0;
}

pointer StorageTable::_getEntryStartMemory() const noexcept {
    pointer ret = _entry_mapper->get();
    mtb_ptr_advance(ret, i32size);
    return ret;
}
/** @fn StorageTable::_getEntryMemory(index)
 * @brief 根据条目的下标获取条目存储区的内存起始地址。
 * @return pointer 条目的内存起始地址，指向`is_allocated`字段。 */
pointer StorageTable::_getEntryMemory(size_t index) const noexcept {
    pointer ret = _getEntryStartMemory();
    mtb_ptr_advance(ret, _entry_size * index);
    return ret;
}
size_t StorageTable::_getEntryOffset(size_t index) const noexcept {
    return i32size + index * _entry_size;
}

void StorageTable::_dumpTypeItemNameBuffer()
{
    using StringOffsetPair = std::pair<size_t, size_t>;
    std::vector<StringOffsetPair> offset_pairs;
    for (size_t offset = 0;
         StorageTypeItem &i : _type_item_list) {
        _type_item_name_buffer.append(i.name);
        size_t length = i.name.length();
        offset_pairs.push_back({offset, length});
        offset += length;
    }
    const char *buffer_begin = _type_item_name_buffer.data();
    const char *current_buffer = buffer_begin;
    for (size_t index = 0; auto &i: offset_pairs) {
        current_buffer = buffer_begin + i.first;
        _type_item_list[index].name = {current_buffer, i.second};
        index++;
    }
}

void StorageTable::_initKeyIndexMap()
{
    for (int index = 0; StorageTypeItem &i : _type_item_list) {
        _type_item_index_map.insert({i.name, index});
        index++;
    }
}

/* public class StorageTable */
void StorageTable::traverseReadEntries(StorageTable::EntryTraverseReadFunc fn) const
{
    for (int index: *_entry_allocator) {
        Entry entry(*this, index);
        fn(entry);
    }
}
void StorageTable::traverseRWEntries(StorageTable::EntryTraverseRWFunc fn)
{
    for (int index: *_entry_allocator) {
        Entry entry(*this, index);
        fn(entry);
    }
}

StorageTable::Entry StorageTable::allocateEntry()
{
    int id = _entry_allocator->allocate();
    if (id >= _entry_list_num)
        _entry_list_num++;
    _entry_allocated_num++;
    while (_getEntryOffset(id) >= _entry_mapper->get_file_size())
        _entry_mapper->resizeAppend();
    /* 同步分配情况到文件映射的内存区域 */
    uint32_t &allocate_status = *reinterpret_cast<uint32_t*>(_getEntryMemory(id));
    allocate_status = htobe32(true);
    /* 同步总条目个数到文件映射区域 */
    uint32_t &entry_list_length = *reinterpret_cast<uint32_t*>(_entry_mapper->get());
    entry_list_length = htobe32(_entry_list_num);
    return Entry(*this, id);
}
StorageTable::Entry StorageTable::appendEntry(std::vector<owned<Value>> const &value_list)
{
    Entry entry = allocateEntry();
    for (int index = 0; owned<Value> const &i: value_list) {
        entry.set(_type_item_list[index].name, *i.get());
        index++;
    }
    return entry;
}

bool StorageTable::deleteEntryByID(int32_t id)
{
    if (_entry_allocator->isAllocated(id) == false)
        return false;
    uint32_t &allocate_status = *((uint32_t*)_getEntryMemory(id));
    allocate_status = htobe32(false);
    _entry_allocator->free(id);
    return true;
}
bool StorageTable::deleteEntry(StorageTable::Entry *entry) {
    return deleteEntryByID(entry->_header_index);
}
bool StorageTable::deleteEntryByPrimaryKey(Value *value)
{
    auto index_type = getPrimaryIndex();
    std::list<Entry> deleted_entries;
    if (value->get_value_type() != index_type->type)
        return false;
    traverseReadEntries(
        [&deleted_entries](Entry const &entry) {
            deleted_entries.push_back(entry);
        });
    for (auto &i : deleted_entries)
        deleteEntry(&i);
    return true;
}

const StorageTypeItem *StorageTable::getPrimaryIndex() const 
{
    return &_type_item_list[_primary_index_order];
}

int32_t StorageTable::getTypeIndex(std::string_view name) const
{
    if (!_type_item_index_map.contains(name))
        return 0xFFFF'FFFF;
    return _type_item_index_map.at(name);
}

void StorageTable::eraseAndMakeUnavailable()
{
    std::string entry_filename(_entry_mapper->get_filename());
    std::string index_filename(_index_mapper->get_filename());
    _entry_allocator.reset();
    _entry_mapper.reset();
    _index_mapper.reset();
    _type_item_index_map.clear();
    _has_error = true;
    _type_item_map.clear();
    _type_item_list.clear();
    _type_item_name_buffer.clear();
    std::filesystem::path entry_path(entry_filename);
    std::filesystem::path index_path(index_filename);
    std::filesystem::remove(entry_path);
    std::filesystem::remove(index_path);
}
/* end class StorageTable */

} // namespace mygsql
