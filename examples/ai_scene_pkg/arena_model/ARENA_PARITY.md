# Arena 控件 parity（v1）

对照现有 object 控件能力；包场景直画路径。

| 控件 | 直画状态 | 说明 |
|---|---|---|
| container 底色 | [√] | 实心 fill |
| label 文本 | [√] | `name_off`=文案，`bg_rgb`=字色；需 `arena_scene_set_font` |
| button 底色 | [√] | 圆角 fill（r=6）+ press 压暗 |
| button 文案 | [√] | `name_off` 居中白字 |
| button 边框 | [ ] | 暂无 |
| image / icon | [√] | RGB565 blob（`arena_img_hdr_t` + pixels）；`reserved`=偏移 |
| list / wheel | [√] | 静态行 + 选中高亮 + 点击改 `selected`（无惯性滚动） |
| scroll / motion | [ ] | |

裸 FB `arena_draw()`（无 font）仍跳过 label，仅测色块；image 仅在 display 路径绘制。
