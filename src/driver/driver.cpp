/** @file driver.cpp
 * @brief 主函数、驱动程序类的存放区。在这里会初始化 */
#include "base/mtb-object.hxx"
#include "sql-lang/sql-lang-interpreter.hxx"
#include "engine/engine.hxx"
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

using namespace mygsql;
using namespace mygsql::engine;
using namespace std::string_literals;

static std::string help_text =
"MYG-SQL version 0.0.1 下面是帮助:\n"s+
"exit (退出)\n"+
"quit (退出)\n"+
"快捷键`Ctrl+D` (退出)\n"+
"create database <dbname>; (创建数据库)\n"+
"drop database <dbname>; (销毁数据库)\nuse <dbname>; (切换数据库)\n"+
"create table <table-name> (\n    <column> <type>,\n    ...\n"+
"); (创建表，目前只考虑 int 和 string 类型)\n"+
"drop table <table-name> (删除表)\n"+
"select <column> from <table>[where <cond>] (根据条件(如果有)查询表，显示查询结果)\n"+
"delete <table> [where <cond>] (根据条件(如果有)删除表中的记录)\n"+
"insert <table> values (<const-value>,<const-value>, ...)"+
" (在表中插入数据，注意和上面一样，最后一个的右边也没有',')\n"+
"sync (把表中的数据同步到映射缓冲区)\n";

/** @class Driver
 * @brief  驱动类。用于保存运行时的上下文，同时管理输入。 */
class Driver {
public:
    Driver(int argc, char *argv[]) {
        std::filesystem::path argv0path(argv[0]);
        file_dir_name = std::filesystem::absolute(argv0path).parent_path();
        file_dir_name += "/storage";
        for (int i = 0; i < argc; i++) {
            // args.push_back(argv[i]);
            argset.insert(argv[i]);
        }
        if (argset.contains("-h") || argset.contains("--help"))
            state = HELP;
        else
            state = RUN;
        engine      = new Engine(file_dir_name);
        interpreter = new Interpreter(*engine);
    }
    int operator()()
    {
        if (state == HELP) {
            std::cout << help_text << std::endl;
            return 0;
        }
        while (interpreter->get_state() != Interpreter::State::EXIT) {
            if (prompt_input() == false)
                return 0;
            interpreter->run(inputstr);
        }
        return 0;
    }
    /** 显示一个prompt, 然后输入一条命令。 */
    bool prompt_input() {
        std::cout << "> ";
        std::cout.flush();
        std::getline(std::cin, inputstr);
        if (std::cin.eof()) {
            std::cout << std::endl;
            return false;
        }
        return true;
    }
public:
    enum {
        HELP, RUN
    } state;
    MTB::owned<Interpreter> interpreter;
    MTB::owned<Engine>           engine;
    std::string           file_dir_name;
    std::vector<std::string>       args;
    std::multiset<std::string>   argset;
    std::string                inputstr;
}; // class Driver


int main(int argc, char *argv[]){
    Driver driver(argc, argv);
    return driver();
}