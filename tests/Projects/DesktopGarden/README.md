# DesktopGarden 项目

这是 `tests/Project/` 下的第一版可玩项目，包含场景、C# 玩法脚本和基础素材：
- Background：白天/夜晚窗台背景
- Plants：三叶草与月光花成长图集（每行 6 阶段）
- Props：石头、蜗牛、萤火虫图集
- Weather：雨丝与薄雾叠加层
- UI：种子、浇水、收获、肥力图标
- Scripts：`GardenController`、`GardenPlant`、`GardenOneShot`

图集导入建议：
- 水彩风格使用线性过滤
- Plants 图集每格 128x256；Props 每格约 128x256；UI 每格 128x128
- 透明 PNG 可直接交给 Texture/Sprite importer
- 植物底部基准线位于每格底部附近，切换成长阶段时保持 pivot=(0.5,1.0)

当前文件是可运行原型素材，后续可用手绘/生成素材替换同名文件。

玩法输入：
- 鼠标左键：对最近的植物槽位播种、浇水或收获
- 按住 Space：模拟打字，提高土壤肥力
- 场景自动循环昼夜，并在白天中段进入小雨

项目入口：`Garden.doproj`，启动场景：`Assets/Scenes/DesktopGarden.doscn`。
