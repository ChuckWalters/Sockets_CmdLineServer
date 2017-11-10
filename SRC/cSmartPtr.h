/** \file
 *
 * Smart Point Template class with "Node" container template and STL Map framework
*/

#ifndef cSmartPtr_H
#define cSmartPtr_H

#if defined(WIN32)
#include <windows.h>
#else
#include <synch.h>
#include <pthread.h>
#include <thread.h>
#endif

#include <map>
#include <queue>

//============================================================================
// <summary>
// This class encapsulates another class derived from cRefCount.
// <use>
// Recomended not using this class directly, rather create cNode<yourClass>
// objects.
//============================================================================
template<class T>
class cSmartPtr
{
public:
	cSmartPtr(T* ptr = 0)
		:m_Ptr(ptr)
	{
		init();
	}
	cSmartPtr(const cSmartPtr& rhs)
		:m_Ptr(rhs.m_Ptr)
	{
		init();
	}
	~cSmartPtr()
	{
		if (m_Ptr)
			m_Ptr->decRef();
	}

	cSmartPtr&
    operator=(const cSmartPtr& rhs)
	{
		if (m_Ptr != rhs.m_Ptr)
		{
			if (m_Ptr)
				m_Ptr->decRef();
			m_Ptr = rhs.m_Ptr;
			init();
		}
		return(*this);
	}

	T*
    operator->()const
	{
		return(m_Ptr);
	}
	T&
    operator*()const
	{
		return(*m_Ptr);
	}

private:
	T*	
		m_Ptr;

	void
    init()
	{
		if (m_Ptr==NULL)
			return;
		m_Ptr->incRef();
	}
};

//============================================================================
//  <summary> 
//  Tracks number of references currently held to this object.
//  Destructs itself when the object loses it's last reference.
//  <use>
//  This object should only be used by cNode
//============================================================================
class cRefCount
{
public:
	void 
	incRef(void)
	{
		InterlockedIncrement((long*)&m_Count);
		//std::cerr<<"cRefCount inc, post inc cnt = "<<m_Count<<std::endl;
	}
	void 
	decRef(void)
	{
		if (InterlockedDecrement ((long*) &m_Count) == 0)
		{
			//std::cerr<<"cRefCount delete this"<<std::endl;
			delete this;
		}
	}

protected:
	/// Constructors & Destructors not publicly visible

	cRefCount(void)
		: m_Count(0)
	{
		//std::cerr<<"cRefCount constructor"<<std::endl;
	}

	cRefCount(const cRefCount& rhs)
	: m_Count(0)
	{
		//std::cerr<<"cRefCount copy constructor"<<std::endl;
	}

	cRefCount& operator=(const cRefCount& rhs)
	{
		//std::cerr<<"cRefCount = op"<<std::endl;
		return(*this);
	}
	virtual 
	~cRefCount(void)
	{
		//std::cerr<<"~cRefCount"<<std::endl;
	}

private:
	volatile long
		m_Count;
};

///============================================================================
/// Holds an instance of a reference counting smart pointer
/// Sample Declaration:
///     typedef cNode<cMyClass> cMyClassNode;
//
/// see cMap below for use with the STL
///============================================================================
template <class T_NodeData>
class cNode
{
private:
	struct sNode_Data:public cRefCount
	{
		T_NodeData 
			*mData;
		void
		init(T_NodeData * data)
		{ mData = data;}
    
		sNode_Data(T_NodeData *data)
		{ init(data);}
    
		sNode_Data(const sNode_Data& rhs)
		{ init(rhs.mData);}
    
		~sNode_Data()
		{ delete mData;}
	};

	cSmartPtr<sNode_Data> 
		m_SmartPtr;

public:
	cNode(T_NodeData *data)
		:m_SmartPtr(new sNode_Data(data))
	{}
	cNode (void)
		:m_SmartPtr()
	{}

	T_NodeData * 
    getData (void)
		{ return(m_SmartPtr->mData);}

	T_NodeData *
    operator->()const
	{
		return(m_SmartPtr->mData);
	}

};

///============================================================================
/// Encapsulate critical sections
/// Member methods are virtual so child classes can override to either make
//  a Unix crit section or provide empty methods to disable critical sections.
///============================================================================
class cCriticalSection
{
public:
	cCriticalSection()
	{
		InitLock();
	}
	virtual void
    Lock(void)
	{
		EnterCriticalSection(&mMutex);
	}
	virtual void
    UnLock(void)
	{
		LeaveCriticalSection(&mMutex);
	}
protected:
	virtual void
    InitLock(void)
	{
		InitializeCriticalSection(&mMutex);
	}
	virtual void
	DeleteLock(void)
	{
		DeleteCriticalSection(&mMutex);
	}
private:
	CRITICAL_SECTION  
		mMutex;
};
///============================================================================
/// Sample Declaration:
///     typedef cNode<cMyClass> cMyClassNode;
///
///		cMap<int,cMyClassNode> myMap;
///
/// Sample Use:
///     cMyClass * myClass = new cMyClass(); // any class of yours
///		cMyClassNode node(myClass);
///		myMap.Add(&node);
///============================================================================
template <class T_Key, class T_NodeData>
class cMap : 
	public cCriticalSection
{
public:
	cMap(void):cCriticalSection()
	{
	}
	virtual
    ~cMap(void)
	{
		Lock();
		tMap::iterator it;
		if (!mMap.empty())
		{
			for (it = mMap.begin(); it != mMap.end();)
			{
				Remove(it->first);
				it = mMap.begin();
			}
		}
		UnLock();
		DeleteLock();
	}
	virtual bool          
    Add(T_Key &key, T_NodeData * data)
	{
		if (!data)
			return(FALSE);
		Lock();
		mMap.insert(tMap::value_type(key,*data));
		UnLock();
		return(TRUE);
	}

	virtual bool     
    Remove(const T_Key &key, T_NodeData &data)
	{
		Lock();
		if (Find(key,data))
		{
			mMap.erase(key);
			UnLock();
			return (TRUE);
		}
		UnLock();
		return(FALSE);
	}
  
	virtual bool     
    Remove(const T_Key &key)
	{
		Lock();
		if (Find(key))
		{
			mMap.erase(key);
			UnLock();
			return (TRUE);
		}
		UnLock();
		return(FALSE);
	}
	virtual bool          
    Find(const T_Key &key, T_NodeData &data)
	{
		Lock();
		if (!mMap.empty())
		{
			tMap::iterator it = mMap.find(key);
			if (it != mMap.end())
			{
				data = (*it).second;
				UnLock();
				return (TRUE);
			}
		}
		UnLock();
		return(FALSE);
	}
	virtual bool          
    Find(const T_Key &key)
	{
		Lock();
		if (!mMap.empty())
		{
			tMap::iterator it = mMap.find(key);
			if (it != mMap.end())
			{
				UnLock();
				return (TRUE);
			}
		}
		UnLock();
		return(FALSE);
	}
	virtual int           
    Count(void)const
	{return(mMap.size());}

	typedef std::map<T_Key, T_NodeData> tMap;

protected:
	tMap mMap;
};
//============================================================================
// A wrapper for STD::queue<> that deals in reference counting smart pointers
// \see cMap
//============================================================================
template <class T_NodeData>
class cQ :
	public cCriticalSection
{
public:
	cQ(void):cCriticalSection()
	{
	}
	virtual
    ~cQ(void)
	{
		Lock();
		if (!mQ.empty())
		{
			pop();
		}
		UnLock();
		DeleteLock();
	}
	virtual void
    push(T_NodeData &data)
	{
		Lock();
		mQ.push(data);
		UnLock();
	}

	virtual T_NodeData *    
    pop()
	{
		Lock();
		T_NodeData * data = mQ.front();
		UnLock();
		return(data);
	}
	virtual int           
    Count(void)const
	{return(mQ.size());}

protected:
	typedef std::queue<T_NodeData> tQ;
	tQ	mQ;
};

#endif // cSmartPtr_H

