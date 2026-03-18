#include "type_table.h"
#include <algorithm>
#include <iostream>

const TypeTable::AggregateMemberEntry *
TypeTable::AggregateEntry::find(const std::string &name) const
{
    auto it = std::find_if(members.begin(), members.end(),
        [&name](const AggregateMemberEntry &m) -> bool {
            return m.name == name;
        }
    );
    if (it == members.end())
            return nullptr;
    return &*it;
}

void TypeTable::insert(const std::string &tag, const TypeTable::AggregateEntry &entry)
{
    m_map[tag] = entry;
}

const TypeTable::AggregateEntry *TypeTable::get(const std::string &tag) const
{
    auto it = m_map.find(tag);
    if (it != m_map.end())
        return &it->second;
    return nullptr;
}

bool TypeTable::contains(const std::string &tag) const
{
    return m_map.contains(tag);
}

void TypeTable::print() const
{
    for (auto &[tag, entry] : m_map) {
        std::cout << "Aggregate " << tag << "(size: " << entry.size
                  << ", alignment: " << entry.alignment << ") {" << std::endl;
        for (auto &member : entry.members)
            std::cout << "    " << member.name << " - " << member.type << std::endl;
        std::cout << "}" << std::endl;
    }
}
