#pragma once
#include "pch.h"

void DllAddRef();
void DllRelease();

HRESULT CExplorerCommandVerb_CreateInstance(REFIID riid, void** ppv);
