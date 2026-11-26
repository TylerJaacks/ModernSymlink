#pragma once
#include "pch.h"
#include "State.h"
#include <optional>

struct StateStore
{
	static HRESULT Save(const State& state);
	static std::optional<State> Load();
	static void Clear();
};