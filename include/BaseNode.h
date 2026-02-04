#pragma once

#include <functional>
#include <string>
#include <vector>

#include "Expected.h"
#include "Handle.h"
#include "INode.h"
#include "Uuid.h"

namespace SnAPI::GameFramework
{

class NodeGraph;

class BaseNode : public INode
{
public:
    static constexpr const char* kTypeName = "SnAPI::GameFramework::BaseNode";

    BaseNode()
        : m_typeId(TypeIdFromName(kTypeName))
    {
    }
    explicit BaseNode(std::string InName)
        : m_name(std::move(InName))
        , m_typeId(TypeIdFromName(kTypeName))
    {
    }

    const std::string& Name() const override
    {
        return m_name;
    }

    void Name(std::string Name) override
    {
        m_name = std::move(Name);
    }

    NodeHandle Handle() const override
    {
        return m_self;
    }

    void Handle(NodeHandle Handle)
    {
        m_self = Handle;
    }

    const Uuid& Id() const
    {
        return m_self.Id;
    }

    void Id(Uuid Id)
    {
        m_self = NodeHandle(std::move(Id));
    }

    const TypeId& TypeKey() const
    {
        return m_typeId;
    }

    void TypeKey(const TypeId& Id)
    {
        m_typeId = Id;
    }

    NodeHandle Parent() const
    {
        return m_parent;
    }

    void Parent(NodeHandle Parent)
    {
        m_parent = Parent;
    }

    const std::vector<NodeHandle>& Children() const
    {
        return m_children;
    }

    void AddChild(NodeHandle Child)
    {
        m_children.push_back(Child);
    }

    void RemoveChild(NodeHandle Child)
    {
        for (auto It = m_children.begin(); It != m_children.end(); ++It)
        {
            if (*It == Child)
            {
                m_children.erase(It);
                return;
            }
        }
    }

    bool Active() const
    {
        return m_active;
    }

    void Active(bool Active)
    {
        m_active = Active;
    }

    std::vector<TypeId>& ComponentTypes()
    {
        return m_componentTypes;
    }

    const std::vector<TypeId>& ComponentTypes() const
    {
        return m_componentTypes;
    }

    std::vector<uint64_t>& ComponentMask()
    {
        return m_componentMask;
    }

    const std::vector<uint64_t>& ComponentMask() const
    {
        return m_componentMask;
    }

    uint32_t MaskVersion() const
    {
        return m_maskVersion;
    }

    void MaskVersion(uint32_t Version)
    {
        m_maskVersion = Version;
    }

    NodeGraph* OwnerGraph() const
    {
        return m_ownerGraph;
    }

    void OwnerGraph(NodeGraph* Graph)
    {
        m_ownerGraph = Graph;
    }

    template<typename T, typename... Args>
    TExpectedRef<T> Add(Args&&... args);

    template<typename T>
    TExpectedRef<T> Component();

    template<typename T>
    bool Has() const;

    template<typename T>
    void Remove();

    void TickTree(float DeltaSeconds);
    void FixedTickTree(float DeltaSeconds);
    void LateTickTree(float DeltaSeconds);

private:
    NodeHandle m_self{};
    NodeHandle m_parent{};
    std::vector<NodeHandle> m_children{};
    std::string m_name{"Node"};
    bool m_active = true;
    std::vector<TypeId> m_componentTypes{};
    std::vector<uint64_t> m_componentMask{};
    uint32_t m_maskVersion = 0;
    NodeGraph* m_ownerGraph = nullptr;
    TypeId m_typeId{};
};

} // namespace SnAPI::GameFramework
