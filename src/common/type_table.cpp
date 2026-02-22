#include "type_table.h"

const TypeTable::StructMemberEntry *
TypeTable::StructEntry::find(const std::string &name) const
{
    auto it = std::find_if(members.begin(), members.end(),
        [&name](const StructMemberEntry &m) -> bool {
            return m.name == name;
        }
    );
    if (it == members.end())
            return nullptr;
    return &*it;
}

void TypeTable::insert(const std::string &tag, const TypeTable::StructEntry &entry)
{
    m_map[tag] = entry;
}

const TypeTable::StructEntry *TypeTable::get(const std::string &tag) const
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
