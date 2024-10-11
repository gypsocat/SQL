#include "sql-value.hxx"
#include <compare>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>
#include <unordered_map>

namespace mygsql {

std::string_view ValueTypeGetString(int32_t self)
{
    static std::unordered_map<Value::Type, std::string_view>
    value_string_map = {
        {Value::Type::NONE,   "<undefined>"},
        {Value::Type::INT,    "int"},
        {Value::Type::STRING, "string"}
    };
    Value::Type &&tself = static_cast<Value::Type>(self);
    if (value_string_map.contains(tself))
        return value_string_map.at(tself);
    return "<undefined>";
}

std::string_view ValueTypeGetString(Value::Type self)
{
    return ValueTypeGetString(int32_t(self));
}


std::weak_ordering Value::operator<=>(Value const& another) {
    int64_t comp_result = compare(&another);
    if (comp_result == 0xFFFF'FFFF) {
        throw InconsistantTypeException(
                _value_type, another._value_type);
    }

    if (comp_result == 0)
        return std::weak_ordering::equivalent;
    else if (comp_result < 0)
        return std::weak_ordering::less;
    else
        return std::weak_ordering::greater;
}

bool ValueMeetsCondition(TotalOrderRelation contition, Value *left, Value *right)
{
    if (left->get_value_type() != right->get_value_type()) {
        throw Value::InconsistantTypeException(
            left->get_value_type(),
            right->get_value_type());
    }
    int compare_result = left->compare(right);
    if (((int8_t)contition & (int8_t)TotalOrderRelation::EQ) != 0 &&
        compare_result == 0) {
        return true;
    }
    if (((int8_t)contition & (int8_t)TotalOrderRelation::LT) != 0 &&
        compare_result < 0) {
        return true;
    }
    if (((int8_t)contition & (int8_t)TotalOrderRelation::GT) != 0 &&
        compare_result > 0) {
        return true;
    }
    return false;
}

IntValue::~IntValue() {
    _value = 0;
}

void IntValue::setFromString(std::string_view value)
{
    std::string value_str(value);
    const char *value_cstr = value_str.c_str();
    _value = std::atoi(value_cstr);
}

} // namespace mygsql