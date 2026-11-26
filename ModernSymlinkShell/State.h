#pragma once
#include <string>

// What Pick Link Source remembers between right-clicks (see StateStore).
struct State
{
	std::wstring m_linkSource;
	bool m_isFolder;
};
