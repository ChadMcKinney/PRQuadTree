/* Copyright (C) Chad McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <iostream>
#include <memory>
#include <vector>
#include <cstdint>
#include <cassert>
#include <random>

// A Point Region Quadtree
class CQuadTree
{
public:
  typedef uint64_t TScalar;
  class CCoordinate
  {
  public:
    CCoordinate();
    CCoordinate(TScalar _x, TScalar _y);

    inline bool operator==(const CCoordinate& rhs) const;
    inline bool operator!=(const CCoordinate& rhs) const;
    inline CCoordinate operator/(const CCoordinate& rhs) const;
    inline CCoordinate operator+(const CCoordinate& rhs) const;
    inline CCoordinate operator-(const CCoordinate& rhs) const;

    TScalar x, y;
  };

	enum class EInsertResult : uint8_t
	{
		OutOfRegionBounds,
		DuplicateEntry,
		Success
	};

	enum class EFindResult : uint8_t
	{
		OutOfRegionBounds,
		NoEntry,
		Success
	};

  CQuadTree(size_t pageSize);

  EInsertResult Insert(const CCoordinate& point);
  EFindResult Find(const CCoordinate& point);
  void Reset();
  void SanityCheck() const;

private:

  class CBounds
  {
  public:
    CBounds() = default;
    CBounds(const CCoordinate& _min, const CCoordinate& _max);

    inline bool Contains(const CCoordinate& point) const;
    inline bool operator==(const CBounds& rhs) const;
    inline bool operator!=(const CBounds& rhs) const;

    CCoordinate min, max;
  };

  class CNode
  {
  public:
    void InitializeAsLeaf(const CCoordinate& _point, const CBounds& _regionBounds);
    void InitializeAsRegion(const CBounds& _regionBounds);
    EFindResult Find(const CCoordinate& point, CNode** pFoundNode);
    void Split(CQuadTree& quadTree);
    CNode* ContainingSubRegion(const CCoordinate& point);

    enum class EType : uint8_t
    {
      Leaf,
      Region,
      Undefined
    };

    //// Node state
    CBounds m_regionBounds; // The entire region this quad node can contain
    CCoordinate m_point;
    CNode* m_pNorthWest = nullptr;
    CNode* m_pNorthEast = nullptr;
    CNode* m_pSouthEast = nullptr;
    CNode* m_pSouthWest = nullptr;
    EType m_nodeType = EType::Undefined;

    //// Memory pool state
    CNode* pPoolNext = nullptr; // intrusive pointer for pool allocation
  };

  void SanityCheckChild_Recursive(CNode* pChild) const;
  void AllocatePage();
  CNode* AllocateNode();
  CNode* AllocateLeafNode(const CCoordinate& point, const CBounds& regionBounds);
  CNode* AllocateRegionNode(const CBounds& regionBounds);

  //// QuadTree state
  CNode* m_pTreeRoot;

  //// allocator state
  size_t m_pageSize;
  CNode* m_pPoolHead; // head of the linked list of available nodes in the pool
  CNode* m_pPoolRoot; // root node for the pool, allows for fast reset
  typedef std::unique_ptr<CNode[]> TNodePage;
  std::vector<TNodePage> m_pages;
};

//////////////////////////////////////////////////////////////////////////////
// SCoordiante
CQuadTree::CCoordinate::CCoordinate()
  : x(TScalar{})
  , y(TScalar{})
{
}

CQuadTree::CCoordinate::CCoordinate(TScalar _x, TScalar _y)
	: x(_x)
	, y(_y)
{
}

inline bool CQuadTree::CCoordinate::operator==(const CCoordinate & rhs) const
{
	return x == rhs.x && y == rhs.y;
}

inline bool CQuadTree::CCoordinate::operator!=(const CCoordinate & rhs) const
{
	return x != rhs.x || y != rhs.y;
}

inline CQuadTree::CCoordinate CQuadTree::CCoordinate::operator/(const CCoordinate& rhs) const
{
  return CCoordinate(x / rhs.x, y / rhs.y);
}

inline CQuadTree::CCoordinate CQuadTree::CCoordinate::operator+(const CCoordinate& rhs) const
{
  return CCoordinate(x + rhs.x, y + rhs.y);
}

inline CQuadTree::CCoordinate CQuadTree::CCoordinate::operator-(const CCoordinate& rhs) const
{
  return CCoordinate(x - rhs.x, y - rhs.y);
}

//////////////////////////////////////////////////////////////////////////////
// CBounds
CQuadTree::CBounds::CBounds(const CCoordinate& _min, const CCoordinate& _max)
	: min(_min)
	, max(_max)
{
}

inline bool CQuadTree::CBounds::Contains(const CCoordinate& point) const
{
	return point.x >= min.x && point.y >= min.y && point.x <= max.x && point.y <= max.y;
}

inline bool CQuadTree::CBounds::operator==(const CBounds& rhs) const
{
  return min == rhs.min && max == rhs.max;
}

inline bool CQuadTree::CBounds::operator!=(const CBounds& rhs) const
{
  return min != rhs.min || max != rhs.max;
}

//////////////////////////////////////////////////////////////////////////////
// CNode
void CQuadTree::CNode::InitializeAsLeaf(const CCoordinate& _point, const CBounds& _regionBounds)
{
  m_point = _point;
  m_regionBounds = _regionBounds;
  m_nodeType = EType::Leaf;
}

void CQuadTree::CNode::InitializeAsRegion(const CBounds& _regionBounds)
{
  m_regionBounds = _regionBounds;
  m_nodeType = EType::Region;
  assert(m_point == CCoordinate());
}

CQuadTree::EFindResult CQuadTree::CNode::Find(const CCoordinate& point, CNode** pFoundNode)
{
  CNode* pCurrentNode = this;
  while (pCurrentNode->m_pNorthWest != nullptr)
  {
		assert(pCurrentNode->m_pNorthEast && pCurrentNode->m_pSouthEast && pCurrentNode->m_pSouthWest);

		if (pCurrentNode->m_pNorthWest->m_regionBounds.Contains(point))
		{
			pCurrentNode = pCurrentNode->m_pNorthWest;
		}
		else if (pCurrentNode->m_pNorthEast->m_regionBounds.Contains(point))
		{
			pCurrentNode = pCurrentNode->m_pNorthEast;
		}
		else if (pCurrentNode->m_pSouthEast->m_regionBounds.Contains(point))
		{
			pCurrentNode = pCurrentNode->m_pSouthEast;
		}
		else
		{
			assert(pCurrentNode->m_pSouthWest->m_regionBounds.Contains(point));
			pCurrentNode = pCurrentNode->m_pSouthWest;
		}
  }

	*pFoundNode = pCurrentNode;
	if (pCurrentNode->m_nodeType == EType::Region)
	{
		return EFindResult::NoEntry;
	}

	assert(pCurrentNode->m_nodeType == EType::Leaf);
	return pCurrentNode->m_point == point ? EFindResult::Success : EFindResult::NoEntry;
}

void CQuadTree::CNode::Split(CQuadTree& quadTree)
{
	assert(m_pNorthWest == nullptr);
	assert(m_pNorthEast == nullptr);
	assert(m_pSouthEast == nullptr);
	assert(m_pSouthWest == nullptr);

	// Create new four children entries, with the point being in the quadrant it is inside
	const CCoordinate min = m_regionBounds.min;
	const CCoordinate max = m_regionBounds.max;

  CCoordinate centerMin = min + ((max - min) / CCoordinate(2, 2));
  CCoordinate centerMax = centerMin + CCoordinate(1, 1);

	CBounds northWestBounds(min, centerMin);
	CBounds northEastBounds(CCoordinate(centerMax.x, min.y), CCoordinate(max.x, centerMin.y));
	CBounds southEastBounds(centerMax, max);
	CBounds southWestBounds(CCoordinate(min.x, centerMax.y), CCoordinate(centerMin.x, max.y));

	m_pNorthWest = quadTree.AllocateRegionNode(northWestBounds);
	m_pNorthEast = quadTree.AllocateRegionNode(northEastBounds);
	m_pSouthEast = quadTree.AllocateRegionNode(southEastBounds);
	m_pSouthWest = quadTree.AllocateRegionNode(southWestBounds);

	assert(m_pNorthWest != nullptr);
	assert(m_pNorthEast != nullptr);
	assert(m_pSouthEast != nullptr);
	assert(m_pSouthWest != nullptr);

  m_nodeType = EType::Region;
  m_point = CCoordinate();
}

CQuadTree::CNode* CQuadTree::CNode::ContainingSubRegion(const CCoordinate& point)
{
  if (!m_regionBounds.Contains(point))
    return nullptr;

  if (m_pNorthWest)
  {
    assert(m_nodeType == EType::Region);
		assert(m_pNorthEast != nullptr);
		assert(m_pSouthEast != nullptr);
		assert(m_pSouthWest != nullptr);

		if (m_pNorthWest->m_regionBounds.Contains(point))
		{
			return m_pNorthWest;
		}
		else if (m_pNorthEast->m_regionBounds.Contains(point))
		{
			return m_pNorthEast;
		}
		else if (m_pSouthEast->m_regionBounds.Contains(point))
		{
			return m_pSouthEast;
		}
		else
		{
			assert(m_pSouthWest->m_regionBounds.Contains(point));
			return m_pSouthWest;
		}
  }

  return nullptr;
}

//////////////////////////////////////////////////////////////////////////////
// CQuadTree
CQuadTree::CQuadTree::CQuadTree(size_t pageSize)
  : m_pageSize(pageSize)
  , m_pPoolHead(nullptr)
  , m_pPoolRoot(nullptr)
{
  assert(pageSize > 0);
  m_pages.reserve(8);
  Reset();
}

CQuadTree::EInsertResult CQuadTree::Insert(const CCoordinate& point)
{
	assert(m_pTreeRoot != nullptr);

	CNode* pFoundNode = nullptr;
  assert(m_pTreeRoot->m_regionBounds.Contains(point));
  EFindResult findResult = m_pTreeRoot->Find(point, &pFoundNode);
	assert(pFoundNode != nullptr);
	if (findResult == EFindResult::Success)
	{
		assert(pFoundNode->m_point == point);
		return EInsertResult::DuplicateEntry;
	}
	else
	{
    if (pFoundNode->m_nodeType == CNode::EType::Leaf)
    {
      // We expect either an empty region or a leaf, neither of which should have children
			assert(pFoundNode->m_pNorthWest == nullptr);
			assert(pFoundNode->m_pNorthEast == nullptr);
			assert(pFoundNode->m_pSouthEast == nullptr);
			assert(pFoundNode->m_pSouthWest == nullptr);

      // Split recursively until point and pFoundNode->m_point are in different quandrants
      CCoordinate existingPoint = pFoundNode->m_point;
      CNode* pExistingSubRegion = pFoundNode;
      CNode* pSubRegion = pFoundNode;

      do
      {
				pSubRegion->Split(*this);
				pExistingSubRegion = pSubRegion->ContainingSubRegion(existingPoint);
				pSubRegion = pSubRegion->ContainingSubRegion(point);
        assert(pExistingSubRegion != nullptr && pSubRegion != nullptr);
      } while (pExistingSubRegion == pSubRegion);


      assert(pExistingSubRegion != nullptr);
			pExistingSubRegion->m_nodeType = CNode::EType::Leaf;
			pExistingSubRegion->m_point = existingPoint;

      assert(pSubRegion != nullptr);
			pSubRegion->m_nodeType = CNode::EType::Leaf;
			pSubRegion->m_point = point;
    }
    else
    {
      // If it isn't a leaf we expect a region with no children (null entry)
      assert(pFoundNode->m_nodeType == CNode::EType::Region);
      // Change to a leaf and set point
      pFoundNode->m_nodeType = CNode::EType::Leaf;
      pFoundNode->m_point = point;
    }
	}

	return EInsertResult::Success;
}

CQuadTree::EFindResult CQuadTree::Find(const CCoordinate& point)
{
  assert(m_pTreeRoot != nullptr);
	CNode* pFoundNode = nullptr;
  assert(m_pTreeRoot->m_regionBounds.Contains(point));
	return m_pTreeRoot->Find(point, &pFoundNode);
}

void CQuadTree::Reset()
{
  m_pPoolHead = m_pPoolRoot;
  constexpr TScalar minValue = std::numeric_limits<TScalar>::min();
  constexpr TScalar maxValue = std::numeric_limits<TScalar>::max();
  m_pTreeRoot = AllocateRegionNode(CBounds(CCoordinate(minValue, minValue), CCoordinate(maxValue, maxValue)));
}

void CQuadTree::SanityCheck() const
{
  SanityCheckChild_Recursive(m_pTreeRoot);
}

void CQuadTree::SanityCheckChild_Recursive(CNode* pChild) const
{
  assert(pChild);
  assert(pChild->m_regionBounds != CBounds(CCoordinate(), CCoordinate()));
  assert(pChild->m_regionBounds.max.x >= pChild->m_regionBounds.min.x);
  assert(pChild->m_regionBounds.max.y >= pChild->m_regionBounds.min.y);
  assert(pChild->m_regionBounds.min.x <= pChild->m_regionBounds.max.x);
  assert(pChild->m_regionBounds.min.y <= pChild->m_regionBounds.max.y);
  switch (pChild->m_nodeType)
  {
  case CNode::EType::Leaf:
    assert(pChild->m_pNorthWest == nullptr);
    assert(pChild->m_pNorthEast == nullptr);
    assert(pChild->m_pSouthEast == nullptr);
    assert(pChild->m_pSouthWest == nullptr);
    assert(pChild->m_regionBounds.Contains(pChild->m_point));
    break;
  case CNode::EType::Region:
    assert(pChild->m_point == CCoordinate(0, 0));
    if (pChild->m_pNorthWest)
    {
			assert(pChild->m_pNorthWest != nullptr);
			assert(pChild->m_pNorthEast != nullptr);
			assert(pChild->m_pSouthEast != nullptr);
			assert(pChild->m_pSouthWest != nullptr);

			assert(pChild->m_regionBounds.Contains(pChild->m_pNorthWest->m_regionBounds.min));
			assert(pChild->m_regionBounds.Contains(pChild->m_pNorthWest->m_regionBounds.max));
			assert(pChild->m_regionBounds.Contains(pChild->m_pNorthEast->m_regionBounds.min));
			assert(pChild->m_regionBounds.Contains(pChild->m_pNorthEast->m_regionBounds.max));
			assert(pChild->m_regionBounds.Contains(pChild->m_pSouthEast->m_regionBounds.min));
			assert(pChild->m_regionBounds.Contains(pChild->m_pSouthEast->m_regionBounds.max));
			assert(pChild->m_regionBounds.Contains(pChild->m_pSouthWest->m_regionBounds.min));
			assert(pChild->m_regionBounds.Contains(pChild->m_pSouthWest->m_regionBounds.max));

      assert(pChild->m_pNorthWest->m_regionBounds.min.x <  pChild->m_pNorthEast->m_regionBounds.min.x);
      assert(pChild->m_pNorthWest->m_regionBounds.min.y == pChild->m_pNorthEast->m_regionBounds.min.y);
      assert(pChild->m_pNorthWest->m_regionBounds.max.x <  pChild->m_pNorthEast->m_regionBounds.min.x);
      assert(pChild->m_pNorthWest->m_regionBounds.max.x <  pChild->m_pNorthEast->m_regionBounds.max.x);
      assert(pChild->m_pNorthWest->m_regionBounds.min.y <  pChild->m_pNorthEast->m_regionBounds.max.y);
      assert(pChild->m_pNorthWest->m_regionBounds.max.y == pChild->m_pNorthEast->m_regionBounds.max.y);

      assert(pChild->m_pNorthEast->m_regionBounds.min.x == pChild->m_pSouthEast->m_regionBounds.min.x);
      assert(pChild->m_pNorthEast->m_regionBounds.min.y <  pChild->m_pSouthEast->m_regionBounds.min.y);
      assert(pChild->m_pNorthEast->m_regionBounds.max.x >  pChild->m_pSouthEast->m_regionBounds.min.x);
      assert(pChild->m_pNorthEast->m_regionBounds.max.x == pChild->m_pSouthEast->m_regionBounds.max.x);
      assert(pChild->m_pNorthEast->m_regionBounds.min.y <  pChild->m_pSouthEast->m_regionBounds.max.y);
      assert(pChild->m_pNorthEast->m_regionBounds.max.y <  pChild->m_pSouthEast->m_regionBounds.max.y);

      assert(pChild->m_pSouthEast->m_regionBounds.min.x >  pChild->m_pSouthWest->m_regionBounds.min.x);
      assert(pChild->m_pSouthEast->m_regionBounds.min.y == pChild->m_pSouthWest->m_regionBounds.min.y);
      assert(pChild->m_pSouthEast->m_regionBounds.max.x >  pChild->m_pSouthWest->m_regionBounds.min.x);
      assert(pChild->m_pSouthEast->m_regionBounds.max.x >  pChild->m_pSouthWest->m_regionBounds.max.x);
      assert(pChild->m_pSouthEast->m_regionBounds.min.y <  pChild->m_pSouthWest->m_regionBounds.max.y);
      assert(pChild->m_pSouthEast->m_regionBounds.max.y == pChild->m_pSouthWest->m_regionBounds.max.y);

      assert(pChild->m_pSouthWest->m_regionBounds.min.x == pChild->m_pNorthWest->m_regionBounds.min.x);
      assert(pChild->m_pSouthWest->m_regionBounds.min.y >  pChild->m_pNorthWest->m_regionBounds.min.y);
      assert(pChild->m_pSouthWest->m_regionBounds.max.x >  pChild->m_pNorthWest->m_regionBounds.min.x);
      assert(pChild->m_pSouthWest->m_regionBounds.max.x == pChild->m_pNorthWest->m_regionBounds.max.x);
      assert(pChild->m_pSouthWest->m_regionBounds.min.y >  pChild->m_pNorthWest->m_regionBounds.max.y);
      assert(pChild->m_pSouthWest->m_regionBounds.max.y >  pChild->m_pNorthWest->m_regionBounds.max.y);

      if (pChild->m_pNorthWest->m_nodeType == CNode::EType::Leaf)
      {
        assert(pChild->m_regionBounds.Contains(pChild->m_pNorthWest->m_point));
      }

      SanityCheckChild_Recursive(pChild->m_pNorthWest);

      if (pChild->m_pNorthEast->m_nodeType == CNode::EType::Leaf)
      {
        assert(pChild->m_regionBounds.Contains(pChild->m_pNorthEast->m_point));
      }
      
      SanityCheckChild_Recursive(pChild->m_pNorthEast);

      if (pChild->m_pSouthEast->m_nodeType == CNode::EType::Leaf)
      {
        assert(pChild->m_regionBounds.Contains(pChild->m_pSouthEast->m_point));
      }
      SanityCheckChild_Recursive(pChild->m_pSouthEast);

      if (pChild->m_pSouthWest->m_nodeType == CNode::EType::Leaf)
      {
        assert(pChild->m_regionBounds.Contains(pChild->m_pSouthWest->m_point));
      }

      SanityCheckChild_Recursive(pChild->m_pSouthWest);
    }
    else
    {
			assert(pChild->m_pNorthWest == nullptr);
			assert(pChild->m_pNorthEast == nullptr);
			assert(pChild->m_pSouthEast == nullptr);
			assert(pChild->m_pSouthWest == nullptr);
    }
    break;
  case CNode::EType::Undefined:
    assert(false);
    break;
  default:
    assert(false);
    break;
  }
}

void CQuadTree::AllocatePage()
{
  assert(m_pageSize > 0);
  m_pages.emplace_back(new CNode[m_pageSize]());
  CNode* pPages = m_pages.back().get();
  size_t lastIndex = m_pageSize - 1;
  for (size_t i = 0; i < lastIndex; ++i)
  {
    CNode* pCurrentPage = &pPages[i];
    pCurrentPage->pPoolNext = pCurrentPage + 1;
  }

  pPages[lastIndex].pPoolNext = nullptr;

  if (m_pPoolRoot == nullptr)
  {
    assert(m_pPoolHead == nullptr);
    m_pPoolRoot = m_pPoolHead = pPages;
  }
  else if(m_pPoolHead != nullptr)
  {
		m_pPoolHead->pPoolNext = pPages;
  }
  else
  {
    assert(m_pPoolHead != nullptr);
  }
}

CQuadTree::CNode* CQuadTree::AllocateNode()
{
  if (m_pPoolHead == nullptr || m_pPoolHead->pPoolNext == nullptr)
  {
    AllocatePage();
  }

  assert(m_pPoolHead != nullptr);
  assert(m_pPoolHead->pPoolNext != nullptr);
  CNode* pAllocatedNode = m_pPoolHead;
  CNode* pPoolNext = pAllocatedNode->pPoolNext;
  *pAllocatedNode = CNode();
  pAllocatedNode->pPoolNext = pPoolNext;
  m_pPoolHead = pPoolNext;
  return pAllocatedNode;
}

CQuadTree::CNode* CQuadTree::AllocateLeafNode(const CCoordinate& point, const CBounds& regionBounds)
{
  CNode* pLeafNode = AllocateNode();
  assert(pLeafNode != nullptr);
  pLeafNode->InitializeAsLeaf(point, regionBounds);
  return pLeafNode;
}

CQuadTree::CNode* CQuadTree::AllocateRegionNode(const CBounds& regionBounds)
{
  CNode* pLeafNode = AllocateNode();
  assert(pLeafNode != nullptr);
  pLeafNode->InitializeAsRegion(regionBounds);
  return pLeafNode;
}

//////////////////////////////////////////////////////////////////////////////
// main
int main()
{
  constexpr CQuadTree::TScalar min = std::numeric_limits<CQuadTree::TScalar>::min();
  constexpr CQuadTree::TScalar max = std::numeric_limits<CQuadTree::TScalar>::max();
	std::default_random_engine generator;
	std::uniform_int_distribution<CQuadTree::TScalar> distribution(min, max);
	const size_t pageSize = 32768;
	CQuadTree quadTree(pageSize);
  for (size_t i = 1; i < 8192; ++i)
  {
    quadTree.Reset();
		for (size_t j = 0; j < i; ++j)
		{
      quadTree.Insert(CQuadTree::CCoordinate(distribution(generator), distribution(generator)));
		}
    quadTree.SanityCheck();
  }

  return 0;
}
