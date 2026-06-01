// do@Redlive

#include "lang_config.h"

namespace cakery {

    using LangDict = std::unordered_map<std::string_view, const char*>;

    static LangType s_cur_lang = LangType::EN;

    static LangDict s_dict_en = {
        {"AUTO_SCROLL", "Auto Scroll"},
        {"BIN_CLEAR", "Clear"},
        {"FILTER", "Filter"},
        {"COLLAPSE", "Collapse"},
        {"CLEAR_ON_PLAY", "Clear on Play"},
        {"SEARCH", "Search logs"},
        {"ALL", "All"},
        {"TRACE", "Trace"},
        {"DEBUG", "Debug"},
        {"INFO", "Info"},
        {"WARN", "Warn"},
        {"ERROR", "Error"},
        {"CRITICAL", "Critical"},
        {"PROJECT_MANAGER", "Project Manager"},
        {"PROJECT_MANAGER_SUBTITLE", "Select a project to continue"},
        {"PM_SEARCH_HINT", "Search projects"},
        {"PM_RECENT", "Recent Projects"},
        {"PM_DETAILS", "Details"},
        {"PM_NAME", "Name"},
        {"PM_PATH", "Project File"},
        {"PM_PINNED", "Pinned"},
        {"PM_NO_SELECTION", "Select a project on the left."},
        {"PM_NEW", "New"},
        {"PM_OPEN", "Open"},
        {"PM_DELETE", "Delete"},
        {"PM_REFRESH", "Refresh"},
        {"PM_OPEN_HINT", "Enter a .doproj file path:"},
        {"PM_NEW_HINT", "Create a new project:"},
        {"PM_LOCATION", "Location"},
        {"PM_CREATE", "Create"},
        {"PM_CANCEL", "Cancel"},
        {"PM_DELETE_CONFIRM", "Delete selected project?"},
        {"PM_DELETE_FROM_DISK", "Also delete project folder from disk"},
    };

    static LangDict s_dict_cn = {
        {"AUTO_SCROLL", "自动滚动"},
        {"BIN_CLEAR", "清除"},
        {"FILTER", "过滤"},
        {"COLLAPSE", "折叠"},
        {"CLEAR_ON_PLAY", "运行时清除"},
        {"SEARCH", "搜索日志"},
        {"ALL", "全部"},
        {"TRACE", "跟踪"},
        {"DEBUG", "调试"},
        {"INFO", "信息"},
        {"WARN", "警告"},
        {"ERROR", "错误"},
        {"CRITICAL", "严重"},
        {"PROJECT_MANAGER", "项目管理器"},
        {"PROJECT_MANAGER_SUBTITLE", "选择一个项目后进入编辑器"},
        {"PM_SEARCH_HINT", "搜索项目"},
        {"PM_RECENT", "最近项目"},
        {"PM_DETAILS", "详情"},
        {"PM_NAME", "名称"},
        {"PM_PATH", "项目文件"},
        {"PM_PINNED", "置顶"},
        {"PM_NO_SELECTION", "请在左侧选择一个项目"},
        {"PM_NEW", "新建"},
        {"PM_OPEN", "打开"},
        {"PM_DELETE", "删除"},
        {"PM_REFRESH", "刷新"},
        {"PM_OPEN_HINT", "输入 .doproj 文件路径："},
        {"PM_NEW_HINT", "创建新项目："},
        {"PM_LOCATION", "位置"},
        {"PM_CREATE", "创建"},
        {"PM_CANCEL", "取消"},
        {"PM_DELETE_CONFIRM", "确认删除选中的项目？"},
        {"PM_DELETE_FROM_DISK", "同时从磁盘删除项目目录"},
    };

    static const LangDict& GetDict(LangType lang) {
        switch (lang) {
            case LangType::EN:    return s_dict_en;
            case LangType::ZH_CN: return s_dict_cn;
            default:              return s_dict_en;
        }
    }

    const char* LangConfig::TR(const char* key) {
        const auto& dict = GetDict(s_cur_lang);
        if (auto it = dict.find(key); it != dict.end())
            return it->second;
        return key;
    }

    void LangConfig::SetLangType(LangType lang_type) {
        s_cur_lang = lang_type;
    }

    LangType LangConfig::GetLangType() {
        return s_cur_lang;
    }

} // cakery
