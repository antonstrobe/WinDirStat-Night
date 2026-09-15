#pragma once

#include <string>
#include <unordered_set>

// Immutable during rendering: never query UI controls from render workers.
template<typename Node>
struct GraphColorSelection
{
    enum class Kind { None, Items, Extensions };
    Kind kind = Kind::None;
    std::unordered_set<const Node*> items;
    std::unordered_set<std::wstring> extensions;

    [[nodiscard]] bool Contains(const Node* item) const
    {
        if (!item) return false;
        if (kind == Kind::Extensions) return extensions.contains(item->GetExtension());
        if (kind != Kind::Items) return false;
        for (const Node* ancestor = item; ancestor; ancestor = ancestor->GetParent())
            if (items.contains(ancestor)) return true;
        return false;
    }
};
