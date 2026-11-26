// ReSharper disable CppUseInternalLinkage
#include "pch.h"
#include "Dll.h"
#include "ExplorerCommands.h"

typedef HRESULT(*PFNCREATEINSTANCE) (REFIID riid, void** ppvObject);

struct CLASS_OBJECT_INIT
{
	const CLSID* pClsid;
	PFNCREATEINSTANCE pfnCreate;
};

static constexpr CLASS_OBJECT_INIT c_rgClassObjectInit[] =
{
	{ .pClsid = &CLSID_ModernSymlinkVerb, .pfnCreate = CExplorerCommandVerb_CreateInstance },
};

static long g_cRefModule = 0;

STDAPI DllCanUnloadNow()
{
	return (g_cRefModule == 0) ? S_OK : S_FALSE;
}

void DllAddRef()
{
	InterlockedIncrement(&g_cRefModule);
}

void DllRelease()
{
	InterlockedDecrement(&g_cRefModule);
}

class CClassFactory final : public IClassFactory
{
public:
	static HRESULT CreateInstance(REFCLSID clsid, const CLASS_OBJECT_INIT* pClassObjectInits, size_t cClassObjectInits, REFIID riid, void** ppv)
	{
		*ppv = nullptr;

		HRESULT hr = CLASS_E_CLASSNOTAVAILABLE;

		for (size_t i = 0; i < cClassObjectInits; i++)
		{
			if (clsid == *pClassObjectInits[i].pClsid)
			{
				IClassFactory* pClassFactory = new (std::nothrow) CClassFactory(pClassObjectInits[i].pfnCreate);

				hr = pClassFactory ? S_OK : E_OUTOFMEMORY;

				if (SUCCEEDED(hr))
				{
					hr = pClassFactory->QueryInterface(riid, ppv);
					pClassFactory->Release();
				}

				break;
			}
		}

		return hr;
	}

	explicit CClassFactory(PFNCREATEINSTANCE pfnCreate) : _cRef(1), _pfnCreate(pfnCreate)
	{
		DllAddRef();
	}

	// IUnknown
	IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override
	{
		static const QITAB qit[] =
		{
			QITABENT(CClassFactory, IClassFactory),
			{ .piid = nullptr }
		};

		return QISearch(this, qit, riid, ppv);
	}

	IFACEMETHODIMP_(ULONG) AddRef() override
	{
		return InterlockedIncrement(&_cRef);
	}

	IFACEMETHODIMP_(ULONG) Release() override
	{
		const long cRef = InterlockedDecrement(&_cRef);

		if (cRef == 0)
		{
			delete this;
		}

		return cRef;
	}

	// IClassFactory
	IFACEMETHODIMP CreateInstance(IUnknown* pUnknownOuter, REFIID riid, void** ppv) override
	{
		return pUnknownOuter ? CLASS_E_NOAGGREGATION : _pfnCreate(riid, ppv);
	}

	IFACEMETHODIMP LockServer(BOOL fLock) override
	{
		if (fLock)
		{
			DllAddRef();
		}
		else
		{
			DllRelease();
		}

		return S_OK;
	}

private:
	~CClassFactory()
	{
		DllRelease();
	}

	long _cRef;

	PFNCREATEINSTANCE _pfnCreate;
};

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void** ppv)
{
	return CClassFactory::CreateInstance(rclsid, c_rgClassObjectInit, ARRAYSIZE(c_rgClassObjectInit), riid, ppv);
}