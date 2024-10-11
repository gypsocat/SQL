#ifndef __MYG_SQL_LANG_INTERPRETER_H__
#define __MYG_SQL_LANG_INTERPRETER_H__

#include "base/mtb-exception.hxx"
#include "base/mtb-object.hxx"
#include "engine/engine.hxx"
#include <cstdint>
#include <format>
#include <set>
#include <string>
#include <string_view>

namespace mygsql {
using MTB::owned;

class Interpreter: public MTB::Object {
public:
    enum class CommandType: int32_t {
        _NONE,
        QUIT,           // 退出
        CREATE_DATABASE,// 创建数据库
        DROP_DATABASE,  // 删除数据库
        USE_DATABASE,   // 选择数据库
        CREATE_TABLE,   // 创建表
        DROP_TABLE,     // 删除表
        SELECT,         // 选择并打印表项
        DELETE,         // 删除表项
        INSERT,         // 插入表项
        UPDATE,         // 更新表列
        SYNC,           // 同步到磁盘映射区
        _COUNT,
    }; // enum class CommandType

    enum class State: int32_t {
        IDLE = int32_t(CommandType::_COUNT),
        RUN, COMMAND_END, EXIT, ERROR
    }; // enum class State

    /** @class IllegalCommandException
     * @brief command命令非法，原因见reason */
    class IllegalCommandException: public MTB::Exception {
    public:
        IllegalCommandException(std::string_view command,
                                std::string_view reason)
            : command(command), reason(reason),
              MTB::Exception(MTB::ErrorLevel::CRITICAL,"") {
            constexpr const char *const illegal_format = 
R"(IllegalCommandException for command '{}': {})";
            msg = std::format(illegal_format, command, reason);
        }
        std::string command;
        std::string reason;
    }; // exception class IllegalCommandException
    using EnginePtrT = owned<engine::Engine>;
public:
    Interpreter(engine::Engine &engine);

    void run(std::string_view command);
    void run();
    std::string_view get_current_command() const {
        return _current_command;
    }
    void set_current_command(std::string_view command);
    void set_current_command(std::string &&command);
    State get_state() const { return _state; }
private:
    engine::Engine  &_executor_engine;
    std::string      _current_command;
    const char*      _current_sentry;
    State            _state;
private:
    static const std::set<char> _illegal_characters;

    //退出程序
    void _do_quit();
    //创建数据库
    void _do_create_database();
    //销毁数据库
    void _do_drop_database();
    //切换数据库
    void _do_use();
    //表的操作都必须在选中数据库之前，所以需要先进行判断
    bool _do_check_if_use();
    //创建表
    void _do_create_table();
    //删除表
    void _do_drop_table();
    //查询表
    void _do_select();
    //删除表中的记录
    void _do_delete();
    //在表中插入数据
    void _do_insert();
    //更新表中数据
    void _do_update();
    //处理未知命令
    void _do_unknown();
    //同步到磁盘
    void _do_sync();
}; // class Interpreter

} // namespace mygsql

#endif