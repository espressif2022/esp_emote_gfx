# gfx_sm 设计链路说明

## 目标

把设计、仿真、导出统一到一条稳定链路上：

1. 导入 `Lottie JSON` 或 `scene.json v1`
2. 归一化成标准场景描述 `scene.json v1`
3. 编译成设备端可用的 `gfx_sm_asset`
4. 用和设备端一致的语义做网页仿真
5. 导出 `.inc`

这里的核心思想是：

- `scene.json v1` 是设计源文件
- `.inc` 是编译产物
- 网页仿真和设备端都围绕同一份编译结果

## 文件角色

### `gui_designer.html`

一体化工作台，负责：

- 导入 Lottie 或标准 JSON
- 编辑 `part / pose / clip / step`
- 实时回写 `scene.json v1`
- 编译并仿真
- 导出 `.inc`

### `gui_simulator.html`

偏验证和回放，负责：

- 直接加载 `.inc`
- 按设备端 `gfx_sm_scene` 语义回放
- 验证导出产物和设备端表现是否一致

## 标准模型

### 1. Part

`part` 是稳定拓扑的最小可绘制单元。

建议字段：

- `id`
- `label`
- `kind`
- `stroke_width`
- `radius_hint`

含义：

- `part` 决定“这是什么东西、怎么画”
- `part` 不直接存时间轴
- 同一个 `part` 在所有 `pose` 中必须保持相同控制点数量

推荐的 `kind`：

- `BEZIER_FILL`：贝塞尔填充
- `BEZIER_LOOP`：贝塞尔闭环
- `BEZIER_STRIP`：贝塞尔开放线
- `CAPSULE`：胶囊段
- `RING`：圆环

### 2. Pose

`pose` 是一张完整姿态快照。

建议字段：

- `id`
- `label`
- `label_cn`
- `parts`

其中：

- `parts[part_id] = [[x,y], ...]`

含义：

- `pose` 决定“所有部件在这一帧分别长什么样”
- face 资产里，本质上是各个部件控制点坐标的集合

### 3. Clip

`clip` 是一个可播放动作片段。

建议字段：

- `id`
- `label`
- `label_cn`
- `loop`
- `steps`

每个 `step`：

- `pose`
- `hold_ticks`
- `facing`
- `interp`

含义：

- `clip` 决定“按什么顺序播放哪些姿态”
- `step` 是离散步骤，不直接依赖连续时间轴

### 4. Sequence

`sequence` 是默认播放顺序。

它存的是 `clip id` 数组，而不是 `pose id`。

## 为什么不是直接用 Lottie

Lottie 更适合作为导入源，而不是设备端标准。

原因：

- Lottie 偏连续时间轴
- 设备端当前运行时偏离散 `pose / clip / step`
- 设备端更关心稳定拓扑和固定控制点数量

所以推荐链路是：

`Lottie JSON -> scene.json v1 -> .inc`

而不是：

`Lottie JSON -> .inc`

## 当前网页编辑能力

### Scene Meta

用于编辑：

- 场景名称
- 导出前缀

### Parts

当前支持：

- 查看部件列表
- 编辑部件 `id / label / kind / stroke_width / radius_hint`
- 新增部件
- 删除部件

新增部件时：

- 系统会按 `kind` 自动给所有 pose 补默认点位

默认点位规则：

- `RING`：1 点
- `CAPSULE`：2 点
- `BEZIER_STRIP`：4 点
- `BEZIER_LOOP / BEZIER_FILL`：7 点

### Poses

当前支持：

- 选择 pose
- 修改 pose 基本信息
- 选择某个 part
- 以点位卡片方式直接编辑该 part 的 X / Y 坐标
- 贝塞尔类部件按“曲线段”分组显示锚点和手柄
- 在右侧预览画布上直接拖动当前 part 的控制点
- 显示当前 `pose + part` 的位置推导值
- 在右侧预览区双击部件后，整体拖动该部件
- 复制 pose

原始控制点 JSON 仍然保留，但放到“高级模式”折叠区，主要用于：

- 批量粘贴点位
- 调试或排查导入问题
- 和历史 JSON 资产直接对照

位置推导值目前不单独落 schema 字段，而是根据当前 pose 控制点实时计算：

- 部位中心
- 包围盒起点
- 包围盒尺寸

这样做的原因是：

- `part` 本身只描述稳定拓扑和绘制方式
- 真正的位置信息随 `pose` 变化
- 设备端消费的仍然是控制点，不需要再额外维护一份位置字段

### Clips

当前支持：

- 编辑 clip 基本信息
- 新建 clip
- 编辑 step
- 新增/删除 step

## 仿真语义

网页仿真按设备端 `gfx_sm_scene` 逻辑执行：

1. `load_target`
2. `scene_advance`
3. `scene_tick`

重点行为：

- `hold_ticks`
- `facing`
- `mirror_x`
- `damping_div`

因此网页预览不只是“画出来”，而是尽量贴近设备端运行方式。

## 推荐工作流

### Lottie 导入型

1. 导入 Lottie JSON
2. 归一化为 `scene.json v1`
3. 检查部件列表是否合理
4. 在 `Poses` 里微调控制点
5. 在 `Clips` 里调整 `hold_ticks / facing / interp`
6. 仿真确认
7. 导出 `.inc`

### 资产微调型

1. 打开已有 `scene.json v1`
2. 编辑 `part / pose / clip`
3. 观察右侧实时仿真
4. 导出新 `.inc`

## 当前边界

当前版本还没有做这些：

- 拖动重排 `parts / poses / clips`
- 左右镜像自动生成部件
- part 模板库
- scene diff / 历史版本

## 下一步建议

推荐优先顺序：

1. 画布拖点之外的“节点插入 / 删除 / 约束编辑”
2. `Part` 的镜像复制
3. `Part` 顺序调整
4. `Clip sequence` 的 GUI 编辑
5. 更细的 Lottie 图层映射面板
