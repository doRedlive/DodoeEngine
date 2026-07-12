#pragma once
#include "generator/generator.h"
namespace Generator
{
    class ScriptBindingGenerator : public GeneratorInterface
    {
    public:
        ScriptBindingGenerator() = delete;
        ScriptBindingGenerator(std::string source_directory, std::function<std::string(std::string)> get_include_function);

        virtual int generate(std::string path, SchemaMoudle schema) override;
        virtual void finish() override;
        virtual ~ScriptBindingGenerator() override;

    protected:
        virtual void prepareStatus(std::string path) override;
        virtual std::string processFileName(std::string path) override;

    private:
        Mustache::data m_cs_component_defines {Mustache::data::type::list};
        Mustache::data m_cs_nativecalls_defines {Mustache::data::type::list};
        Mustache::data m_cpp_glue_defines {Mustache::data::type::list};
        Mustache::data m_pybind_defines {Mustache::data::type::list};
        Mustache::data m_native_bindings_cpp {Mustache::data::type::list};
        Mustache::data m_native_bindings_cs {Mustache::data::type::list};
    };
} // namespace Generator
