#include "pch.h"

#include "Dll.h"
#include "ExplorerCommands.h"

CEnumExplorerCommand::CEnumExplorerCommand(IExplorerCommand* const* ppCommands, ULONG cCommands, ULONG index) : _cRef(1), _index(index)
{
	_commands.reserve(cCommands);

	for (ULONG i = 0; i < cCommands; i++)
	{
		if (ppCommands[i])
		{
			ppCommands[i]->AddRef();

			_commands.push_back(ppCommands[i]);
		}
	}

	if (_index > _commands.size())
	{
		_index = static_cast<ULONG>(_commands.size());
	}

	DllAddRef();
}

CEnumExplorerCommand::~CEnumExplorerCommand()
{
	for (IExplorerCommand* pCommand : _commands)
	{
		pCommand->Release();
	}

	DllRelease();
}

// IUnknown
IFACEMETHODIMP CEnumExplorerCommand::QueryInterface(REFIID riid, void** ppv)
{
	static const QITAB qiTab[] = {
		QITABENT(CEnumExplorerCommand, IEnumExplorerCommand),
		{ .piid = nullptr }
	};

	return QISearch(this, qiTab, riid, ppv);
}

IFACEMETHODIMP_(ULONG) CEnumExplorerCommand::AddRef()
{
	return InterlockedIncrement(&_cRef);
}

IFACEMETHODIMP_(ULONG) CEnumExplorerCommand::Release()
{
	const long cRef = InterlockedDecrement(&_cRef);

	if (!cRef)
	{
		delete this;
	}

	return cRef;
}

// IEnumExplorerCommand
IFACEMETHODIMP CEnumExplorerCommand::Next(ULONG celt, IExplorerCommand** ppUICommand, ULONG* pceltFetched)
{
	if (!ppUICommand)
	{
		return E_POINTER;
	}

	ULONG fetched = 0;

	while (fetched < celt && _index < _commands.size())
	{
		ppUICommand[fetched] = _commands[_index];
		ppUICommand[fetched]->AddRef();

		fetched++;
		_index++;
	}

	if (pceltFetched)
	{
		*pceltFetched = fetched;
	}

	return (fetched == celt) ? S_OK : S_FALSE;
}

IFACEMETHODIMP CEnumExplorerCommand::Skip(ULONG celt)
{
	if (const ULONG remaining = static_cast<ULONG>(_commands.size()) - _index; celt > remaining)
	{
		_index = static_cast<ULONG>(_commands.size());

		return S_FALSE;
	}

	_index += celt;

	return S_OK;
}

IFACEMETHODIMP CEnumExplorerCommand::Reset()
{
	_index = 0;

	return S_OK;
}

IFACEMETHODIMP CEnumExplorerCommand::Clone(IEnumExplorerCommand** ppEnum)
{
	*ppEnum = new (std::nothrow) CEnumExplorerCommand(_commands.data(), static_cast<ULONG>(_commands.size()), _index);

	return *ppEnum ? S_OK : E_OUTOFMEMORY;
}
