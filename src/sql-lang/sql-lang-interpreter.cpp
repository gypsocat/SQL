#include "base/mtb-object.hxx"
#include "base/sql-value.hxx"
#include "engine/engine-database.hxx"
#include "engine/engine.hxx"
#include "sql-lang-interpreter.hxx"
#include "storage/storage-table.hxx"
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <exception>
#include <format>
#include <iostream>
#include <ostream>
#include <regex>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

using CommandType = mygsql::Interpreter::CommandType;
using CommandTypeMapT = std::unordered_map<std::string_view, CommandType>;
using IllegalCommandException = mygsql::Interpreter::IllegalCommandException;

static char *cstring_jump_space(const char *begin, const char *end)
{
    while (begin != end && std::isspace(*begin))
        begin++;
    return (char*)begin;
}
static char *cstring_to_space(const char *begin, const char *end)
{
    while (begin != end && !std::isspace(*begin))
        begin++;
    return (char*)begin;
}
static std::string_view cstring_get_word(const char *begin, const char *end)
{
    begin = cstring_jump_space(begin, end);
    const char *sentry_end = cstring_to_space(begin, end);
    return {
        begin, sentry_end
    };
}
static std::string_view cstring_get_identifier(const char *begin, const char *end)
{
    begin = cstring_jump_space(begin, end);
    const char *sentry_end = begin;
    while (sentry_end != end && isalnum(*sentry_end))
        sentry_end++;
    return {begin, sentry_end};
}
static bool word_is_identifier(std::string_view word)
{
    if (!isalpha(word[0]))
        return false;
    for (char i: word) {
        if (!isalnum(i))
            return false;
    }
    return true;
}

struct CommandTypeContext {
    CommandType command_type;
    const char *current_ptr;
}; // struct CommandTypeContext
static CommandTypeContext
handle_second_opcode(const char *opcode_end, const char *end,
                     CommandTypeMapT const &map)
{
    const char *begin_sentry = cstring_jump_space(opcode_end, end);
    const char *end_sentry   = cstring_to_space(begin_sentry, end);
    std::string_view _2nd_opcode = {begin_sentry, end_sentry};
    if (map.contains(_2nd_opcode))
        return {map.at(_2nd_opcode), end_sentry};
    return {CommandType::_NONE, end_sentry};
}

static CommandTypeContext
command_get_type(std::string_view command)
{
    using namespace mygsql;
    static CommandTypeMapT command_type_map {
        {"use",    CommandType::USE_DATABASE},
        {"select", CommandType::SELECT},
        {"delete", CommandType::DELETE},
        {"insert", CommandType::INSERT},
        {"update", CommandType::UPDATE},
        {"sync",   CommandType::SYNC},
        {"exit",   CommandType::QUIT},
        {"quit",   CommandType::QUIT}
    };
    static CommandTypeMapT create_2nd_opcode_map {
        {"database", CommandType::CREATE_DATABASE},
        {"table",    CommandType::CREATE_TABLE}
    };
    static CommandTypeMapT drop_2nd_opcode_map {
        {"database", CommandType::DROP_DATABASE},
        {"table",    CommandType::DROP_TABLE}
    };

    const char *begin_sentry = cstring_jump_space(command.data(), command.end());
    const char *end_sentry   = cstring_to_space(begin_sentry, command.end());
    if (std::string_view{begin_sentry, end_sentry} == "create") {
        auto [ret, new_end] = handle_second_opcode(end_sentry,
                                               command.end(),
                                               create_2nd_opcode_map);
        if (ret == CommandType::_NONE) {
            throw mygsql::Interpreter::IllegalCommandException(
                command, "word 'create' must follow 'database' or 'table'");
        }
        return {ret, new_end};
    }
    if (std::string_view{begin_sentry, end_sentry} == "drop") {
        auto [ret, new_end] = handle_second_opcode(end_sentry,
                                               command.end(),
                                               drop_2nd_opcode_map);
        if (ret == CommandType::_NONE) {
            throw mygsql::Interpreter::IllegalCommandException(
                command, "word 'drop' must follow 'database' or 'table'");
        }
        return {ret, new_end};
    }
    std::string_view opcode = {begin_sentry, end_sentry};
    if (command_type_map.contains(opcode))
        return {command_type_map.at(opcode), end_sentry};

    /* 所有可能的命令都枚举完了，现在必有错 */
    std::string error_message = std::format("unknown opcode `{}`", opcode);
    throw IllegalCommandException(command, error_message);
}

namespace mygsql {

using namespace engine;
using Condition = Engine::Condition;

Interpreter::Interpreter(engine::Engine &engine)
    : _executor_engine(engine),
      _current_command(""),
      _state(State::IDLE) {
}
const std::set<char> Interpreter::_illegal_characters {
    '\\', '/', ':', '?', '|',
};

void Interpreter::set_current_command(std::string_view command)
{
    for (char i: command) {
        if (_illegal_characters.contains(i)) {
            throw IllegalCommandException(command,
                std::format("illegal character `{}`", i));
        }
    }
    _current_command = command;
    _current_sentry  = &_current_command[0];
}
void Interpreter::set_current_command(std::string &&command)
{
    for (char i: command) {
        if (_illegal_characters.contains(i)) {
            throw IllegalCommandException(command,
                std::format("illegal character `{}`", i));
        }
    }
    _current_command = std::move(command);
    _current_sentry  = &_current_command[0];
}

void Interpreter::run(std::string_view command)
{
    set_current_command(command);
    run();
}

void Interpreter::_do_quit() {
    _executor_engine.syncAll();
    _state = State::EXIT;
}
void Interpreter::_do_create_database()
{
    const char *end = _current_command.end().base();
    _current_sentry = cstring_jump_space(_current_sentry, end);
    const char *sentry_end = cstring_to_space(_current_sentry, end);
    std::string_view database_name = {
        _current_sentry, sentry_end
    };
    engine::DataBase *db = _executor_engine.createDataBase(database_name);
    if (db == nullptr) {
        std::cout << "Database "<< database_name
            << " has already created, or there exists an error."
            << std::endl;
    } else {
        std::cout << std::format("Database {} successfully created.", database_name)
                  << std::endl;
    }
    _state = State::COMMAND_END;
}

void Interpreter::_do_drop_database()
{
    const char *end         = _current_command.end().base();
    std::string_view dbname = cstring_get_word(_current_sentry, end);
    bool drop_result        = _executor_engine.dropDataBase(dbname);
    if (drop_result == false) {
        std::cout << std::format("database named '{}' not exist",
                                 dbname)
                  << std::endl;
        return;
    }
    std::cout << std::format("Database named '{}' successfully removed",
                             dbname)
              << std::endl;
}

void Interpreter::_do_use()
{
    const char *end         = _current_command.end().base();
    std::string_view dbname = cstring_get_word(_current_sentry, end);
    engine::DataBase *db = _executor_engine.useDataBase(dbname);
    if (db == nullptr) {
        std::cout << std::format("Database named '{}' not exist",
                                 dbname)
                  << std::endl;
        return;
    }
    std::cout << std::format("Now using '{}' as current data base.",
                             dbname)
              << std::endl;
}

bool Interpreter::_do_check_if_use()
{
    if (_executor_engine.get_current_database() == nullptr) {
        std::cout << std::format("Critical: current database `{}` is NOT available",
                                 _executor_engine.get_current_database_name())
                  << std::endl;
        return false;
    }
    return true;
}

static StorageTable::TypeItemListT
create_tilist_from_string(std::string_view tilist_string_view,
                          std::vector<std::string> &out_cut_string);

/** 语法:
 * CreateTable: 'create' 'table' WORD '(' TypeItemList ')' */
void Interpreter::_do_create_table()
{
    /* "create table" */
    const char *end = _current_command.end().base();

    /* WORD */
    std::string_view table_name = cstring_get_identifier(_current_sentry, end);
    if (table_name.length() == 0) {
        throw IllegalCommandException(_current_command,
                    "table creation requires a table name");
    }

    /* '(' */
    _current_sentry = cstring_jump_space(table_name.end(), end);
    if (*_current_sentry != '(') {
        throw IllegalCommandException(_current_command,
                    "table creation requires a type item list");
    } _current_sentry++; // jumps '('
    /* 错误情况: "()" */
    _current_sentry = cstring_jump_space(_current_sentry, end);
    if (*_current_sentry == ')') {
        throw IllegalCommandException(_current_command,
                    "table creation encounters an empty type item list");
    }

    /* 获取类型初始化列表 */
    std::string_view init_list_str;
    const char *ilist_begin = _current_sentry;
    const char *ilist_end   = ilist_begin;
    while (ilist_end != _current_command.end().base() &&
           *ilist_end != ')') {
        ilist_end++;
    }
    // 判断括号是否闭合
    if (_current_sentry == _current_command.end().base()) {
        throw IllegalCommandException(_current_command,
                    "table creation encounters non-closed quote");
    }
    // 通过初始化列表字符串得到类型列表
    init_list_str = {ilist_begin, ilist_end};
    std::vector<std::string> init_list;
    StorageTable::TypeItemListT ti_list = create_tilist_from_string(
            init_list_str, init_list);
    
    // 构建Table
    std::cout << "creating table " << table_name << std::endl;
    engine::Table *table = _executor_engine.createTable(table_name, std::move(ti_list));
    if (table == nullptr) {
        std::cout << "Table creation failed." << std::endl;
    }
    /* 输出 */
    std::cout << "created table {\n";
    for (auto &i: ti_list) {
        std::cout << std::format("  [name:'{}', type:'{}', is primary:{}]\n",
            i.name, ValueTypeGetString(i.type),
            i.is_primary ? "true":"false");
    }
    std::cout << "}" << std::endl;
}

static StorageTypeItem
create_typeitem_from_string(std::string const &str);

/** 语法:
 * TypeItemList: TypeItem
 *             | TypeItem ',' TypeItemList */
static StorageTable::TypeItemListT
create_tilist_from_string(std::string_view tilist_string_view,
                          std::vector<std::string> &out_cut_string)
{
    std::string tilist_string(tilist_string_view);
    static std::regex splitter("[,]");
    StorageTable::TypeItemListT ret;
    out_cut_string = std::vector<std::string> {
        std::sregex_token_iterator(
            tilist_string.begin(),
            tilist_string.end(),
            splitter, -1
        ),
        std::sregex_token_iterator()
    };
    for (std::string const &i: out_cut_string) {
        ret.push_back(create_typeitem_from_string(i));
    }
    // for (auto &ti: ret) {
    //     std::cout << "prebuild: ti.name    =" << ti.name << std::endl
    //               << "          ti.primary =" << ti.is_primary << std::endl
    //               << "          ti.type    =" << ValueTypeGetString(ti.type) << std::endl;
    // } // comfirmed
    return ret;
}

/** 语法:
 * StorageTypeItem: column type
 *                | column type 'primary' */
static StorageTypeItem
create_typeitem_from_string(std::string const &str)
{
    StorageTypeItem ti{};
    const char *end    = str.end().base();
    const char *cursor = cstring_jump_space(str.begin().base(), end);
    // std::cout << "got type string: '" << str << "'" << std::endl;

    std::string_view column = cstring_get_identifier(cursor, end);
    ti.name = column;
    cursor = column.end();

    std::string_view type = cstring_get_identifier(cursor, end);
    if (type == "int") {
        ti.type = Value::Type::INT;
    } else if (type == "string") {
        ti.type = Value::Type::STRING;
    } else {
        throw IllegalCommandException(str,
            std::format("you must set 'int' or 'string' as type",
            type));
    }
    cursor = type.end();

    cursor = cstring_jump_space(cursor, end);
    // std::cout << "prebuild: ti.name    =" << ti.name << std::endl
    //           << "          ti.primary =" << ti.is_primary << std::endl
    //           << "          ti.type    =" << ValueTypeGetString(ti.type) << std::endl;
    if (cursor == end || *cursor == ',' || *cursor == ')')
        return ti;
    std::string_view primary = cstring_get_identifier(cursor, end);
    // std::cout << "primary:" << primary << std::endl;
    if (primary == "primary")
        ti.is_primary = true;
    // std::cout << "aftbuild: ti.name    =" << ti.name << std::endl
    //           << "          ti.primary =" << ti.is_primary << std::endl
    //           << "          ti.type    =" << ValueTypeGetString(ti.type) << std::endl;
    return ti;
}

void Interpreter::_do_drop_table()
{
    const char *end = _current_command.end().base();
    std::string_view table_name = cstring_get_word(_current_sentry, end);
    bool drop_result = _executor_engine.dropTable(table_name);
    if (drop_result == false) {
        std::cout << std::format("drop table '{}' failed", table_name)
                  << std::endl;
    }
    std::cout << std::format("Successfully deleted table '{}'", table_name)
              << std::endl;
}

struct StringValueContext {
    std::string string_value;
    const char *current_position;

    operator std::string() && {
        return std::move(string_value);
    }
    operator std::string() const& {
        return string_value;
    }
    operator const char*() const {
        return current_position;
    }
}; // struct StringValueContext
static StringValueContext interpret_get_string_value(std::string_view value_string)
{
    std::string ret;
    if (value_string[0] != '"')
        return {};
    const char *end = value_string.end();
    const char *cur = value_string.begin() + 1;
    while (cur != end) {
        if (*cur == '"')
            break;
        if (*cur == '\\') {
            cur++;
            if (cur == end) break;
            switch (*cur) {
            case 'n': ret += '\n'; break;
            case 't': ret += '\t'; break;
            default: ret += *cur;
            }
        } else {
            ret += *cur;
        }
        cur++;
    }
    if (cur != end)
        cur++;
    // AC
    // std::cout << "got string value:" << ret << std::endl;
    // throw IllegalCommandException(value_string, "debug purpous");
    return {ret, cur};
}

struct ValuePositionContext {
    Value *owned_value;
    const char *current_position;
}; // struct ValueStringContext
static ValuePositionContext interpret_get_value(std::string_view value_string)
{
    if (value_string.empty()) {
        return {nullptr, value_string.begin()};
    }
    Value *ret_value;
    const char *end = value_string.end();
    const char *cur = cstring_jump_space(value_string.begin(), end);
    if (end == cur)
        return {nullptr, cur};
    if (*cur == '"') {
        // interpret get string value
        auto [value_raw, new_cur] = interpret_get_string_value({cur, end});
        if (value_raw.empty()) {
            throw IllegalCommandException(
                value_string, "empty value string");
        }
        ret_value = new StringValue(value_raw);
        cur = new_cur;
    } else {
        // interpret get int value
        int64_t i64value = 0;
        int sign = 1;
        if (*cur == '-') {
            sign = -1;
            cur++;
        }
        while (cur != end && isdigit(*cur)) {
            i64value = i64value * 10 + *cur - '0';
            cur++;
        }
        i64value *= sign;
        // AC
        // std::cout << "int value: " << i64value << std::endl;
        // throw IllegalCommandException(value_string, "debug purpous");
        ret_value = new IntValue(i64value);
    }
    return {ret_value, cur};
}

static Engine::ValueListT interpret_get_value_list(std::string_view value_string)
{
    const char *end = value_string.end();
    Engine::ValueListT ret;
    const char *cur = value_string.begin();
    // std::cout << "===============[Debug]===============" << std::endl;
    while (cur != end) {
        cur = cstring_jump_space(cur, end);
        auto [value, new_cur] = interpret_get_value({cur, end});
        new_cur = cstring_jump_space(new_cur, end);
        if (value == nullptr) {
            throw IllegalCommandException(value_string,
                    "Value string contains illegal character");
        }
        ret.push_back(value);
        // std::cout << std::format("type:{}, value:'{}'",
        //                 ValueTypeGetString(value->get_value_type()),
        //                 value->getString())
        //           << std::format(" new cur: {}", std::string_view{new_cur, end})
        //           << std::endl;
        if (new_cur == end) {
            throw IllegalCommandException(value_string,
                    "Value string should end with ')'");
        }
        if (*new_cur == ')') {
            break;
        }
        if (*new_cur != ',') {
            throw IllegalCommandException(value_string,
                    "Value list should be separated with comma");
        }

        cur = new_cur + 1;
    }
    // AC
    // throw IllegalCommandException(value_string, "debug purpous");
    return ret;
}

static Condition interpret_get_condition(std::string_view condition)
{
    Condition ret = {
        "", TotalOrderRelation::NONE, nullptr
    };
    const char *cur = condition.begin();
    const char *end = condition.end();
    std::string_view condition_column = cstring_get_identifier(cur, end);
    if (!word_is_identifier(condition_column)) {
        throw IllegalCommandException(condition,
                "column detected illegal character");
    }
    ret.name = condition_column;
    cur = condition_column.end();
    std::string_view op = cstring_get_word(cur, end);
    if (op.contains(">")) {
        ret.relation = TotalOrderRelation::GT;
    } else if (op.contains("<")) {
        ret.relation = TotalOrderRelation(
                (int32_t)ret.relation | (int32_t)TotalOrderRelation::LT);
    } else if (op.contains("=")) {
        ret.relation = TotalOrderRelation(
                (int32_t)ret.relation | (int32_t)TotalOrderRelation::EQ);
    }
    cur = op.end();
    auto [cond_value, new_cur] = interpret_get_value({cur, end});
    ret.condition_value = cond_value;
    // AC
    // std::cout << "DEBUG:" << std::endl;
    // std::cout << std::format("condition column='{}' op={:04b} value.type={} value={}",
    //                          ret.name, int32_t(ret.relation),
    //                          ValueTypeGetString(ret.condition_value->get_value_type()),
    //                          ret.condition_value->getString())
    //           << std::endl;
    // throw IllegalCommandException(condition, "debugging...");
    return ret;
}

static void print_matrix_selector(Engine::NameValueMatrixT const &selector)
{
    if (selector.empty()) {
        std::cout << "No value selected." << std::endl;
    } else {
        std::cout << "column head:" << std::endl;
        for (auto &i: selector[0])
            std::cout << std::format("{:16s}", i.first);
        std::cout << std::endl;
        for (auto &i: selector) {
            for (auto &j: i)
                std::cout << std::format("{:16s}", j.second->getString());
            std::cout << std::endl;
        }
    }
}
static void print_listed_selector(std::deque<Value*> &selector)
{
    if (selector.empty()) {
        std::cout << "No value selected." << std::endl;
        return;
    }
    for (auto &i: selector)
        std::cout << i->getString() << std::endl;
}
static void print_listed_selector(Engine::ValueListT &selector)
{
    if (selector.empty()) {
        std::cout << "No value selected." << std::endl;
        return;
    }
    for (auto &i: selector)
        std::cout << i->getString() << std::endl;
}

/** Select: 'select' WORD 'from' WORD
 *        | 'select' WORD 'from' WORD 'where' WhereCondition */
void Interpreter::_do_select()
{
    const char *end = _current_command.end().base();
    std::string_view column = cstring_get_word(_current_sentry, end);
    _current_sentry = column.end();
    std::string_view from = cstring_get_identifier(_current_sentry, end);
    if (from != "from") {
        throw IllegalCommandException(_current_command,
            std::format("'select' statement must follow 'from'"));
    } _current_sentry = from.end();
    std::string_view table = cstring_get_word(_current_sentry, end);
    if (!word_is_identifier(table)) {
        throw IllegalCommandException(_current_command, "deteted illegal character");
    }
    _current_sentry = table.end();
    std::string_view where = cstring_get_identifier(_current_sentry, end);

    /* select all */
    if (where != "where") {
        if (column == "*") {
            auto selector = _executor_engine.selectFromTable(table);
            print_matrix_selector(selector);
        } else {
            auto selector = _executor_engine.selectValueFromTable(table, column);
            std::cout << "select column: " << column << std::endl;
            print_listed_selector(selector);
        }
        return;
    }
    /* select where */
    _current_sentry = where.end();
    Condition condition = interpret_get_condition({_current_sentry, end});
    MTB::owned<Value> value_lifetime_proxy = condition.condition_value;
    if (column == "*") {
        auto selector = _executor_engine.selectFromTable(table, condition);
        print_matrix_selector(selector);
    } else {
        auto selector =
            _executor_engine.selectValueFromTable(table, column, condition);
        print_listed_selector(selector);
    }
}
void Interpreter::_do_delete()
{
    const char *end = _current_command.end().base();
    std::string_view table = cstring_get_word(_current_sentry, end);
    _current_sentry = table.end();
    std::string_view where = cstring_get_identifier(_current_sentry, end);
    if (where != "where") {
        size_t nelems = _executor_engine.deleteValueFromTable(table);
        std::cout << std::format("deleted {} elements.", nelems)
                  << std::endl;
        return;
    }
    _current_sentry = where.end();
    Condition condition = interpret_get_condition({_current_sentry, end});
    MTB::owned<Value> lifetime_proxy{condition.condition_value};
    size_t nelems = _executor_engine.deleteValueFromTable(table, condition);
    std::cout << "deleted " << nelems << " elements." << std::endl;
}

void Interpreter::_do_insert()
{
    const char *end = _current_command.end().base();
    std::string_view table = cstring_get_word(_current_sentry, end);
    _current_sentry = table.end();
    std::string_view values = cstring_get_word(_current_sentry, end);
    if (values != "values") {
        throw IllegalCommandException(_current_command, 
            "insert command should follow 'values'");
    }
    
    /* '(' */
    _current_sentry = cstring_jump_space(values.end(), end);
    if (*_current_sentry != '(') {
        throw IllegalCommandException(_current_command,
                    "value insertion requires a value list");
    } _current_sentry++; // jumps '('
    /* 错误情况: "()" */
    _current_sentry = cstring_jump_space(_current_sentry, end);
    if (*_current_sentry == ')') {
        throw IllegalCommandException(_current_command,
                    "table insertion encounters an empty value list");
    }

    /* 获取值初始化列表 */
    std::string_view init_list_str;
    const char *ilist_begin = _current_sentry;
    Engine::ValueListT value_list(
        interpret_get_value_list({ilist_begin, end})
    );
    Engine::NameValueListT name_value_list {
        _executor_engine.insertToTable(table, value_list)
    };
    std::cout << "inserted an entry:" << std::endl;
    for (auto &i: name_value_list) {
        std::cout << std::format("{}:{}", i.first, i.second->getString())
                  << std::endl;
    }
}

void Interpreter::_do_update()
{
    const char *end = _current_command.end().base();
    /* table */
    std::string_view table = cstring_get_identifier(_current_sentry, end);
    _current_sentry = table.end();

    /* 'set' */
    std::string_view keyword_set = cstring_get_identifier(_current_sentry, end);
    if (keyword_set != "set") {
        _state = State::ERROR;
        throw IllegalCommandException(_current_command,
                    "update command should follow 'set'");
    }
    _current_sentry = keyword_set.end();

    /* column */
    std::string_view column = cstring_get_identifier(_current_sentry, end);
    _current_sentry = cstring_jump_space(column.end(), end);
    if (*_current_sentry != '=') {
        _state = State::ERROR;
        throw IllegalCommandException(_current_command,
            "update command with 'set' expression should look like `column = value`");
    }
    _current_sentry = cstring_jump_space(_current_sentry + 1, end);
    auto [const_value, cur] = interpret_get_value({_current_sentry, end});
    owned<Value> value_lifetime_proxy(const_value);

    /* 'where' */
    _current_sentry = cstring_jump_space(cur, end);
    std::string_view where = cstring_get_identifier(_current_sentry, end);
    if (_current_sentry == end || where != "where") {
        size_t nelems {
            _executor_engine.updateTable(table, column, const_value)
        };
        std::cout << "updated " << nelems << " elements" << std::endl;
        return;
    }
    /* WhereCondition */
    _current_sentry = where.end();
    Condition condition = interpret_get_condition({_current_sentry, end});
    size_t nelems = _executor_engine.updateTable(table, column, const_value, condition);
    std::cout << "updated " << nelems << " elements" << std::endl;
}

void Interpreter::_do_sync()
{
    _executor_engine.syncAll();
}

void Interpreter::run() try {
    const char *cmd_begin = cstring_jump_space(
            _current_command.begin().base(),
            _current_command.end().base());
    if (cmd_begin == _current_command.end().base())
        return;
    auto [command_type,
          current_ptr] = command_get_type(_current_command);
    _current_sentry = current_ptr;
    switch (command_type) {
    case CommandType::CREATE_DATABASE:
        _do_create_database();
        break;
    case CommandType::DROP_DATABASE:
        _do_drop_database();
        break;
    case CommandType::USE_DATABASE:
        _do_use();
        break;
    case CommandType::CREATE_TABLE:
        _do_create_table();
        break;
    case CommandType::DROP_TABLE:
        _do_drop_table();
        break;
    case CommandType::SELECT:
        _do_select();
        break;
    case CommandType::DELETE:
        _do_delete();
        break;
    case CommandType::INSERT:
        _do_insert();
        break;
    case CommandType::UPDATE:
        _do_update();
        break;
    case CommandType::SYNC:
        _do_sync();
        break;
    case CommandType::QUIT:
        _do_quit();
        break;
    default:
        _state = State::ERROR;
        return;
    }
} catch (IllegalCommandException &e) {
    std::cout << "Encountered illegal command!" << std::endl;
    std::cout << e.what() << std::endl;
    std::cout << "you can run this database program with parameter '--help'"
              << " to see verbose help" << std::endl;
} catch (std::exception &e) {
    std::cout << e.what() << std::endl;
}

} // namespace mygsql