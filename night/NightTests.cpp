#include "../windirstat/GraphColorSelection.h"
#include <iostream>

struct Node
{
    const Node* parent = nullptr;
    std::wstring extension;
    const Node* GetParent() const { return parent; }
    const std::wstring& GetExtension() const { return extension; }
};

#define CHECK(condition) do { if (!(condition)) { std::cerr << "FAIL line " << __LINE__ << ": " << #condition << '\n'; return 1; } } while (false)

int main()
{
    const Node root{};
    const Node folder{ &root, L"" };
    const Node nested{ &folder, L"" };
    const Node first{ &folder, L".txt" };
    const Node second{ &root, L".txt" };
    const Node photo{ &nested, L".jpg" };
    const Node noExtension{ &root, L"" };
    GraphColorSelection<Node> selection;
    using Kind = GraphColorSelection<Node>::Kind;

    CHECK(!selection.Contains(&first));
    CHECK(!selection.Contains(nullptr));
    selection.kind = Kind::Items;
    selection.items = { &first };
    CHECK(selection.Contains(&first));
    CHECK(!selection.Contains(&second)); // Selecting a file must not select its whole type.
    CHECK(!selection.Contains(&folder));

    selection.items = { &folder };
    CHECK(selection.Contains(&first));
    CHECK(selection.Contains(&photo)); // Nested descendants of a selected directory.
    CHECK(!selection.Contains(&second));
    CHECK(!selection.Contains(&root));

    selection.items = { &first, &photo };
    CHECK(selection.Contains(&first) && selection.Contains(&photo));
    CHECK(!selection.Contains(&second));
    selection.items.clear();
    CHECK(!selection.Contains(&first)); // Deselecting removes the classic color.

    selection.kind = Kind::Extensions;
    selection.extensions = { L".txt" };
    CHECK(selection.Contains(&first) && selection.Contains(&second));
    CHECK(!selection.Contains(&photo));
    CHECK(!selection.Contains(&noExtension));
    selection.extensions = { L".jpg", L".txt" };
    CHECK(selection.Contains(&photo) && selection.Contains(&first));
    selection.extensions = { L"" };
    CHECK(selection.Contains(&noExtension));
    CHECK(!selection.Contains(&first));

    selection = {};
    CHECK(!selection.Contains(&noExtension)); // No focus differs from the no-extension type.
    CHECK(!selection.Contains(&first));
    std::cout << "PASS: individual files, multiple selection, nested folders, types, grouped types, no extension, and deselection.\n";
    return 0;
}
