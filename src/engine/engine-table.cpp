#include "engine-table.hxx"
#include "base/mtb-object.hxx"
#include "base/sql-value.hxx"
#include "storage/storage-table.hxx"
#include <cstddef>
#include <cstdint>
#include <deque>
#include <iostream>
#include <string_view>

namespace mygsql::engine {

TableEntry::TableEntry(Table &table, ValueListT const &value_list)
    : _table(table), _has_error(false),
      _value_list(value_list),
      _internal_storage_entry(
        table._storage_table->appendEntry(value_list)) {}

TableEntry::TableEntry(Table &table, StorageTable::Entry const &entry)
    : _table(table),
      _internal_storage_entry(entry) {
    for (StorageTypeItem const& i: _table._storage_table->get_type_item_list()) {
        _value_list.push_back(entry.get(i.name));
    }
}

Value *TableEntry::get(std::string_view key)
{
    size_t index = _table._storage_table->getTypeIndex(key);
    if (index == -1)
        return nullptr;
    return _value_list[index].get();
}

bool TableEntry::set(std::string_view key, Value *value)
{
    size_t index = _table._storage_table->getTypeIndex(key);
    if (index == -1)
        return false;
    _value_list[index] = value;
    return true;
}
bool TableEntry::set(std::string_view key, std::string_view value)
{
    size_t index = _table._storage_table->getTypeIndex(key);
    if (index == -1)
        return false;
    _value_list[index] = new StringValue(value);
    return true;
}
bool TableEntry::set(std::string_view key, int32_t value)
{
    size_t index = _table._storage_table->getTypeIndex(key);
    if (index == -1)
        return false;
    _value_list[index] = new IntValue(value);
    return true;
}

void TableEntry::sync()
{
    StorageTable::TypeItemListT const &ti_list {
        _table._storage_table->get_type_item_list()
    };
    for (int index = 0; Value *value: _value_list) {
        StorageTypeItem const &item = ti_list[index];
        _internal_storage_entry.set(item.name, *value);
        index++;
    }
}

void TableEntry::removeAndMakeUnavailable()
{
    _table._storage_table->deleteEntry(&_internal_storage_entry);
    _has_error = true;
}

Table::Table(StorageTable &table)
    : _storage_table(&table),
      _name(table.get_name()),
      _primary_key_index(table.get_primary_index_order()) {
    _initializeFromStorageTable();
}

void Table::_initializeFromStorageTable()
{
    _storage_table->traverseReadEntries(
        [this](StorageTable::Entry const &entry) mutable {
            owned<TableEntry> tentry = new TableEntry(*this, entry);
            _entry_list.push_back(std::move(tentry));
        });
    // 主键代码涉及外部修改，不能使用! 不能使用! 不能使用!
    // if (_primary_key_index != -1) {
    //     for (auto &i: _entry_list) {
    //         _entry_map.insert({
    //             i->_value_list[_primary_key_index].get(),
    //             i});
    //     }
    // }
}

TableEntry *Table::insert(TableEntry::ValueListT const &value_list)
{
    owned<TableEntry> entry = new TableEntry(*this, value_list);
    if (nullptr == entry.get())
        return nullptr;
    TableEntry *ret = entry;
    _entry_list.push_back(std::move(entry));
    return ret;
}

Table::EntrySelectListT Table::selectAll()
{
    EntrySelectListT list = {};
    for (EntryListT::iterator i = _entry_list.begin();
         i != _entry_list.end();
         i++) {
        list.push_back(std::move(i));
    }
    return list;
}

std::deque<Value*> Table::selectAllValue(std::string_view column)
{
    std::deque<Value*> ret{};
    for (auto &i: _entry_list) {
        Value *val = i->get(column);
        if (val == nullptr) {
            throw TableEntry::ColumnUnmatchedException(column);
        }
        ret.push_back(val);
    }
    return ret;
}

Table::EntrySelectListT Table::selectByCondition(
                            std::string_view   condition_column,
                            TotalOrderRelation relation,
                            Value             *condition_value)
{
    // AC
    // std::cout << "DEBUGGING" << std::endl;
    EntrySelectListT ret{};
    for (EntryListT::iterator i = _entry_list.begin();
         i != _entry_list.end();
         i++) {
        Value *iteratored_value = (*i)->get(condition_column);
        // std::cout << condition_column
        //           << " type: "
        //           << ValueTypeGetString(iteratored_value->get_value_type())
        //           << std::endl;
        if (iteratored_value == nullptr) {
            throw TableEntry::ColumnUnmatchedException(condition_column);
        }
        if (ValueMeetsCondition(relation, iteratored_value, condition_value)) {
            ret.push_back(i);
        }
    }
    // throw TableEntry::ColumnUnmatchedException(condition_column);
    return ret;
}

std::deque<Value*> Table::selectValueByCondition(
                        std::string_view   column,
                        std::string_view   condition_column,
                        TotalOrderRelation relation,
                        Value             *condition_value)
{
    EntrySelectListT list = selectByCondition(condition_column, relation, condition_value);
    std::deque<Value*> ret;
    for (auto &i: list) {
        TableEntry *entry = (*i).get();
        Value *value = entry->get(column);
        if (value == nullptr) {
            throw TableEntry::ColumnUnmatchedException(column);
        }
        ret.push_back(value);
    }
    return ret;
}

size_t Table::updateEntireTable(std::string_view column, Value *value)
{
    size_t ret_update_count = 0;
    for (auto &i: _entry_list) {
        if (i->set(column, value) == false)
            return 0;
        ret_update_count++;
    }
    return ret_update_count;
}

size_t Table::updateTableByCondition(
            std::string_view column,     Value *value,
            /* condition */
            std::string_view   condition_column,
            TotalOrderRelation relation, Value *condition_value)
{
    size_t ret_update_count = 0;
    for (auto &i: _entry_list) {
        if (ValueMeetsCondition(relation,
                                i->get(condition_column),
                                condition_value)) {
            i->set(column, value);
            ret_update_count++;
        }
    }
    return ret_update_count;
}

size_t Table::deleteEntryByCondition(std::string_view   condition_column,
                                     TotalOrderRelation relation,
                                     Value *condition_value)
{
    std::list<owned<TableEntry>> remove_list;
    for (auto &i: _entry_list) {
        Value *iter_value = i->get(condition_column);
        if (iter_value == nullptr) {
            throw TableEntry::ColumnUnmatchedException(condition_column);
        }
        if (ValueMeetsCondition(relation, iter_value, condition_value)) {
            remove_list.push_back(i);
        }
    }
    for (auto &i: remove_list) {
        i->removeAndMakeUnavailable();
        _entry_list.remove(i);
    }
    return remove_list.size();
}

void Table::syncToStorageTable()
{
    for (auto &i: _entry_list)
        i->sync();
}

} // namespace mygsql