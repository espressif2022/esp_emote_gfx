# Arena ABI v1（ARN1）

> 冻结文档。不兼容改动必须升高 `ARENA_VERSION`。  
> 头文件：`include/gfx/scene/arena.h`

## 总则

- 小端、`#pragma pack(1)`、包内**无原生指针**（仅 u32 偏移 / 定宽字段）
- Host 64 / 设备 32 共用同一字节包
- `arena_load`：校验后 **memcpy 到 RAM**；原地改只碰 RAM 副本

## Header（24 B）

| 字段 | 类型 | 说明 |
|---|---|---|
| magic | u32 | `0x314E5241`（`'ARN1'` LE） |
| version | u16 | `1` |
| node_count | u16 | 节点数 |
| nodes_off | u32 | 节点表字节偏移 |
| str_off | u32 | 字符串表字节偏移 |
| total_size | u32 | 整包字节数（须等于加载长度） |
| root_off | u32 | 根节点偏移；`0xFFFFFFFF` = 无 |

## Node（32 B）

| 字段 | 类型 | 说明 |
|---|---|---|
| type | u16 | 1=container 2=label 3=button 4=image 5=list 6=wheel |
| flags | u16 | 见下 |
| x,y | i16 | 相对父节点 |
| w,h | u16 | 尺寸 |
| bg_rgb | u32 | RGB888（低 24 bit）；LABEL=字色；LIST/WHEEL=面板底 |
| name_off | u32 | 名字/文案字符串偏移；0=无 |
| first_child | u32 | 首子；`0xFFFFFFFF`=无 |
| next_sibling | u32 | 下一兄弟 |
| reserved | u32 | BUTTON: action 名偏移；IMAGE: `arena_img_hdr_t`；LIST/WHEEL: `arena_items_hdr_t` |

### Flags

| Bit | 宏 | 含义 |
|---|---|---|
| 0 | `ARENA_F_VISIBLE` | 可见 |
| 1 | `ARENA_F_BG` | 画实心底 |
| 2 | `ARENA_F_CLICKABLE` | 可命中 |
| 3 | `ARENA_F_PRESSED` | 按下态（运行时） |

## Image blob

布局（在字符串表之后）：

```text
arena_img_hdr_t { u16 w, h, format, pad }  // format=0 RGB565
u16 pixels[w * h]
```

节点 `reserved` 指向 `arena_img_hdr_t`；绘制取 `min(node.w,img.w) × min(node.h,img.h)`。

## List / Wheel items blob

```text
arena_items_hdr_t {
  u16 item_count
  u16 selected          // 0xFFFF = none；RAM 可改
  u16 item_height       // 0 = 默认 24
  u16 flags             // bit0 CYCLIC (wheel)
  u32 text_rgb
}
repeated item_count:
  u16 byte_len + UTF-8 bytes   // 无强制 NUL
```

直画：可见行 + 选中高亮；点击改 `selected`（无惯性滚动）。

## v1 控件子集

| 已支持（直画） | 约定 | 暂不迁 |
|---|---|---|
| container 底色 | `ARENA_F_BG` + `bg_rgb` | motion / anim |
| label 文本 | `name_off`=文案，`bg_rgb`=字色；需 scene font | JPEG/缩放 image |
| button | 圆角底 + `name_off` 居中白字；`reserved`=action | 边框/图标按钮 |
| image | RGB565 blob | |
| list / wheel | items blob；点击选中 | 惯性滚动 / snap 动画 |

详见 [`ARENA_PARITY.md`](ARENA_PARITY.md)。

## 双后端

| 路径 | 模型 |
|---|---|
| 场景包 | arena + `arena_scene_*` |
| 手写 UI | `gfx_*_create` / `gfx_object_t` |

脏区：`arena_scene_mark_dirty` → `gfx_invalidate_area_disp` → 现有 merge / 局部刷新。
