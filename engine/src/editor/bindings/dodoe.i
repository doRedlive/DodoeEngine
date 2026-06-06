// do@Redlive
// SWIG interface — defines which C API functions are exposed to C#
// Usage: swig -csharp -c++ -outdir Generated -namespace Dodoe.Bindings dodoe.i

%module DodoeRuntime

// ============================================================
// 1. SWIG standard library
// ============================================================
%include "stdint.i"

// ============================================================
// 2. C code block (inserted into generated _wrap.cxx)
// ============================================================
%{
#include "native_api/dodoe_api.h"
%}

// ============================================================
// 3. Typemaps & configuration
// ============================================================

// SWIG handles C structs as C# classes by default:
//   DodoeVec2        -> C# DodoeVec2 (x, y properties)
//   DodoeTextureInfo -> C# DodoeTextureInfo (id, path, width, height)
//   DodoeAssetRef    -> C# DodoeAssetRef (id, path_id, path, type)

// Tell SWIG to ignore the DODOE_API macro
#define DODOE_API

// ============================================================
// 4. Parse header
// ============================================================
%include "native_api/dodoe_api.h"
