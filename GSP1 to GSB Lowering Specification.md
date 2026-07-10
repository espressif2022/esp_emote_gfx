# GSP1 → GSB Lowering 规范

> 状态：草案 v0.9（随 GSB 规范同步冻结）
> 输入：GSP1 v4 JSON（`tools/uic/gsp_v4_schema.py`：9 种对象类型、6 事件、10 动作）
> 输出：GSB/GRB/GFB + manifest + 应用侧头文件
> 关联文档：《GSB Binary Format Specification》《GSP Primitive Coverage Specification》
> 实现者：gspc（frontend → ir → opt → backend）

## 1. 目的与总原则

本规范定义 gspc 如何把作者格式 GSP1 降级为设备执行格式 GSB。总原则：

1. **属性驱动，不是控件矩阵驱动**。旧协议为每种控件维护"属性支持矩阵"
   （旧协议 §7.2）；lowering 后控件不复存在，任何对象上出现的
   `bg_color/border/radius/text/image` 属性都按统一规则产生命令。
   `type` 只在两处起作用：结构展开（list/wheel/progress/toggle 的特殊
   构造）和默认交互（button 的 hit entry）。
2. **设备端零解析**：坐标、颜色、文本、动作在编译期全部落定为
   绝对值/索引（主计划 §4.1 的"设备不再执行"清单是本规范的验收面）。
3. **确定性**：相同输入 + 相同 target profile + 相同 gspc 版本必须产生
   字节一致的输出（可复现构建，CI 依赖此性质做回归）。

## 2. 全局降级规则

### 2.1 坐标

- 对象 `x/y` 为相对 parent 的局部坐标；gspc 沿 parent 链累加得到绝对
  坐标，写入命令 bbox（half-open：`x2 = x + w`）。
- parent 引用必须满足 `parent < 当前对象索引`（先序，继承旧协议规则）；
  环不可能存在，越界即报错。
- 全部或部分超出屏幕的命令保留并依赖 clip；完全不可见（零面积或
  完全在屏幕外且无 bind）的命令在 opt 阶段删除并记入 manifest。

### 2.2 z-order 与命令顺序

- `objects[]` 数组顺序即绘制顺序（与旧协议先序一致）：对象自身背景 →
  边框 → 内容（text/image）→ 其子对象。
- 子树在命令流中展平为**连续区间**——这是 `VIS_GROUPS` 区间语义的
  前提，opt 阶段的命令删除/合并不得破坏区间连续性。

### 2.3 颜色

- 作者值 `#RRGGBB` / `#RRGGBBAA` / 整数 → IR 统一为语义 RGBA8888
  （主计划 §2.5：不从 RGB565 反推）。
- backend 按 target profile 量化为 native 值写入命令 payload；量化规则
  见 Coverage 规范 §3。
- 无 alpha 的作者色 alpha=255。

### 2.4 可见性与 `hidden`

- 静态 `hidden: true` 且无任何 bind/action 指向该对象及其子树 →
  整个子树不生成命令（死代码删除，记入 manifest）。
- `hidden` 但可被动作显示（SHOW/TOGGLE/GOTO 目标）→ 子树生成
  `VIS_GROUPS` 条目 + VISIBLE state slot，默认 `visible=false`。
- `layer` 类型对象无条件生成 VIS_GROUP（它是 GOTO 的切换单元）。

### 2.5 bind 与标识符

- 对象 `bind: "ident"` → 分配 `bind_id`（按名字排序后顺序分配，保证
  确定性）；按对象属性生成对应类型的 state slot 与 `BIND_INDEX` 条目：
  有 `text` → TEXT，有 `value` → I32，有 `image` → RESOURCE，
  有 `checked` → I32，其余 → VISIBLE。
- 一个 bind 名只允许出现在一个对象上；重复即报错。
- 输出 `<scene>_binds.h`：`#define GSP_BIND_<IDENT> <bind_id>`，应用侧
  使用宏而非魔数。

### 2.6 callback 与 action_id

- `callback: "fn_name"` 及 `action: call` 的 `target_name` → 分配整数
  `action_id`（按名字排序顺序分配）。
- 输出 `<scene>_actions.h`：`#define GSP_ACT_ID_<NAME> <action_id>`；
  名称同时写入 `DEBUG_NAMES`（调试构建）与 manifest。
- 设备运行时无字符串查找（主计划 §12）。

### 2.7 字体与文本资源

- 对象 `font`/`font_size` 回退到场景级 `font`/`default_font_size`；
  每个 (字体, 字号, 目标字符集) 组合产生一个 GFB。
- 场景静态字符集 = 全部静态 `text` 的并集；动态字符集由业务在 gspc
  配置中声明（默认 ASCII 0x20..0x7E）。缺字报告进 manifest（主计划 §7.5）。

## 3. 类型映射表

通用属性命令（所有类型一致，出现即生成，顺序固定）：

| 属性组合 | 生成命令 |
|---|---|
| `bg_color`，`radius == 0` | `FILL_RECT(bg_color)` |
| `bg_color`，`radius > 0` | `FILL_ROUND_RECT(bg_color, radius, stroke_width=0)` |
| `border_color + border_width` | `FILL_ROUND_RECT(border_color, radius, stroke_width=border_width, stroke_mode=inside)` |
| `text` | §4 文本降级 |
| `image` | §5 图片降级 |
| `opacity` | 写入上述各命令的 `opacity` 字段（不生成独立命令） |

类型特有规则：

| type | 结构展开 | 交互 |
|---|---|---|
| `container` | 仅通用属性 + 子对象 | 无默认；`events[]` 按 §6 |
| `label` | 仅通用属性（text 必需） | 无默认 |
| `button` | 通用属性；text 默认水平垂直居中 | 默认生成 hit entry（CLICK）；无 events/callback 时警告 |
| `image` | 通用属性（image 必需） | 无默认 |
| `layer` | 通用属性 + VIS_GROUP（§2.4） | GOTO 目标 |
| `progress` | 轨道：通用 bg/border；滑条：`FILL_[ROUND_]RECT(fg_color, value_mode=1|2)`，`vertical` 选 value_mode；`value` → I32 slot，命令 bbox 为满值矩形 | VALUE 事件 |
| `toggle` | 轨道 round-rect；开/关两态 knob 各自成组：`VIS_GROUPS(MATCH_VALUE, match_value=1/0)` 绑定 `checked` I32 slot | 默认 hit(CLICK) → `ACT_TOGGLE_VALUE(checked)` |
| `list` | §3.1 | 行 hit(CLICK) → `ACT_SET_VALUE(selected, arg=row)` + VALUE |
| `wheel` | §3.1（同 list 策略） | VALUE 事件 |

### 3.1 list/wheel 的两级策略

- **静态可全显**（`items` 行数 × 行高 ≤ viewport 高，且无滚动需求）：
  直接展开为行命令 + 每行 hit entry；`selected` → I32 slot，选中态高亮
  以 VIS_GROUPS(MATCH_VALUE, match_value=行号) 实现。
- **需滚动**：降级为行 template（`TEMPLATES` section）+ repeater
  描述（主计划 §11.4），viewport 用 `CLIP_PUSH/POP` 包裹。repeater
  描述符的二进制格式与滚动交互属 Phase 2 交付，本规范先冻结上述
  分界条件与静态路径。
- `items_per_page/visible_rows/cyclic/snap_to_item/item_height/params_hex`
  进入 repeater 描述符；静态路径中仅 `item_height` 参与布局。

## 4. 文本降级

1. 解析 UTF-8 → codepoint 序列；按对象字体/字号选择 GFB；
2. host 端完成 glyph 选择、kerning、（需要时）shaping；
3. `text_align`（auto=left）在编译期折算进 glyph 坐标：对齐基准为对象
   bbox，`auto/left/center/right` 分别取左缘/左缘/水平居中/右缘；
   垂直方向 v1 统一 baseline 居中（`(h + ascent - descent) / 2` 处为
   baseline，整数舍入向下）；
4. 无 bind：写入 `GLYPH_RUNS`，生成 `DRAW_GLYPH_RUN`（bbox = 实际
   glyph 覆盖范围 ∩ 对象 bbox）；
5. 有 bind：TEXT slot + `TEXT_DEFAULTS` 存默认 UTF-8；命令
   `STATE_BOUND`，bbox = 对象完整 bbox（运行期文本变化的最大 dirty）；
   运行期 glyph run 由 runtime 用 GFB cmap 生成并缓存（主计划 §7.5）；
6. 文本超出对象 bbox：编译期裁剪到 bbox 并在 manifest 告警（v1 无
   省略号/换行；多行属 gspc 后续能力，不改变 GSB 编码）。

## 5. 图片降级

1. `image: "path"` 经 gspc image pipeline（现 `image.py` 迁移）处理：
   按 target profile 决定 codec/pixel_format/alpha_mode/cache_policy
   （决策树见主计划 §7.4 图片分类）；
2. 输出进 GRB（跨场景以 `content_hash` 去重），场景内 `RES_REFS` 引用；
3. 对象 `w/h` ≠ 源图尺寸 → host 端缩放（v1 无运行期 scale）；
4. 不透明 → `BLIT_OPAQUE`；含 alpha → `BLIT_ALPHA`（alpha_mode 按资源
   entry）；
5. 有 bind（RESOURCE 类型）→ 命令 `STATE_BOUND`，运行期换图必须与
   编译期资源同尺寸同格式（gspc 对声明的候选资源集校验），bbox 不变。

## 6. 交互降级

1. 对象级 `events[]` 与顶层 `actions[]`（`src/src_idx/src_name` 定位源
   对象）合并为统一 (源对象, 事件) → 动作序列；
2. 每个有交互的源对象生成一个 hit entry：bbox = 对象绝对 bbox，
   z = 对象命令区间末尾的 z 序，`action_first/action_count` 指向按序
   排列的动作链（GSB §3.7）；
3. 动作映射：`show/hide/toggle` → 目标对象 VISIBLE bind（必要时为目标
   补建 VIS_GROUP）；`set_text/set_bg_color/set_opacity` → 对应类型
   bind（目标无该 slot 即报错）；`call` → §2.6 action_id；`goto` 目标为
   layer → 展开为 `SHOW(目标) + HIDE(各同级 layer)` 动作链，目标为
   scene → `ACT_GOTO(scene_id)`；`back` → `ACT_BACK`；
4. `param` 字符串进 `ACTION_PARAMS`；`arg` 若为颜色字符串按 §2.3 编码
   后写入 `arg`。

## 7. 优化 pass（固定顺序，均可单独关闭以定位问题）

1. **死代码删除**：不可见静态子树、零面积命令（§2.4/§2.1）；
2. **覆盖剔除**：被后续不透明命令完全覆盖的静态命令删除
   （主计划 §4.4；不得跨 VIS_GROUP 边界剔除——组可能被隐藏）；
3. **同色矩形合并**：相邻、同色、同 opacity、z 间无交叠命令的
   `FILL_RECT` 合并；
4. **资源去重**：GRB `content_hash`；字符串/glyph run 场景内去重；
5. **静态烘焙**（可选，profile 开关）：连续静态命令区间烘焙为
   `DRAW_STATIC_TILE`（主计划 §4.3，tile 粒度）；
6. **tile index 决策**：按命令数与重叠率决定是否生成 `TILE_INDEX`
   （主计划 §5.4），阈值来自 profile。

每个 pass 输出统计进 manifest（删除数、合并数、烘焙面积），
禁止静默丢弃（主计划"no silent caps"原则）。

## 8. 输出工件

| 工件 | 内容 |
|---|---|
| `<scene>.gsb` | 场景二进制 |
| `<bundle>.grb` / `<font>.gfb` | 资源与字体（bundle 级共享） |
| `<scene>_binds.h` / `<scene>_actions.h` | 应用侧常量 |
| `<scene>.manifest.md` | header 值、section 布局、命令/bind/action/hit/组/模板表、资源引用、pool 与 RAM 预算、优化统计、缺字报告、校验结果 |
| preview golden | host preview 渲染的 framebuffer golden（CI 基准） |

## 9. 诊断策略

- **错误**（终止导出）：schema 校验失败、parent 越界、bind 重名、动作
  目标缺 slot、bind 换图尺寸/格式不匹配、计数溢出（命令 >65535 等）；
- **警告**（导出但记录）：button 无交互、文本溢出裁剪、缺字 fallback、
  完全屏幕外对象、opacity=0 静态对象；
- 所有诊断带 JSON 路径定位（`objects[12].text`），供 web editor 回显。
