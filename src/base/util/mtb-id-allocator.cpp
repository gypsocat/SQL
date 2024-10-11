#include "mtb-id-allocator.hxx"

namespace MTB {

/* @class IDAllocator 复用已归还ID的ID分配器 */

constexpr auto UNREACHABLE = -1;

IDAllocator::IDAllocator()
    : _cur_max_id(2) {
    _entry_list.push_back({
        UNREACHABLE, UNREACHABLE, false
    }); // [0] = Not Allocated
    _entry_list.push_back({
        UNREACHABLE, UNREACHABLE, true
    }); // [1] = Allocated
}

IDAllocator::IDAllocator(std::initializer_list<bool> allocated_list)
    : IDAllocator() {
    int cur_real_id = 2;
    for (bool item: allocated_list) {
        if (item == true) {
            _entry_list.push_back({1, -1, true});
            _entry_list[1].next = cur_real_id;
            int next_id = _entry_list[cur_real_id].next;
        } else {
            _entry_list.push_back({0, -1, false});
            _entry_list[0].next = cur_real_id;
            int next_id = _entry_list[cur_real_id].next;
        }
    }
    _cur_max_id = cur_real_id;
}

IDAllocator::IDAllocator(bool8vec const &allocated_list)
    : IDAllocator() {
    for (bool item: allocated_list)
        allocate();
    for (int id = 0;
         bool is_allocated: allocated_list) {
        if (!is_allocated)
            free(id);
        id++;
    }
    return;
}

int IDAllocator::allocate()
{
    if (_entry_list[0].next == -1) {
        int current_id = _entry_list.size();
        _entry_list.push_back({0, -1, false});
        _entry_list[0].next = current_id;
        _cur_max_id++;
    }

    /* remove the first "unused" id */
    int ret = _entry_list[0].next;
    int ret_next = _entry_list[ret].next;
    _entry_list[0].next = ret_next;
    if (ret_next != -1)
        _entry_list[ret_next].prev = ret;


    /* and let it join in the "used" list. */
    ret_next = _entry_list[1].next;
    _entry_list[ret] = {1, ret_next, true};
    if (ret_next != -1)
        _entry_list[ret_next].prev = ret;
    _entry_list[1].next = ret;
    
    return ret - 2;
}

bool IDAllocator::isAllocated(int id)
{
    id += 2;
    if (id < 2 || id > _cur_max_id)
        return false;
    return _entry_list[id].allocated;
}

void IDAllocator::free(int id)
{
    id += 2;
    if (isAllocated(id))
        return;
    /* remove the target id */
    int prev = _entry_list[id].prev;
    int next = _entry_list[id].next;
    _entry_list[prev].next = next;
    if (next != -1)
        _entry_list[next].prev = prev;

    /* and let it join in the "unused" list. */
    next = _entry_list[0].next;
    _entry_list[id] = {0, next, false};
    if (next != -1)
        _entry_list[next].prev = UNREACHABLE;
    _entry_list[0].next = id;
}

void IDAllocator::traverseAllocated(IDAllocator::ItemTraverseFunc fn)
{
    for (int i = _entry_list[1].next;
         i != UNREACHABLE;
         i = _entry_list[i].next) {
        fn(i);
    }
}

void IDAllocator::traverseUnallocated(IDAllocator::ItemTraverseFunc fn)
{
    for (int i = _entry_list[0].next;
         i != UNREACHABLE;
         i = _entry_list[i].next) {
        fn(i);
    }
}

} // namespace MTB
