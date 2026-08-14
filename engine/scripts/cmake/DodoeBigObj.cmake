# 脚本绑定 / 反射生成代码会实例化大量模板，单个 TU 的 COFF 节数很容易超过
# 默认的 65535 限制（error C1128）。对整个 DodoeRuntime 开启 /bigobj 一劳永逸。
target_compile_options(DodoeRuntime PRIVATE "/bigobj")
