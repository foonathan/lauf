// Copyright (C) 2022-2023 Jonathan Müller and null contributors
// SPDX-License-Identifier: BSL-1.0

// C header including everything to check that it compiles as C.
#include <lauf/config.h>
#include <lauf/lib.h>
#include <lauf/reader.h>
#include <lauf/vm.h>
#include <lauf/writer.h>

#include <lauf/asm/builder.h>
#include <lauf/asm/module.h>
#include <lauf/asm/program.h>

#include <lauf/runtime/process.h>
#include <lauf/runtime/stacktrace.h>
#include <lauf/runtime/value.h>

#include <lauf/backend/dump.h>
#include <lauf/backend/qbe.h>
#include <lauf/frontend/text.h>

