/*
 * Rosalie's Mupen GUI - https://github.com/Rosalie241/RMG
 *  Copyright (C) 2020-2025 Rosalie Wanders <rosalie@mailbox.org>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3.
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#ifndef M64P_MACROS_HPP
#define M64P_MACROS_HPP

#include "Library.hpp"

#define HOOK_FUNC_OPT(handle, prevar, var) this->var = (ptr_##prevar##var)CoreGetLibrarySymbol(handle, #prevar #var);

#define HOOK_FUNC(handle, prevar, var)                                    \
    this->var = reinterpret_cast<ptr_##prevar##var>(CoreGetLibrarySymbol(handle, #prevar #var)); \
    if (this->var == nullptr)                                             \
    {                                                                     \
        this->errorMessage += "Failed to hook \"" #prevar #var "\": ";    \
        this->errorMessage += CoreGetLibraryError();                     \
        return false;                                                     \
    }

#define UNHOOK_FUNC(prevar, var) \
    this->var = nullptr;

#endif // M64P_MACROS_HPP
