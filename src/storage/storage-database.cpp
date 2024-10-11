#include "storage-database.hxx"
#include "storage-table.hxx"
#include <filesystem>
#include <regex>
#include <string_view>
#include <unordered_set>

namespace mygsql {

using MTB::unowned;
const std::regex extension_pattern(R"(\.[^\.]+$)");

StorageDataBase::StorageDataBase(std::string_view twd, std::string_view name)
    : _name(name), _work_dir(twd), _has_error(false) {
    _work_dir /= name;
    if (std::filesystem::exists(_work_dir)) {
        _loadTables();
        return;
    }
    std::filesystem::create_directory(_work_dir);
}

void StorageDataBase::_loadTables()
{
    if (!std::filesystem::is_directory(_work_dir))
        return;
    std::unordered_set<std::string> name_set;
    for (auto &entry: std::filesystem::directory_iterator(_work_dir)) {
        std::string filename(entry.path().filename().string());
        std::string name(std::regex_replace(filename, extension_pattern, ""));
        if (!name_set.contains(name))
            name_set.insert(std::move(name));
    }
    for (std::string const &name: name_set) {
        StorageTable *table = new StorageTable(_work_dir.string(), name);
        _table_map.insert({
            table->get_name(), table
        });
    }
}

unowned<StorageTable> StorageDataBase::createTable(std::string_view name,
                                                   TypeItemListT type_items)
{
    if (!_table_map.contains(name)) {
        _table_map.insert({name,
            new StorageTable(_work_dir.string(), name, type_items)});
    }
    return _table_map.at(name).get();
}

bool StorageDataBase::dropTable(std::string_view name)
{
    if (!_table_map.contains(name))
        return false;
    StorageTable *table = _table_map.at(name);
    table->eraseAndMakeUnavailable();
    _table_map.erase(name);
    return true;
}

void StorageDataBase::eraseAndMakeUnavailable()
{
    _table_map.clear();
    std::filesystem::remove(_work_dir);
    _has_error = true;
}

} // namespace mygsql