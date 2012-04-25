class CHashElem 
{
	public:
		~CHashElem(){ }
		int Key;
		int Val;
		CHashElem *pNext;
};

class CHash
{
	public:
		CHash(int iSlots = 10);
		~CHash();

	public: 
		bool QueryValue(const int Key,int & Val);
		bool Remove(const int Key);  
		int GetSlotNum();
		int GetLength();
		bool Insert(int Key, int Val);
		CHashElem*  QueryElem(const int iIndex);

	protected:

		CHashElem **m_pHashSlots;
		int m_iNumSlots; 
		int m_iLength;

};


CHash::CHash(int iSlots) 
{
	if (iSlots < 5) iSlots = 5;
	m_pHashSlots = new CHashElem*[iSlots];
	for(int i=0;i<iSlots;i++)
		m_pHashSlots[i]=0;
	m_iNumSlots = iSlots;
	m_iLength=0;
}


CHash::~CHash() 
{
	if (m_pHashSlots)
	{
		CHashElem *phe;
		for (int i=0;i<m_iNumSlots;i++) 
		{
			phe = m_pHashSlots[i];
			while (phe != 0) 
			{
				CHashElem *pNext = phe->pNext;
				delete phe;
				phe = pNext;
			}
		}
		delete m_pHashSlots;
		m_pHashSlots = 0;
	}
}


bool CHash::QueryValue(const int Key,int& Val) 
{
	bool bRet=false;
	unsigned int num=(unsigned int)Key%m_iNumSlots;    
	if (num >= 0)
	{
		CHashElem *pElem = m_pHashSlots[num];
		while (pElem) 
		{
			if (pElem->Key==Key) 
			{
				Val=pElem->Val;
				bRet=true;
			}
			pElem = pElem->pNext;
		}

	}
	return bRet;

}
CHashElem*  CHash::QueryElem(const int iIndex)
{
	CHashElem *pElem=0;
	int iSlot=0;
	pElem=m_pHashSlots[0];
	for(int i=0;i<=iIndex;i++)
	{
BEGIN:
		if(iSlot<m_iNumSlots)
		{            
			if(!pElem)
			{
				iSlot++;
				pElem=m_pHashSlots[iSlot];
				goto BEGIN;
			}
			else
			{
				pElem=pElem->pNext;

			}
		}
		else
		{
			pElem=0;
			break;
		}

	}
	return pElem;

}

bool CHash::Insert(int Key, int Val) 
{
	bool bRet=false;
	unsigned int num=(unsigned int)Key%m_iNumSlots;    
	if (num >= 0)
	{
		if (m_pHashSlots[num]) 
		{
			CHashElem *pIns = m_pHashSlots[num];
			while (pIns->pNext) 
			{
				pIns = pIns->pNext;
			}
			pIns->pNext = new CHashElem;
			pIns->pNext->pNext = 0;
			pIns->pNext->Val = Val;
			pIns->pNext->Key = Key;
			bRet=true;
			m_iLength++;
		} 
		else 
		{
			m_pHashSlots[num] = new CHashElem;
			m_pHashSlots[num]->pNext = 0;
			m_pHashSlots[num]->Key = Key;
			m_pHashSlots[num]->Val = Val;
			bRet=true;
			m_iLength++;
		}
	}
	return bRet;
}



bool CHash::Remove(const int Key) 
{
	bool bRet=false;
	unsigned int num=(unsigned int)Key%m_iNumSlots;    
	if (num >= 0) 
	{
		CHashElem *pElem = m_pHashSlots[num];
		CHashElem *pPrev = 0;
		while (pElem) 
		{
			if (pElem->Key==Key) 
			{
				if (pPrev) 
				{
					pPrev->pNext = pElem->pNext;
				}
				else 
				{
					m_pHashSlots[num] = pElem->pNext;
				}
				delete pElem;
				bRet=true;
				m_iLength--;
				break;
			}
			pPrev = pElem;
			pElem = pElem->pNext;
		}
	}
	return bRet;
}
int CHash::GetLength()
{
	return m_iLength;
}
int CHash::GetSlotNum()
{
	return m_iNumSlots;
} 
