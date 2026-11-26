#include "pch.h"
#include "StateStore.h"

static constexpr auto c_szKey = L"Software\\ModernSymlink";
static constexpr auto c_szLinkSourceValue = L"LinkSource";
static constexpr auto c_szIsFolderValue = L"LinkSourceIsFolder";
static constexpr auto c_szClipboardSequenceValue = L"ClipboardSequence";

static HRESULT SetDword(PCWSTR pszValue, DWORD value)
{
	return HRESULT_FROM_WIN32(SHSetValueW(HKEY_CURRENT_USER, c_szKey, pszValue, REG_DWORD, &value, sizeof(value)));
}

static bool GetDword(PCWSTR pszValue, DWORD& value)
{
	DWORD type = 0;
	DWORD cb = sizeof(value);

	return SHGetValueW(HKEY_CURRENT_USER, c_szKey, pszValue, &type, &value, &cb) == ERROR_SUCCESS && type == REG_DWORD;
}

HRESULT StateStore::Save(const State& state)
{
	if (state.m_linkSource.empty())
	{
		return E_INVALIDARG;
	}

	const DWORD cbPath = static_cast<DWORD>((state.m_linkSource.size() + 1) * sizeof(WCHAR));

	if (const LSTATUS ls = SHSetValueW(HKEY_CURRENT_USER, c_szKey, c_szLinkSourceValue, REG_SZ, state.m_linkSource.c_str(), cbPath); ls != ERROR_SUCCESS)
	{
		return HRESULT_FROM_WIN32(ls);
	}

	HRESULT hr = SetDword(c_szIsFolderValue, state.m_isFolder ? 1 : 0);

	if (SUCCEEDED(hr))
	{
		hr = SetDword(c_szClipboardSequenceValue, GetClipboardSequenceNumber());
	}

	return hr;
}

std::optional<State> StateStore::Load()
{
	WCHAR buffer[MAX_PATH * 4] = {};
	DWORD cb = sizeof(buffer);
	DWORD type = 0;

	if (SHGetValueW(HKEY_CURRENT_USER, c_szKey, c_szLinkSourceValue, &type, buffer, &cb) != ERROR_SUCCESS || type != REG_SZ || buffer[0] == L'\0')
	{
		return std::nullopt;
	}

	DWORD clipboardSequence = 0;

	const bool clipboardChanged = !GetDword(c_szClipboardSequenceValue, clipboardSequence) || clipboardSequence != GetClipboardSequenceNumber();

	if (const bool sourceGone = !PathFileExistsW(buffer); clipboardChanged || sourceGone)
	{
		Clear();

		return std::nullopt;
	}

	// the folder flag is optional in the store; missing means "not a folder"
	DWORD isFolder = 0;

	GetDword(c_szIsFolderValue, isFolder);

	return State{ .m_linkSource = buffer, .m_isFolder = isFolder != 0 };
}

void StateStore::Clear()
{
	SHDeleteValueW(HKEY_CURRENT_USER, c_szKey, c_szLinkSourceValue);
	SHDeleteValueW(HKEY_CURRENT_USER, c_szKey, c_szIsFolderValue);
	SHDeleteValueW(HKEY_CURRENT_USER, c_szKey, c_szClipboardSequenceValue);
}
