#ifndef __MYG_SQL_VALUE_H__
#define __MYG_SQL_VALUE_H__

#include "base/mtb-exception.hxx"
#include "mtb-object.hxx"
#include <compare>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <format>
#include <functional>
#include <string_view>

namespace mygsql {

class Value;

extern std::string_view ValueTypeGetString(int32_t self);

/** @class Value
 * @brief 全序值类, 要求被继承的类有全序关系 */
imcomplete class Value: public MTB::Object {
public:
    enum class Type: int32_t {
        NONE = -1, INT = 0, STRING
    }; // enum class Type

    /** @class InconsistantTypeException
     * @brief this与参数Value类型不一致，或者左右操作数的类型不一致 */
    class InconsistantTypeException: public MTB::Exception {
    public:
        InconsistantTypeException(Type required_type, Type real_type)
            : MTB::Exception(MTB::ErrorLevel::CRITICAL,
                std::format("Inconsistant Type: requires {}, but got {}",
                        ValueTypeGetString(int32_t(required_type)),
                        ValueTypeGetString(int32_t(real_type)))){}
    };
public:
    Value() = default;
    ~Value() override = default;

    /** @fn setFromString(string_view)
     * @brief 从字符串初始化值 */
    virtual void setFromString(std::string_view value) {}
    /** @fn getString()*/
    virtual std::string getString() const = 0;

    virtual int64_t compare(const Value *another) = 0;
    std::weak_ordering operator<=>(Value const &another);
    virtual size_t hash() const = 0;

    Type get_value_type() const { return _value_type; }
protected:
    Type _value_type;   // 值类型
}; // class Value

extern std::string_view ValueTypeGetString(Value::Type self);

/** @class IntValue
 * @brief 包装的整数值 */
class IntValue final: public Value {
public:
    IntValue(int32_t value = 0)
        : _value(value) { _value_type = Type::INT; }
    IntValue(std::string_view value) {
        _value_type = Type::INT;
        setFromString(value);
    }
    ~IntValue() override;

    int32_t &value()   { return _value; }
    operator int32_t() { return value(); }

    std::string getString() const override {
        return std::format("{}", _value);
    }
    void setFromString(std::string_view value) override;
    size_t hash() const override {
        int value = _value;
        std::hash<int> int_hash;
        return int_hash(value);
    }
    int64_t compare(const Value *another) override {
        if (another->get_value_type() != get_value_type())
            return 0xFFFF'FFFF;
        auto ival = reinterpret_cast<const IntValue*>(another);
        return _value - ival->_value;
    }
private:
    int32_t _value;
}; // class IntValue

class StringValue: public Value {
public:
    StringValue(std::string_view value)
        : _value(value) { _value_type = Type::STRING; }

    size_t hash() const override {
        std::string_view value(_value);
        return std::hash<std::string_view>()(value);
    }
    std::string getString() const override { return _value; }
    int64_t compare(const Value *another) override {
        if (another->get_value_type() != get_value_type())
            return 0xFFFF'FFFF;
        auto sval = reinterpret_cast<const StringValue*>(another);
        return strcmp(_value.c_str(), sval->_value.c_str());
    }
    void setFromString(std::string_view value) override {
        _value = value;
    }
    std::string const &value() const { return _value; }

    /** 转化为普通字符串 */
    operator std::string_view() const& { return value(); }
    operator std::string() && { return _value; }
private:
    std::string _value;
}; // class StringValue

/** @enum 全序关系枚举，是一个二进制掩码. */
enum class TotalOrderRelation: int8_t {
    NONE  = 0b0000, // 不应该使用的值
    LT    = 0b0001, // 小于(第0位)
    GT    = 0b0010, // 大于(第1位)
    EQ    = 0b0100, // 等于(第2位)
    LE    = 0b0101, // 大于等于(LT|EQ)
    GE    = 0b0110, // 小于等于(GT|EQ)
    NE    = 0b0011  // 不等于(LT|GT)
}; // enum class TotalOrderRelation

/** @fn ValueMeetsCondition
 * @brief 判断两个Value是否满足condition所示的相等条件。
 * @warning 注意两个Value的类型是否相等。类型不同的Value比较，会
 *          抛出InconsistantTypeException. */
bool ValueMeetsCondition(TotalOrderRelation contition,
                         Value *left, Value *right);

} // namespace mygsql


namespace std {

template<>
struct hash<mygsql::Value> {
    size_t operator()(mygsql::Value const &value) const {
        return value.hash();
    }
};
template<>
struct hash<mygsql::Value*> {
    size_t operator()(mygsql::Value const *&value) const {
        return value->hash();
    }
};

} // namespace std
#endif
