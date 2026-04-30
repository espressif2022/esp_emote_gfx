Motion、Mesh Image 与绘制架构说明
==================================

文档目标
--------

这份文档用于说明当前 motion scene、mesh image 和底层绘制链路的模块划分，方便后续做性能优化、功能迭代和代码 review。重点覆盖下面几个文件：

* ``src/widget/motion/gfx_motion_scene.c``
* ``src/widget/motion/gfx_motion_player.c``
* ``src/widget/motion/gfx_motion_primitives.c``
* ``src/widget/motion/gfx_motion_style.c``
* ``src/widget/img/gfx_mesh_img.c``
* ``src/core/draw/gfx_blend.c``

当前架构的核心原则是：动作播放、segment 到 mesh 的转换、mesh image 绘制、底层像素混合是四个独立层次。每一层只维护自己负责的状态；如果需要跨层交互，应通过小而稳定的 API 完成，而不是让上层直接依赖下层内部实现。

整体数据流
----------

运行时链路如下：

.. code-block:: text

   gfx_motion_asset_t
       |
       v
   gfx_motion_scene.c
       校验 asset，维护 action 时间线，更新 pose_cur/pose_tgt
       |
       v
   gfx_motion_player.c
       管理 mesh 对象、callback、canvas 映射和 segment dispatch
       |
       +--> gfx_motion_primitives.c
       |       生成 capsule/ring/bezier 的 mesh 几何
       |
       +--> gfx_motion_style.c
               绑定 palette/resource/opacity/layer/UV 样式
       |
       v
   gfx_mesh_img.c
       维护 mesh 状态、图片源、UV/rest_points、bounds，
       绘制每个 mesh cell 或 scanline 填充多边形
       |
       v
   gfx_blend.c
       三角形光栅化、图片采样、多边形填充、AA、裁剪
       |
       v
   display buffer

模块职责边界
------------

``gfx_motion_scene.c`` 是 parser 和 action timeline 层。

它负责：

* 校验 ``gfx_motion_asset_t`` 的结构合法性。
* 维护 pose 状态：``pose_cur`` 和 ``pose_tgt``。
* 维护当前 action、当前 step、step tick、loop override。
* 处理 ``HOLD``、``DAMPED`` 等插值策略。
* 加载 target pose 时处理 facing 和 mirror。

它不应该负责：

* display object 的创建或销毁。
* mesh grid 的尺寸或点位。
* 像素颜色、图片 descriptor、palette 图片。
* scanline fill、triangle fallback 等绘制策略。

``gfx_motion_player.c`` 是 motion scene 到显示对象的适配层。

它负责：

* 每个 segment 创建并持有一个 ``gfx_mesh_img`` 对象。
* 将设计空间坐标映射到目标 canvas 的屏幕坐标。
* 设置每个 segment 的 mesh grid，并缓存 grid 尺寸以避免热路径重复分配。
* 提供 ``gfx_motion_t`` 的 tick/apply callback，把 scene 状态同步到 mesh 对象。
* 将 segment 分发给 primitive helper 和 style helper。

它不应该负责：

* action timeline 规则本身，除了调用 ``gfx_motion_scene_*``。
* 通用 mesh 绘制逻辑。
* 底层三角形光栅化、多边形填充或 alpha blend。
* primitive 几何算法和资源样式绑定细节。

``gfx_motion_primitives.c`` 是 motion 几何算法层。

它负责：

* 将 capsule、ring、Bezier stroke、Bezier fill 转换成 mesh 点。
* 通过 ``gfx_motion_player_runtime_scratch_t`` 使用 primitive 局部 scratch。
* cubic Bezier 位置和 tangent 计算。
* stroke extrusion 和 fill mesh generation。

它不应该负责：

* action 播放。
* display object 生命周期。
* palette/resource 绑定。
* 通用 mesh 绘制内部细节。

``gfx_motion_style.c`` 是 motion 样式和资源绑定层。

它负责：

* runtime solid color、palette color、opacity、texture source、UV crop、layer visibility helper。
* 将 resource UV 映射到 mesh ``rest_points``。
* 为每个 segment 绑定正确的 image source。

它不应该负责：

* primitive 几何。
* action 播放。
* mesh cell 光栅化。

``gfx_mesh_img.c`` 是通用的可变形图片 widget。

它负责：

* 当前 mesh grid 尺寸和 point count。
* ``points``：当前 object-local 的 mesh 几何坐标。
* ``rest_points``：源图片采样坐标，也就是 UV/reference points。
* source image descriptor 和解码后的 image header。
* 根据当前 ``points`` 计算 object bounds。
* mesh 选项：``wrap_cols``、``aa_inward``、``opacity``、control point debug drawing、``scanline_fill``。
* 将 mesh cell 拆成三角形绘制，或对 solid fill 使用 scanline polygon fill。

它不应该负责：

* motion scene 的语义。
* capsule、ring、Bezier 等 segment kind。
* action 播放或 pose 插值。

``gfx_blend.c`` 是软件绘制后端。

它负责：

* 带 UV 的图片三角形绘制。
* 多边形填充 coverage 计算。
* buffer area 和 clip area 裁剪。
* alpha blend 和 RGB565 byte swap。
* primitive 边缘的抗锯齿策略。
* 对超宽 polygon fill 按 X 方向分块，避免大形状因为 coverage buffer 上限直接不绘制。

它不应该负责：

* widget 状态。
* mesh object layout。
* motion 专用假设。

Scene Asset 模型
----------------

``gfx_motion_asset_t`` 是 ROM 侧的 scene bundle，由运行时消费，定义在 ``include/widget/gfx_motion_scene.h``。它包含：

* ``meta``：schema version 和设计空间 viewbox。
* ``joint_names`` / ``joint_count``：命名控制点。
* ``segments``：引用 joints 的可视 primitive。
* ``poses``：完整的 joint 坐标快照。
* ``actions``：由多个 step 组成的动作序列，每个 step 指向一个 target pose。
* ``sequence``：可选的默认播放序列。
* ``layout``：默认 stroke、mirror axis、timer period、damping 等 hint。
* ``resources``：可选纹理图片及 UV crop。
* ``color_palette``：可选固定 segment 颜色。

scene 层会提前校验这些结构不变量：

* viewbox 宽高必须为正数。
* joint、pose、action、sequence 的 pointer 必须和 count 匹配。
* segment 引用的 joint 范围必须在 ``joint_count`` 内。
* Bezier 控制点数量必须满足 ``3k + 1``。
* resource index 和 palette index 必须能解析到有效条目。
* resource UV crop 必须落在 image descriptor 范围内。
* layer bit 必须在 32-bit layer mask 可表达范围内。

播放模型
--------

``gfx_motion_scene_init()`` 负责校验 asset 并初始化第一个 action step。初始化后，``pose_cur`` 会直接 snap 到 ``pose_tgt``。

``gfx_motion_scene_advance()`` 负责推进 action timeline。当 ``hold_ticks`` 到期时，它切到下一个 step，加载新的 target pose，并应用该 step 的插值策略。

``gfx_motion_scene_tick()`` 负责将 ``pose_cur`` 向 ``pose_tgt`` 推进。对 ``DAMPED`` step，它会做 easing；函数返回坐标是否发生变化。player 会结合这个返回值和 dirty flag 判断是否需要更新 mesh 对象。

``GFX_MOTION_INTERP_HOLD`` 表示立即 snap 到 target pose。它在 init、action switch、step advance 时都应该生效。

Player Segment 管线
-------------------

``gfx_motion_player_init()`` 会为每个 segment 创建一个 ``gfx_mesh_img`` 对象。初始 grid 由 segment kind 决定：

* Capsule：``1 x 1`` grid，共 4 个点。
* Ring：``N x 1`` wrapped grid，两行点，分别表示外圈和内圈。
* Bezier strip：按曲线采样生成列，不 wrap。
* Bezier loop：按曲线采样生成列，并启用 wrap。
* Bezier fill：使用 eye/ellipse preset grid，或 generic closed-loop rim grid。

每次 motion apply callback 中，player 会：

1. 检查 scene 或 mesh 是否 dirty。
2. 将当前 segment 需要的 joints 从设计坐标转换到屏幕坐标。
3. 根据 canvas scale 计算 stroke width 和 radius。
4. 按 segment kind 调用对应 primitive apply helper，更新 mesh points。
5. 根据 layer mask 设置 object visible。
6. 所有可见 segment 更新完成后清理 dirty flag。

``gfx_motion_primitives.c`` 中的 primitive 转换细节：

* Capsule 根据两个端点和 stroke width 生成一个沿线段方向的厚矩形。
* Ring 生成外圈和内圈两行圆形采样点，并启用 column wrap。
* Bezier stroke 计算 cubic 位置和解析 tangent，再沿左右法线挤出成两行 mesh。
* Bezier fill 对 eye/ellipse 使用 preset path；对 generic closed loop 构建 hub/rim mesh。

样式与资源
----------

``gfx_motion_style.c`` 按下面优先级绑定 image source：

1. ``resource_idx``：纹理图片。
2. ``color_idx``：palette 生成的 1x1 图片。
3. runtime solid 1x1 图片。

对 texture resource，``uv_x``、``uv_y``、``uv_w``、``uv_h`` 会映射到 mesh 的 ``rest_points``。mesh 当前 ``points`` 仍然表示屏幕几何；``rest_points`` 表示源图片采样坐标。这样 UV crop 逻辑保持通用，同一个 mesh renderer 可以同时绘制整图纹理和裁剪后的 resource segment。

对 palette color 和 runtime solid color，source 是一个 1x1 RGB565 image。Bezier fill segment 还可以启用 scanline fill，让 solid closed shape 不必通过 textured triangles 光栅化。

Mesh Image 模型
---------------

``gfx_mesh_img`` 维护两组点：

* ``points``：object-local Q8 几何坐标。
* ``rest_points``：object-local Q8 源图片采样坐标。

普通图片中，这两组点初始都是覆盖整张图片的规则 grid。motion segment 中，player 会持续更新 ``points`` 来改变屏幕形状，而 ``rest_points`` 保持为源 UV reference。texture crop 只更新 ``rest_points``。

当 ``points`` 改变时，``gfx_mesh_img_update_bounds()`` 会重新计算 object bounding box。draw origin 由 object position 减去 mesh 最小 bound 得出，因此 mesh 可以有负的 local coordinate，同时仍然走正常 object geometry 系统绘制。

重要 mesh 选项：

* ``wrap_cols``：把最后一列和第一列连起来。ring 和 closed Bezier loop 需要它。
* ``aa_inward``：让边缘 AA 向内衰减，避免细 stroke 外侧出现 halo。
* ``scanline_fill``：在条件满足时绕过 textured triangle drawing，直接绘制 solid polygon。
* ``opacity``：应用 segment 级整体透明度。

绘制管线
--------

``gfx_mesh_img_draw()`` 会打开 image decoder，解析 RGB565 或 RGB565A8 payload，计算裁剪区域，然后选择两条路径之一。

Scanline fill 路径：

* 用于部分 solid filled polygon。
* 从 mesh points 构造 polygon。
* 调用 ``gfx_sw_blend_polygon_fill()``。
* 如果 scanline scratch capacity 不够，会 fallback 到 triangle drawing，而不是直接空白。

Triangle 路径：

* 遍历每个 mesh cell。
* 用屏幕坐标和 source UV 构造四个 vertex。
* 将 quad 拆成两个 triangle。
* 选择较短对角线，减少变形 quad 上的裂缝。
* 标记内部边，避免 shared edge AA 产生深色缝。
* 每个 cell 调用两次 ``gfx_sw_blend_img_triangle_draw()``。

底层绘制
--------

``gfx_sw_blend_img_triangle_draw()`` 在 triangle 内采样源图片并混合到目标 buffer。它处理：

* 屏幕裁剪。
* source UV 插值。
* RGB565/RGB565A8 source alpha。
* uniform opacity。
* internal edge suppression。
* 可选 inward AA。

``gfx_sw_blend_polygon_fill()`` 用 solid color 填充 polygon。它负责裁剪、per-pixel coverage 计算，并对超宽 polygon 按 X 方向分块，保证 coverage scratch memory 有上限。

模块划分评估
------------

当前模块划分已经按层拆开：

* ``scene.c`` 是纯 playback state。
* ``player.c`` 是 motion scene runtime orchestration。
* ``primitives.c`` 是 motion geometry generation。
* ``style.c`` 是 motion style/resource binding。
* ``mesh_img.c`` 是可复用 deformable image 基础设施。
* ``gfx_blend.c`` 是底层 rasterization。

后续主要观察 ``gfx_motion_primitives.c`` 的体积。如果继续增加新的 primitive family，先放在该文件中；等 primitive API 和边界稳定后，再按 primitive family 进一步拆分。

优化入口
--------

后续优化可以优先看这些点：

* Player dirty flags：pose 和 canvas 未变化时避免 apply mesh。
* Cached segment grids：热路径避免重复 realloc mesh points。
* Bezier sampling density：stroke 和 fill 分别调采样密度。
* Resource UV updates：只有 grid 或 resource crop 改变时才重算 rest points。
* Mesh bounds：保留 clamp warning，因为过大 bounds 往往暴露坐标 bug。
* Scanline fill：solid 大面积 fill 优先走 scanline；textured 或 unsupported 情况保留 triangle fallback。
* Blend chunk width：只有在 stack/static scratch 预算允许时才增大。
* Layer mask：如果未来 layer 很多，可以在 tessellation 前跳过隐藏 segment group。

测试 Checklist
--------------

改动这条链路时建议覆盖这些 case：

* 空 segment asset 可以安全 init/deinit。
* ``HOLD`` action 在 init、action switch、step advance 时都会立即 snap。
* palette segment 不会被 ``gfx_motion_player_set_color()`` 覆盖。
* texture segment 会尊重 resource UV crop。
* ring grid 动态变化后不会丢失 UV crop。
* layer mask 可以隐藏并恢复 segment。
* Bezier stroke 在急弯或短曲线下不出现 dashed/bowtie artifact。
* oversized scanline fill 会 fallback 到 triangle rendering，而不是绘制空白。
* wide polygon fill 会按 chunk 绘制，而不是提前 return。
* mesh allocation failure 尽量保留旧 grid 状态。
* bounds 超过 geometry range 时会 clamp 并打 log。

迭代规则
--------

后续修改建议遵守这些规则：

* action timeline 或 pose 行为放在 ``gfx_motion_scene.c``。
* segment-to-mesh conversion 放在 ``gfx_motion_primitives.c``。
* palette/resource/layer/opacity 逻辑放在 ``gfx_motion_style.c``。
* 通用 mesh storage、UV、bounds、draw dispatch 放在 ``gfx_mesh_img.c``。
* pixel coverage、sampling、AA、blend math 放在 ``gfx_blend.c``。
* ``include/widget/gfx_motion_scene.h`` 里的 public struct 尽量保持稳定，因为生成的 asset 依赖它们。
* asset 错误尽量在 scene 层校验，不要拖到 player 或 renderer 才失败。
* renderer fallback 要打 log，避免静默绘制空白。
