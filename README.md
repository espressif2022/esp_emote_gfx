<p align="center">
  <br>
</p>

<h1 align="center">ESP Emote GFX</h1>

<p align="center">
  <span>面向嵌入式小屏设备的轻量 UI 图形库</span>
  <br>
  <sub>Widgets · Text · Images · QR Codes · Animation · Motion Scenes</sub>
  <br>
  <br>
</p>

<p align="center">
  <a href="https://components.espressif.com/components/espressif2022/esp_emote_gfx">
    <img src="https://components.espressif.com/components/espressif2022/esp_emote_gfx/badge.svg" alt="Component Registry">
  </a>
  <a href="https://github.com/espressif2022/esp_emote_gfx/blob/main/LICENSE">
    <img src="https://img.shields.io/badge/license-Apache--2.0-blue" alt="License">
  </a>
  <img src="https://img.shields.io/badge/ESP--IDF-5.0%2B-red" alt="ESP-IDF 5.0+">
</p>

<p align="center">
  <img src="https://img.shields.io/badge/rendering-software-2f855a" alt="Software Rendering">
  <img src="https://img.shields.io/badge/target-small%20displays-0f766e" alt="Small Displays">
  <img src="https://img.shields.io/badge/motion-path%20driven-d97706" alt="Path Driven Motion">
</p>

<p align="center">
  <a href="https://espressif2022.github.io/esp_emote_gfx/zh_CN/index.html">中文文档</a> |
  <a href="https://espressif2022.github.io/esp_emote_gfx/en/index.html">English Docs</a> |
  <a href="https://components.espressif.com/components/espressif2022/esp_emote_gfx">Component Registry</a>
</p>

---

<p align="center">
  <strong>把嵌入式小屏 UI 里常见的显示对象、图像、文本、动画、二维码和 Motion 场景，收进一套轻量图形库。</strong>
</p>

<p align="center">
  适合资源受限但仍需要流畅动效、清晰文字和轻量交互的小屏产品。
</p>

## 功能框架

<p align="center">
  <img src="docs/_static/esp_emote_gfx_framework.svg" alt="ESP Emote GFX framework" width="920">
</p>

## 模块说明

<table>
  <tr>
    <td width="33%">
      <strong>基础控件</strong>
      <br>
      提供图片、文本、按钮、二维码、动画和 Motion 场景等常用 UI 元素。
    </td>
    <td width="33%">
      <strong>渲染与图像</strong>
      <br>
      覆盖软件绘制、图像资源、RGB565 / RGB565A8 数据，以及基于控制点的 mesh image 形变。
    </td>
    <td width="33%">
      <strong>文本与字体</strong>
      <br>
      支持 LVGL bitmap font 和 FreeType TTF/OTF 字体渲染。
    </td>
  </tr>
  <tr>
    <td width="33%">
      <strong>动画播放</strong>
      <br>
      负责 EAF 播放、分段控制、循环模式，以及 timer-driven 的状态更新。
    </td>
    <td width="33%">
      <strong>Motion 场景</strong>
      <br>
      面向路径驱动的角色、表情和交互动效，支持生成式 asset、pose/action 切换和颜色/纹理绑定。
    </td>
    <td width="33%">
      <strong>嵌入式集成</strong>
      <br>
      作为组件接入工程，连接显示刷新、输入、内存 buffer 和线程安全对象访问。
    </td>
  </tr>
</table>

## 文档

详细安装、API、示例、Motion 架构和测试工程说明都放在在线文档里：

- 中文文档：<https://espressif2022.github.io/esp_emote_gfx/zh_CN/index.html>
- English docs: <https://espressif2022.github.io/esp_emote_gfx/en/index.html>
- Component Registry: <https://components.espressif.com/components/espressif2022/esp_emote_gfx>

## English

ESP Emote GFX is a lightweight software-rendered graphics library for compact embedded displays that need expressive UI elements without pulling in a heavy graphics stack.

For installation, API references, examples, and motion architecture notes, please visit the online documentation:

- Documentation: <https://espressif2022.github.io/esp_emote_gfx/en/index.html>
- Component Registry: <https://components.espressif.com/components/espressif2022/esp_emote_gfx>

## License

ESP Emote GFX is licensed under the Apache License 2.0. See [LICENSE](LICENSE).
