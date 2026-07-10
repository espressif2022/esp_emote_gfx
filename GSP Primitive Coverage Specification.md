# GSP Primitive Coverage 规范

> 状态：草案 v0.9（reference rasterizer 通过 golden 向量后冻结为 v1.0）
> 地位：**规范性文档**。reference rasterizer 逐条实现本文公式；一切快路径
> （SW 优化、SIMD、PPA、DMA2D）的正确性以 reference 输出为准（主计划 §6.4）。
> 范围：GSB v1 全部 opcode 的像素语义。LINE/ARC/PATH 不在 v1 opcode 集内，
> 本文不定义。

## 1. 坐标与覆盖模型

1. 像素 `(x, y)` 占据连续平面区域 `[x, x+1) × [y, y+1)`，采样中心
   `(x+0.5, y+0.5)`。
2. 所有区域为 half-open：`[x1, x2) × [y1, y2)`；`x2 <= x1` 或 `y2 <= y1`
   为空区域，产生零像素（合法，非错误）。
3. 硬边图元（无 AA 边）按采样中心归属：像素中心落入区域即覆盖 1，
   否则 0。整数矩形因此精确覆盖 `(x2-x1)*(y2-y1)` 个像素，无歧义。
4. v1 所有几何输入为整数像素（GSP1 坐标即整数）；亚像素几何是未来
   扩展，不影响本文规则。因此 **v1 中 AA 只出现在圆角弧段**。

## 2. Clip

1. 有效 clip = 命令 bbox ∩ `clip_id` 矩形 ∩ 当前 `CLIP_PUSH` 栈的交集
   ∩ 目标 surface 区域。全部为 half-open 整数矩形，交集为空则命令
   零输出。
2. clip 在 coverage 计算**之后**、写像素**之前**应用：clip 只裁剪写入
   范围，不改变几何形状的 coverage 值（跨 clip 边缘的弧线在边缘处的
   AA 值与未裁剪时一致）。
3. 任何 backend 不得自行猜测 inclusive/exclusive；PPA/DMA2D 的窗口
   参数由统一层从 half-open 换算。

## 3. 数值规则

### 3.1 8-bit 定点除法（规范定义，全库唯一）

```text
div255(x) = (x + 127) / 255        （精确整数除法，round-half-up）
```

允许实现用 `(x + 128 + ((x + 128) >> 8)) >> 8` 等价替换，但必须与
div255 逐值一致（0..65025 全域可枚举验证）。

### 3.2 RGB565 与 8-bit 互转

```text
展开：r8 = (r5 << 3) | (r5 >> 2)    g8 = (g6 << 2) | (g6 >> 4)   （位复制）
量化：r5 = (r8 * 31 + 127) / 255    g6 = (g8 * 63 + 127) / 255   （round-nearest）
```

RGB565 目标上的 blend：展开到 8-bit → 按 §4 混合 → 量化。快路径可在
565 域直接运算，但结果必须与上述流程逐像素一致（见 §10 一致性等级）。

### 3.3 coverage 表示

coverage 为 `[0, 255]` 的 u8；几何 coverage（浮点 `[0,1]`）转 u8：
`round(cov * 255)`（round-half-up）。reference 内部用浮点或 ≥8 小数位
定点计算 coverage，最终量化点固定在"每像素每命令一次"。

## 4. 合成公式

### 4.1 有效 alpha 的分级合成（顺序固定）

```text
a1 = div255(opacity * coverage)          # 命令 opacity × 几何/mask coverage
ae = div255(a1 * src_alpha)              # × 源像素 alpha（无源 alpha 时 src_alpha=255）
```

分级顺序不可交换（舍入结果不同）。opacity=255、coverage=255、
src_alpha=255 的全不透明路径必须短路为直写（不经乘法，保证无损）。

### 4.2 混合方程（straight alpha，v1 规范）

```text
dst' = div255(src * ae + dst * (255 - ae))     # 每通道独立
```

- v1 的内部合成约定为 **straight alpha**；`alpha_mode = premultiplied`
  的资源在 blit 时按 `dst' = src + div255(dst * (255 - ae))` 混合
  （src 已预乘，ae 仍按 §4.1 计算并已含 opacity/coverage 缩放——
  premul 源与 opacity/coverage 组合时 src 需先乘 `div255(a1)`）。
- 目标无 alpha 通道（RGB565/RGB888/XRGB8888）时 dst alpha 恒视为 255，
  结果 alpha 丢弃。
- `ae == 0` 必须跳过写入（不得写入等值像素——影响 dirty 语义与总线）。

## 5. `FILL_RECT`

整数矩形，无 AA。coverage：区域内 255，区域外 0。`value_mode != 0` 时
先按 slot.value 调整 `x2`（mode 1：`x2 = x1 + round(w * value / 100)`）
或 `y2`（mode 2，自底向上：`y1 = y2 - round(h * value / 100)`），再按
本节规则填充。

## 6. `FILL_ROUND_RECT`（填充）

### 6.1 几何

- 矩形 `[x1,x2)×[y1,y2)`，四角半径 `r`；
- **radius clamp（规范）**：`r_eff = min(r, floor(min(w, h) / 2))`；
- `r_eff == 0` 退化为 `FILL_RECT`（必须走同一条硬边规则）。

### 6.2 coverage（规范：SDF 线性 AA）

四个角的圆心：`(x1 + r_eff, y1 + r_eff)` 等。像素中心 `p`：

```text
角区（p 同时落在某角的 x 带与 y 带内）：
    d = distance(p, 该角圆心)
    cov = clamp(r_eff + 0.5 - d, 0.0, 1.0)
非角区：cov = 1（区域内）/ 0（区域外，按 §1.3 硬边规则）
```

即：直边硬边、弧段 1px 线性过渡的 signed-distance 覆盖。选择 SDF 线性
而非解析面积积分：确定性强、成本低、与主流实现一致；由于它是规范定义，
精度争议不存在——所有路径与它比对。

## 7. `FILL_ROUND_RECT`（描边，`stroke_width > 0`）

### 7.1 几何（outer − inner 双轮廓）

| stroke_mode | outer 轮廓 | inner 轮廓 |
|---|---|---|
| `inside` | 原形状（`r_eff`） | 内缩 `s`：矩形四边内移 `s`，`r_in = max(r_eff - s, 0)` |
| `center` | 外扩 `ceil(s/2)`，`r_out = r_eff + ceil(s/2)` | 内缩 `floor(s/2)`，`r_in = max(r_eff - floor(s/2), 0)` |
| `outside` | 外扩 `s`，`r_out = r_eff + s` | 原形状 |

奇数宽度 center 描边按上表 ceil/floor 分配（外侧多 1px），此为规范决定。

### 7.2 coverage

```text
cov_stroke = clamp(cov_outer - cov_inner, 0, 1)
```

两个轮廓各按 §6.2 计算。**禁止**以"背景色覆盖"伪造空心（主计划 §2.5）：
半透明描边、描边下有图片时语义必须正确。

### 7.3 退化规则

- inner 内缩后为空区域（`s >= ceil(min(w,h)/2)`，inside 模式）→ 实心，
  等价 stroke coverage = outer coverage；
- `r_in == 0` 时 inner 为硬边矩形；
- 描边与填充同时存在（GSP1 对象同时有 bg 与 border）由 lowering 生成
  两条命令，先填充后描边，不在单命令内合成。

## 8. Blit 与 Mask

### 8.1 `BLIT_OPAQUE`

源矩形 `[src_x, src_x + (x2-x1)) × [src_y, src_y + (y2-y1))` 必须完全
落在资源尺寸内（loader/gspc 校验）。同格式逐像素拷贝，无转换、无
运算；结果与 memcpy 语义一致。

### 8.2 `BLIT_ALPHA`

- `alpha_mode = straight/premultiplied`：`src_alpha` 取像素 alpha，按
  §4 合成；
- `alpha_mode = A8 平面`（RGB565+A8 双平面）：color 取 RGB 平面，
  `src_alpha` 取 A8 平面对应像素；
- coverage 恒为 255（blit 无几何 AA），命令 `opacity` 参与 §4.1。

### 8.3 `DRAW_GLYPH_RUN`

- 每 glyph：A8 atlas 位图即 coverage（`coverage = mask 值`），文本
  color 为纯色源（`src_alpha` = color 的 alpha），按 §4 合成；
- glyph 依 run 内顺序**串行混合**；重叠 glyph 的 AA 边缘双重混合是
  规范行为（与串行模型一致，不做 coverage 并集）；
- glyph 位置为整数（fontc 已折算 bearing/kerning），无亚像素。

### 8.4 `DRAW_STATIC_TILE`

语义同 `BLIT_OPAQUE`（baked tile 必为目标 native 格式、不透明）。

## 9. Dirty/bbox 扩张规则

命令 bbox 必须包含其全部可写像素（emitter 责任，loader 抽查校验）：

| 情形 | bbox 相对几何矩形的扩张 |
|---|---|
| 填充/inside 描边/blit/glyph/tile | 0（SDF 过渡带在形状内） |
| center 描边 | 四边各 `ceil(s/2)` |
| outside 描边 | 四边各 `s` |
| `STATE_BOUND` 命令 | 取运行期可达的最大 bbox（lowering 规范 §4/§5） |

renderer 在 debug 构建断言"写入像素 ⊆ bbox ∩ clip"。

## 10. 一致性等级

| 路径 | 要求 |
|---|---|
| reference rasterizer | 本文公式的逐条实现，慢速、无优化，host 可跑 |
| SW 快路径 / SIMD | 与 reference **逐像素 bit-exact** |
| PPA/DMA2D fill、copy、opaque blit | 与 reference **bit-exact**（纯写入/拷贝，无舍入自由度） |
| PPA blend | 允许每通道 **±1 LSB**（硬件舍入不可控），golden 比对采用带容差模式；偏差分布记入 benchmark 报告 |
| 混合路径（HW interior + SW AA 边缘） | 同一 primitive 的两部分共享同一 coverage 定义，接缝处逐像素 bit-exact（接缝像素必须由 SW 侧产生） |

任何路径不满足其等级 → 该路径禁用并回退软件，不放宽等级。

## 11. Golden 向量清单（v1 必测）

每个向量 × 每个 output format（RGB565/RGB888/ARGB8888/L8）×
opacity ∈ {0, 1, 128, 254, 255}：

1. 矩形：0×0、1×1、1×N、全屏、负坐标起点、跨 surface 边缘；
2. 圆角：r=0、r=1、r=min(w,h)/2（精确半圆）、r 越界 clamp、w≠h；
3. 描边：s=1、s=2、s 奇数 center、s ≥ 半尺寸退化实心、三种 mode ×
   半透明描边、描边下有图片背景；
4. clip：clip 边缘切过弧段 AA 带、clip 与 bbox 空交、嵌套 CLIP_PUSH；
5. blit：straight/premul/A8 双平面 × opacity 组合、src 偏移边界；
6. glyph：重叠 glyph、clip 截断 glyph、mask 值 {0,1,254,255}；
7. value_mode：value ∈ {0, 1, 50, 99, 100}；
8. 混合覆盖：同一像素 fill→blit→glyph 三层叠加的串行一致性。

向量以参数化脚本生成（`tools/preview` 与组件 `test/` 共用一份定义），
reference 输出即 golden；任何 coverage 公式修改 = 本规范版本变更 +
全量 golden 重生成。
