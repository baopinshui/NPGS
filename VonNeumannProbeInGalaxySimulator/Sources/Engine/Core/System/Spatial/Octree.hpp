#pragma once

#include <cmath>
#include <array>
#include <functional>
#include <future>
#include <memory>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include "Engine/Core/Base/Base.h"
#include "Engine/Core/Runtime/Threads/ThreadPool.h"

_NPGS_BEGIN
_SYSTEM_BEGIN
_SPATIAL_BEGIN

template <typename LinkTarget>
class TOctreeNode
{
public:
    TOctreeNode(const glm::vec3& Center, float Radius, TOctreeNode* Previous)
        : _Center(Center), _Previous(Previous), _Radius(Radius), _bIsValid(true)
    {
    }

    bool Contains(const glm::vec3& Point) const
    {
        return (Point.x >= _Center.x - _Radius && Point.x <= _Center.x + _Radius &&
                Point.y >= _Center.y - _Radius && Point.y <= _Center.y + _Radius &&
                Point.z >= _Center.z - _Radius && Point.z <= _Center.z + _Radius);
    }

    int CalculateOctant(const glm::vec3& Point) const
    {
        int Octant = 0;

        if (Point.x >= _Center.x) Octant |= 4;
        if (Point.y >= _Center.y) Octant |= 2;
        if (Point.z >= _Center.z) Octant |= 1;

        return Octant;
    }

    bool IntersectSphere(const glm::vec3& Point, float Radius) const
    {
        glm::vec3 MinBound = _Center - glm::vec3(_Radius);
        glm::vec3 MaxBound = _Center + glm::vec3(_Radius);

        glm::vec3 ClosestPoint = glm::clamp(Point, MinBound, MaxBound);
        float Distance = glm::distance(Point, ClosestPoint);

        return Distance <= Radius;
    }

    const bool GetValidation() const
    {
        return _bIsValid;
    }

    const glm::vec3& GetCenter() const
    {
        return _Center;
    }

    const TOctreeNode* GetPrevious() const
    {
        return _Previous;
    }

    TOctreeNode* GetPrevious()
    {
        return _Previous;
    }

    float GetRadius() const
    {
        return _Radius;
    }

    std::unique_ptr<TOctreeNode>& GetNext(int Index)
    {
        return _Next[Index];
    }

    const std::unique_ptr<TOctreeNode>& GetNext(int Index) const
    {
        return _Next[Index];
    }

    void AddPoint(const glm::vec3& Point)
    {
        _Points.emplace_back(Point);
    }

    void DeletePoint(const glm::vec3& Point)
    {
        auto it = std::find(_Points.begin(), _Points.end(), Point);
        if (it != _Points.end())
        {
            _Points.erase(it);
        }
    }

    void RemoveStorage()
    {
        _Points.clear();
    }

    void AddLink(LinkTarget* Target)
    {
        _DataLink.emplace_back(Target);
    }

    template <typename Func>
    LinkTarget* GetLink(Func&& Pred) const
    {
        for (LinkTarget* Target : _DataLink)
        {
            if (Pred(Target))
            {
                return Target;
            }
        }

        return nullptr;
    }

    void RemoveLinks()
    {
        _DataLink.clear();
    }

    std::vector<glm::vec3>& GetPoints()
    {
        return _Points;
    }

    const std::vector<glm::vec3>& GetPoints() const
    {
        return _Points;
    }

    void SetValidation(bool bValidation)
    {
        _bIsValid = bValidation;
    }

    bool IsLeafNode() const
    {
        for (const auto& Next : _Next)
        {
            if (Next != nullptr)
            {
                return false;
            }
        }

        return true;
    }

private:
    glm::vec3    _Center;
    TOctreeNode* _Previous;
    float        _Radius;
    bool         _bIsValid;

    std::array<std::unique_ptr<TOctreeNode>, 8> _Next;
    std::vector<glm::vec3>   _Points;
    std::vector<LinkTarget*> _DataLink;
};

template <typename LinkTarget>
class TOctree
{
public:
    using FNodeType = TOctreeNode<LinkTarget>;

public:
    TOctree(const glm::vec3& Center, float Radius, int MaxDepth = 8)
        :
        _Root(std::make_unique<FNodeType>(Center, Radius, nullptr)),
        _ThreadPool(Runtime::Thread::FThreadPool::GetInstance()),
        _MaxDepth(MaxDepth)
    {
    }

    void BuildEmptyTree(float LeafRadius)
    {
        int Depth = static_cast<int>(std::ceil(std::log2(_Root->GetRadius() / LeafRadius)));
        BuildEmptyTreeImpl(_Root.get(), LeafRadius, Depth);
    }

    void Insert(const glm::vec3& Point)
    {
        InsertImpl(_Root.get(), Point, 0);
    }

    void Delete(const glm::vec3& Point)
    {
        DeleteImpl(_Root.get(), Point);
    }

    void Query(const glm::vec3& Point, float Radius, std::vector<glm::vec3>& Results) const
    {
        QueryImpl(_Root.get(), Point, Radius, Results);
    }

    template <typename Func = std::function<bool(const FNodeType&)>>
    FNodeType* Find(const glm::vec3& Point, Func&& Pred = [](const FNodeType&) -> bool { return true; }) const
    {
        return FindImpl(_Root.get(), Point, std::forward<Func>(Pred));
    }

    template <typename Func>
    void Traverse(Func&& Pred) const
    {
        TraverseImpl(_Root.get(), std::forward<Func>(Pred));
    }

    std::size_t GetCapacity() const
    {
        return GetCapacityImpl(_Root.get());
    }

    std::size_t GetSize() const
    {
        return GetSizeImpl(_Root.get());
    }

    const FNodeType* const GetRoot() const
    {
        return _Root.get();
    }

private:
    void BuildEmptyTreeImpl(FNodeType* Node, float LeafRadius, int Depth)
    {
        if (Node->GetRadius() <= LeafRadius || Depth == 0)
        {
            return;
        }

        std::vector<std::future<void>> Futures;
        float NextRadius = Node->GetRadius() * 0.5f;
        for (int i = 0; i != 8; ++i)
        {
            glm::vec3 Offset((i & 1 ? 1 : -1) * NextRadius,
                             (i & 2 ? 1 : -1) * NextRadius,
                             (i & 4 ? 1 : -1) * NextRadius);

            Node->GetNext(i) = std::make_unique<FNodeType>(Node->GetCenter() + Offset, NextRadius, Node);
            if (Depth == static_cast<int>(std::ceil(std::log2(_Root->GetRadius() / LeafRadius))))
            {
                Futures.emplace_back(_ThreadPool->Submit(
                    &TOctree::BuildEmptyTreeImpl, this, Node->GetNext(i).get(), LeafRadius, Depth - 1));
            }
            else
            {
                BuildEmptyTreeImpl(Node->GetNext(i).get(), LeafRadius, Depth - 1);
            }
        }

        for (auto& Future : Futures)
        {
            Future.get();
        }
    }

    void InsertImpl(FNodeType* Node, const glm::vec3& Point, int Depth)
    {
        if (!Node->Contains(Point) || Depth > _MaxDepth)
        {
            return;
        }

        if (Node->GetNext(0) == nullptr)
        {
            for (int i = 0; i != 8; ++i)
            {
                glm::vec3 NewCenter = Node->GetCenter();
                float Radius = Node->GetRadius();
                NewCenter.x += (i & 4) ? Radius * 0.5f : -Radius * 0.5f;
                NewCenter.y += (i & 2) ? Radius * 0.5f : -Radius * 0.5f;
                NewCenter.z += (i & 1) ? Radius * 0.5f : -Radius * 0.5f;
                Node->GetNext(i) = std::make_unique<FNodeType>(NewCenter, Radius * 0.5f, Node);
            }
        }

        int Octant = Node->CalculateOctant(Point);
        if (Depth == _MaxDepth)
        {
            Node->AddPoint(Point);
        }
        else
        {
            InsertImpl(Node->GetNext(Octant).get(), Point, Depth + 1);
        }
    }

    void DeleteImpl(FNodeType* Node, const glm::vec3& Point)
    {
        if (Node == nullptr)
        {
            return;
        }

        if (Node->Contains(Point))
        {
            if (Node->IsLeafNode())
            {
                auto& Points = Node->GetPoints();
                auto it = std::find(Points.begin(), Points.end(), Point);
                if (it != Points.end())
                {
                    Points.erase(it);
                }
            }
            else
            {
                int Octant = Node->CalculateOctant(Point);
                DeleteImpl(Node->GetNext(Octant).get(), Point);
            }

            if (Node->IsLeafNode() && Node->GetPoints().empty())
            {
                for (int i = 0; i != 8; ++i)
                {
                    auto& Next = Node->GetNext(i);
                    Next.reset();
                }
            }
        }
    }

    void QueryImpl(FNodeType* Node, const glm::vec3& Point, float Radius, std::vector<glm::vec3>& Results) const
    {
        if (Node == nullptr || Node->GetNext(0) == nullptr)
        {
            return;
        }

        for (const auto& StoredPoint : Node->GetPoints())
        {
            if (glm::distance(StoredPoint, Point) <= Radius && StoredPoint != Point)
            {
                Results.emplace_back(StoredPoint);
            }
        }

        for (int i = 0; i != 8; ++i)
        {
            FNodeType* NextNode = Node->GetNext(i).get();
            if (NextNode != nullptr && NextNode->IntersectSphere(Point, Radius))
            {
                QueryImpl(NextNode, Point, Radius, Results);
            }
        }
    }

    template <typename Func>
    FNodeType* FindImpl(FNodeType* Node, const glm::vec3& Point, Func&& Pred) const
    {
        if (Node == nullptr)
        {
            return nullptr;
        }

        if (Node->Contains(Point))
        {
            if (Pred(*Node))
            {
                return Node;
            }
        }

        for (int i = 0; i != 8; ++i)
        {
            FNodeType* ResultNode = FindImpl(Node->GetNext(i).get(), Point, Pred);
            if (ResultNode != nullptr)
            {
                return ResultNode;
            }
        }

        return nullptr;
    }

    template <typename Func>
    void TraverseImpl(FNodeType* Node, Func&& Pred) const
    {
        if (Node == nullptr)
        {
            return;
        }

        Pred(*Node);

        for (int i = 0; i != 8; ++i)
        {
            TraverseImpl(Node->GetNext(i).get(), Pred);
        }
    }

    std::size_t GetCapacityImpl(const FNodeType* Node) const
    {
        if (Node == nullptr)
        {
            return 0;
        }

        if (Node->GetNext(0) == nullptr)
        {
            return Node->GetValidation() ? 1 : 0;
        }

        std::size_t Capacity = 0;
        for (int i = 0; i != 8; ++i)
        {
            Capacity += GetCapacityImpl(Node->GetNext(i).get());
        }

        return Capacity;
    }

    std::size_t GetSizeImpl(const FNodeType* Node) const
    {
        if (Node == nullptr)
        {
            return 0;
        }

        std::size_t Size = Node->GetPoints().size();
        for (int i = 0; i != 8; ++i)
        {
            Size += GetSizeImpl(Node->GetNext(i).get());
        }

        return Size;
    }

private:
    std::unique_ptr<FNodeType>    _Root;
    Runtime::Thread::FThreadPool* _ThreadPool;
    int                           _MaxDepth;
};

_SPATIAL_END
_SYSTEM_END
_NPGS_END
