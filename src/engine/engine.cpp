#include "engine.hxx"
#include "base/sql-value.hxx"
#include "engine/engine-database.hxx"
#include "engine/engine-table.hxx"
#include "storage/storage-table.hxx"
#include <cstddef>
#include <deque>
#include <format>
#include <iostream>
#include <string_view>

namespace mygsql::engine {

Engine::Engine(std::string_view path)
    : _database_manager(path),
      _current_database(nullptr),
      _current_database_name("<undefined>") {
}

Engine::~Engine() {
    syncAll();
}

/** private table */

inline Table *Engine::_tryGetTable(std::string_view name)
{
    if (_current_database == nullptr) {
        throw DataBaseExpiredException(this);
    }
    Table *table = _current_database->useTable(name);
    if (table == nullptr) {
        throw TableUnexistException(
            this,
            std::format("{}[{}]",
                    _current_database_name, name)
        );
    }
    return table;
}


/** public Table */

DataBase *Engine::createDataBase(std::string_view name)
{
    return _database_manager.createDataBase(name);
}

DataBase *Engine::useDataBase(std::string_view name)
{
    DataBase *ret = _database_manager.getDataBase(name);
    if (ret == nullptr)
        return nullptr;
    _current_database = ret;
    _current_database_name = _current_database->get_name();
    return ret;
}

bool Engine::dropDataBase(std::string_view name)
{
    if (name == _current_database_name) {
        _current_database = nullptr;
    }
    return _database_manager.dropDataBase(name);
}

Table *Engine::createTable(std::string_view name,
                           StorageTable::TypeItemListT &&type_item_list)
{
    if (_current_database == nullptr) {
        throw DataBaseExpiredException(this);
    }
    return _current_database->createTable(name, type_item_list);
}

bool Engine::dropTable(std::string_view name)
{
    if (_current_database == nullptr) {
        throw DataBaseExpiredException(this);
    }
    return _current_database->dropTable(name);
}

Engine::NameValueMatrixT Engine::selectFromTable(std::string_view table_name)
{
    NameValueMatrixT ret{};
    Table *table = _tryGetTable(table_name);
    StorageTable::TypeItemListT const &ti_list = table->get_type_item_list();
    for (TableEntry *i: table->get_entry_list()) {
        NameValueListT ret_item;
        for (int cnt = 0; Value *i: i->get_value_list()) {
            ret_item.push_back({
                ti_list[cnt].name, i    
            });
            cnt++;
        }
        ret.push_back(std::move(ret_item));
    }
    return ret;
}
Engine::NameValueMatrixT Engine::selectFromTable(std::string_view table_name,
                                                 Condition const &condition)
{
    Table *table = _tryGetTable(table_name);
    NameValueMatrixT ret{};
    StorageTable::TypeItemListT const &ti_list = table->get_type_item_list();
    Table::EntrySelectListT select_list {
        table->selectByCondition(condition.name,
                                 condition.relation,
                                 condition.condition_value)
    };
    for (auto &i: select_list) {
        NameValueListT ret_list;
        for (int cnt = 0; auto &j: i->get()->get_value_list()) {
            ret_list.push_back({ti_list[cnt].name, j.get()});
            cnt++;
        }
        ret.push_back(std::move(ret_list));
    }
    return ret;
}
std::deque<Value*> Engine::selectValueFromTable(std::string_view table_name,
                                                std::string_view column)
{
    std::deque<Value*> ret{};
    Table *table = _tryGetTable(table_name);
    ret = table->selectAllValue(column);
    return ret;
}
std::deque<Value*> Engine::selectValueFromTable(std::string_view table_name,
                                                std::string_view column,
                                                Condition const &condition)
{
    ValueListT ret{};
    Table *table = _tryGetTable(table_name);
    return table->selectValueByCondition(column, condition.name,
                                         condition.relation,
                                         condition.condition_value);
}

size_t Engine::deleteValueFromTable(std::string_view table_name)
{
    Table *table = _tryGetTable(table_name);
    size_t ret = table->get_entry_list().size();
    table->clear();
    return ret;
}

size_t Engine::deleteValueFromTable(std::string_view table_name,
                                    Condition const &condition)
{
    Table *table = _tryGetTable(table_name);
    return table->deleteEntryByCondition(condition.name,
                                  condition.relation,
                                  condition.condition_value);
}

Engine::NameValueListT Engine::insertToTable(std::string_view table_name,
                                             Engine::ValueListT const &value_list)
{
    Table *table = _tryGetTable(table_name);
    TableEntry *entry = table->insert(value_list);
    NameValueListT ret;
    auto &ti_list = table->get_type_item_list();
    auto &entry_value_list = entry->get_value_list();
    for (int i = 0; i < entry_value_list.size(); i++) {
        ret.push_back({ti_list[i].name, entry_value_list[i]});
    }
    return ret;
}

size_t Engine::updateTable(std::string_view table_name,
                           std::string_view column,
                           Value *value)
{
    // /*DEBUG*/
    // std::cout << "DEBUGGGGG" << std::endl;
    // std::cout << std::format("table name:'{}',column:'{}',value:'{}'",
    //                 table_name, column, value->getString())
    //           << std::endl;
    // return 0;
    Table *table = _tryGetTable(table_name);
    return table->updateEntireTable(column, value);
}

size_t Engine::updateTable(std::string_view table_name,
                           std::string_view column,
                           Value *value, Condition const &condition)
{
    // /*DEBUG*/
    // std::cout << "DEBUGGGGG" << std::endl;
    // std::cout << std::format("table name:'{}',column:'{}',value:'{}'",
    //                 table_name, column, value->getString())
    //           << std::endl;
    // return 0;
    Table *table = _tryGetTable(table_name);
    return table->updateTableByCondition(column, value, condition.name,
                                         condition.relation,
                                         condition.condition_value);
}

void Engine::syncAll()
{
    for (auto &i: _database_manager.get_database_map()) {
        for (auto &j: i.second.get()->get_table_map())
            j.second.get()->syncToStorageTable();
    }
}

void Engine::syncCurrent()
{
    for (auto &j: _current_database->get_table_map()) {
        j.second.get()->syncToStorageTable();
    }
}

} // namespace mygsql