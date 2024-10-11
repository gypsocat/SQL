#ifndef __MYG_SQL_ENGINE_ENGINE_H__
#define __MYG_SQL_ENGINE_ENGINE_H__

#include "base/mtb-exception.hxx"
#include "base/mtb-object.hxx"
#include "base/sql-value.hxx"
#include "engine-database-manager.hxx"
#include "engine/engine-database.hxx"
#include "engine/engine-table.hxx"
#include "storage/storage-table.hxx"
#include <cstddef>
#include <deque>
#include <format>
#include <string_view>
#include <utility>
#include <vector>

namespace mygsql::engine {
using MTB::owned;
using MTB::Object;

/** @class Engine
 * @brief 执行引擎，负责汇总各模块，负责最终所有命令的实现 */
class Engine final: public MTB::Object {
public:
    friend class DataBaseExpiredException;
    friend class TableUnexistException;

    /** @class DataBaseExpiredException
     * @brief Engine上下文被use的DataBase已经失效了。当你接收到这个异常时，要检查
     *        用户是否删除了一个正在被use的DataBase. */
    class DataBaseExpiredException: public MTB::Exception {
    public:
        DataBaseExpiredException(const Engine *engine)
            : MTB::Exception(MTB::ErrorLevel::CRITICAL,
                std::format("current data base `{}` is expired",
                engine->_current_database_name)),
              engine(engine) {
            msg = std::format(
                    "DataBaseExpiredException at function<{}> file<{}> line<{}>: {}",
                        location.function_name(),
                        location.file_name(),
                        location.line(),
                        msg);
        }
    public:
        const Engine *engine;
    }; // class DataBaseExpiredException

    /** @class TableUnexistException
     * @brief  在执行引擎engine里，名为table_name的表不存在 */
    class TableUnexistException: public MTB::Exception {
    public:
        TableUnexistException(Engine *engine, std::string_view table_name)
            : engine(engine),
              table_name(table_name),
              MTB::Exception(MTB::ErrorLevel::CRITICAL, "") {
            constexpr const char* const error_format = 
R"(TableUnexistException at at function<{}> file<{}> line<{}>: table {} does not exist)";
            msg = std::format(error_format,
                    location.function_name(), location.file_name(),
                    location.line(), table_name);
        }
        Engine          *engine;
        std::string_view table_name;
    }; // class TableUnexistException

    /** @struct Condition
     * @brief 表示where条件的三元组 */
    struct Condition {
        std::string            name;
        TotalOrderRelation relation;
        Value      *condition_value;
    }; // struct Condition

    /** 键-值对，first存储的是列名称, second存储的是被选中的列的值 */
    using NameValuePairT   = std::pair<std::string_view, Value*>;
    /** 键-值对列表, 存储一行条目的所有值，或者存储一列条目的所有值 */
    using NameValueListT   = std::vector<NameValuePairT>;
    /** 键-值对矩阵, 存储的是若干被选中的条目 */
    using NameValueMatrixT = std::deque<NameValueListT>;
    /** Value智能指针 */
    using ValuePtrT  = TableEntry::ValuePtrT; // owned<Value>
    /** Value智能指针列表, 类型为vector. */
    using ValueListT = TableEntry::ValueListT;
public:
    Engine(std::string_view storage_path);
    ~Engine() override;
    
    /** database 管理命令 */
    DataBase *createDataBase(std::string_view name);
    DataBase *useDataBase(std::string_view name);
    bool dropDataBase(std::string_view name);

    /** table 管理命令 */
    Table *createTable(std::string_view name,
                       StorageTable::TypeItemListT &&type_item_list);
    bool dropTable(std::string_view name);

    /** select table命令 */
    NameValueMatrixT selectFromTable(std::string_view table_name);
    NameValueMatrixT selectFromTable(std::string_view table_name,
                                     Condition const &condition);
    std::deque<Value*> selectValueFromTable(std::string_view table_name,
                                    std::string_view column);
    std::deque<Value*> selectValueFromTable(std::string_view table_name,
                                    std::string_view column,
                                    Condition const &condition);

    /** delete table命令. 返回删除了多少元素。 */
    size_t deleteValueFromTable(std::string_view table_name);
    size_t deleteValueFromTable(std::string_view table_name,
                                Condition const &condition);

    /** insert命令，返回插入的值列表 */
    NameValueListT insertToTable(std::string_view table_name,
                                 ValueListT const &value_list);
    /** @brief update-set命令
     * @param table_name 表名称
     * @param column     列名称
     * @param value      新值 */
    size_t updateTable(std::string_view table_name,
                       std::string_view column,
                       Value *value);
    /** @brief update-set命令
     * @param table_name 表名称
     * @param column     列名称
     * @param value      新值
     * @param condition  Where条件 */
    size_t updateTable(std::string_view table_name,
                       std::string_view column,
                       Value *value, Condition const &condition);
    
    /** @brief sync all命令 */
    void syncAll();
    /** @brief sync 命令 */
    void syncCurrent();

    /* getters */
    /** getter:current_database 当前被use的数据库 */
    const DataBase *get_current_database() const {
        return _current_database;
    }
    /** getter:current_database_name 当前被use的数据库的名称 */
    std::string_view get_current_database_name() const {
        return _current_database_name;
    }
private:
    DataBaseManager  _database_manager;
    DataBase        *_current_database;
    std::string      _current_database_name;

    Table *_tryGetTable(std::string_view table_name);
}; // class Engine


} // namespace mygsql

#endif