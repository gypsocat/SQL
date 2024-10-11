#include "storage-manager.hxx"
#include "storage-database.hxx"
#include <filesystem>
#include <memory>
#include <string_view>

namespace mygsql {

std::string storage_dirname = "storage";
StorageManager  *global_storage_manager;
static std::unique_ptr<StorageManager> manager_real;

StorageManager::StorageManager(std::string_view work_dir)
    : _work_dir(work_dir) {
    if (!std::filesystem::exists(_work_dir)) {
        std::filesystem::create_directory(_work_dir);
        return;
    }
    for (auto &entry: std::filesystem::directory_iterator(_work_dir)) {
        if (entry.is_directory()) {
            std::string filename = entry.path().filename();
            StorageDataBase *db = new StorageDataBase(_work_dir.string(), filename);
            _database_map.insert({db->get_name(), db});
        }
    }
}

unowned<StorageDataBase> StorageManager::createDataBase(std::string_view name)
{
    if (!_database_map.contains(name)) {
        StorageDataBase *db =  new StorageDataBase(_work_dir.string(), name);
        _database_map.insert({db->get_name(), db});
    }
    return _database_map.at(name).get();
}

bool StorageManager::dropDataBase(std::string_view name)
{
    if (!_database_map.contains(name))
        return false;
    StorageDataBase *db = _database_map.at(name);
    db->eraseAndMakeUnavailable();
    _database_map.erase(name);
    return true;
}

void InitStorageManager(std::string_view argv0)
{
    std::filesystem::path argv0_path(argv0);
    argv0_path = argv0_path.parent_path() / storage_dirname;
    manager_real = std::make_unique<StorageManager>(argv0_path.string());
    global_storage_manager = manager_real.get();
}

} // namespace mygsql