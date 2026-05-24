// do@Redlive

#pragma once

#include "dopch.h"

namespace cakery {

    enum class LangType {
        EN,
        ZH_CN,
    };

    class LangConfig {
    public:
        static const char* TR(const char* key);

        static void SetLangType(LangType lang_type);
        static LangType GetLangType();
    };


} // cakery