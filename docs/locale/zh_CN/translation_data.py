# -*- coding: utf-8 -*-
"""Chinese (zh_CN) msgstr for Sphinx gettext catalogs. Keys must match .pot msgid exactly."""

# api: leave untranslated (empty dict) — HTML shows English for reference pages.
TRANSLATIONS_API: dict[str, str] = {}

TRANSLATIONS_INDEX: dict[str, str] = {
    "Contents:": "目录：",
    "ESP Emote GFX Documentation": "ESP Emote GFX 文档",
    "Welcome to the ESP Emote GFX API documentation. This is a lightweight graphics framework for ESP-IDF with support for images, labels, animations, buttons, QR codes, and fonts.":
        "欢迎使用 ESP Emote GFX API 文档。这是一个面向 ESP-IDF 的轻量级图形框架，支持图像、标签、动画、按钮、二维码与字体。",
    "Overview": "概览",
    "ESP Emote GFX is a graphics framework designed for embedded systems, providing:":
        "ESP Emote GFX 面向嵌入式系统，提供以下能力：",
    "**Images**: Display images in RGB565A8 format with alpha transparency":
        "**图像**：以 RGB565A8 格式显示图像并支持 Alpha 透明",
    "**Animations**: GIF animations with ESP32 tools (EAF format)":
        "**动画**：配合 ESP32 工具使用 EAF 等格式",
    "**Buttons**: Interactive button widgets with text, border, and pressed-state styling":
        "**按钮**：可交互按钮组件，支持文本、边框与按下态样式",
    "**Fonts**: LVGL fonts and FreeType TTF/OTF support":
        "**字体**：LVGL 字体与 FreeType TTF/OTF",
    "**Timers**: Built-in timing system for smooth animations":
        "**定时器**：内置时序系统，用于平滑动画",
    "**Memory Optimized**: Designed for embedded systems with limited resources":
        "**内存优化**：面向资源受限的嵌入式场景",
    "Features": "特性",
    "Lightweight and memory-efficient": "轻量且节省内存",
    "Thread-safe operations with mutex locking": "通过互斥锁实现线程安全操作",
    "Support for multiple object types (images, labels, animations, buttons, QR codes)":
        "支持多种对象类型（图像、标签、动画、按钮、二维码）",
    "Flexible buffer management (internal or external buffers)": "灵活的缓冲区管理（内部或外部）",
    "Rich text rendering with scrolling and wrapping": "富文本渲染，支持滚动与换行",
    "Animation playback control with segments and loops": "动画分段与循环播放控制",
    "Quick Links": "快速链接",
    ":doc:`Quick Start Guide <quickstart>`": ":doc:`快速入门 <quickstart>`",
    ":doc:`Core API Reference <api/core/index>`": ":doc:`Core API 参考 <api/core/index>`",
    ":doc:`Widget API Reference <api/widgets/index>`": ":doc:`Widget API 参考 <api/widgets/index>`",
    ":doc:`Examples <examples>`": ":doc:`示例 <examples>`",
    "`Doxygen API Reference <../doxygen/index.html>`_ - Auto-generated C/C++ API documentation":
        "`Doxygen API 参考 <../doxygen/index.html>`_ — 自动生成的 C/C++ API 文档",
    "Indices and tables": "索引与表格",
    ":ref:`genindex`": ":ref:`genindex`",
    ":ref:`modindex`": ":ref:`modindex`",
    ":ref:`search`": ":ref:`search`",
}

TRANSLATIONS_QUICKSTART: dict[str, str] = {
    "Quick Start Guide": "快速入门",
    "This guide will help you get started with ESP Emote GFX in just a few steps.":
        "本指南帮助你在几个步骤内上手 ESP Emote GFX。",
    "Installation": "安装",
    "Add ESP Emote GFX to your ESP-IDF project by including it as a component. The component is available through the ESP Component Registry.":
        "将 ESP Emote GFX 作为组件加入 ESP-IDF 工程；组件可在 ESP 组件注册表中获取。",
    "Basic Setup": "基础设置",
    "Include the main header:": "包含主头文件：",
    "Initialize the graphics core (no display yet):": "初始化图形核心（尚未添加显示）：",
    "Add a display with a flush callback:": "添加显示设备并设置 flush 回调：",
    "(Optional) Register panel IO callback so the framework knows when flush is done:":
        "（可选）注册 panel IO 回调，以便框架获知 flush 完成：",
    "(Optional) Add touch input:": "（可选）添加触摸输入：",
    "Creating Your First Widget": "创建第一个组件",
    "Widgets are created on a **display** (``gfx_disp_t *``), not on the handle.":
        "组件创建在 display（``gfx_disp_t *``）上，而不是在 handle 上。",
    "Creating a Label": "创建标签",
    "Creating an Image": "创建图像",
    "Creating an Animation": "创建动画",
    "Object touch callback (e.g. drag)": "对象触摸回调（例如拖拽）",
    "Thread Safety": "线程安全",
    "When modifying objects from outside the graphics task, use the graphics lock:":
        "在图形任务之外修改对象时，请使用图形锁：",
    "Complete Example": "完整示例",
    "Next Steps": "下一步",
    "Read the :doc:`Core API Reference <api/core/index>` for detailed API documentation":
        "阅读 :doc:`Core API 参考 <api/core/index>` 获取完整 API 说明",
    "Check out the :doc:`Widget API Reference <api/widgets/index>` for widget-specific functions":
        "查看 :doc:`Widget API 参考 <api/widgets/index>` 了解各组件接口",
    "See :doc:`Examples <examples>` for more complex usage patterns":
        "参阅 :doc:`示例 <examples>` 获取更多用法",
}

TRANSLATIONS_EXAMPLES: dict[str, str] = {
    "Examples": "示例",
    "This section provides comprehensive examples demonstrating various features of ESP Emote GFX.":
        "本节提供 ESP Emote GFX 各项能力的示例代码。",
    "Initialization (core + display + optional touch)": "初始化（核心 + 显示 + 可选触摸）",
    "Initialize the graphics core, add a display with flush callback, and optionally add touch. Widgets are created on the display (``gfx_disp_t *disp``).":
        "初始化图形核心，添加带 flush 回调的显示设备，并可选择添加触摸。组件创建在显示（``gfx_disp_t *disp``）上。",
    "Basic Examples": "基础示例",
    "Simple Label": "简单标签",
    "Create and display a simple text label on a display (``disp`` from ``gfx_disp_add``):":
        "在显示上创建并显示简单文本标签（``disp`` 来自 ``gfx_disp_add``）：",
    "Image Display": "图像显示",
    "Display an image:": "显示图像：",
    "Advanced Examples": "进阶示例",
    "Multiple Widgets": "多组件",
    "Create and manage multiple widgets on the same display:":
        "在同一显示上创建并管理多个组件：",
    "Touch and object callback (e.g. drag)": "触摸与对象回调（例如拖拽）",
    "Register a per-object touch callback so the object receives PRESS/MOVE/RELEASE (e.g. for dragging):":
        "注册逐对象的触摸回调，使对象接收 PRESS/MOVE/RELEASE（用于拖拽等）：",
    "Text Scrolling": "文本滚动",
    "Create a scrolling text label (see widget API for ``gfx_label_set_long_mode``, ``gfx_label_set_scroll_speed``, etc.):":
        "创建可滚动文本标签（详见 Widget API 中的 ``gfx_label_set_long_mode``、``gfx_label_set_scroll_speed`` 等）：",
    "Timer-Based Updates": "基于定时器的更新",
    "Use the graphics timer to update widgets periodically. Timers are created on the **handle**:":
        "使用图形定时器周期性更新组件。定时器创建在 **handle** 上：",
    "QR Code Generation": "二维码生成",
    "Generate and display a QR code:": "生成并显示二维码：",
    "Thread-Safe Operations": "线程安全操作",
    "When modifying widgets from another task, always use the graphics lock (on the **handle**):":
        "从其他任务修改组件时，务必使用图形锁（在 **handle** 上）：",
    "Complete Application Example": "完整应用示例",
    "Initialization (core + one display), then create a label and refresh:":
        "初始化（核心 + 单个显示），再创建标签并刷新：",
    "For more examples, see the test applications in the ``test_apps/`` directory.":
        "更多示例见 ``test_apps/`` 目录中的测试应用。",
}

TRANSLATIONS_OVERVIEW: dict[str, str] = {
    "Overview": "概览",
    "ESP Emote GFX is a lightweight graphics framework for ESP-IDF that provides a simple yet powerful API for rendering graphics on embedded displays. It is designed with memory efficiency and performance in mind, making it ideal for resource-constrained embedded systems.":
        "ESP Emote GFX 是面向 ESP-IDF 的轻量级图形框架，提供简洁而强大的嵌入式显示渲染 API，在内存与性能之间取得平衡，适合资源受限场景。",
    "Architecture": "架构",
    "The framework is built around a core object system where all graphical elements (images, labels, animations, buttons, QR codes) are treated as objects. These objects share common properties like position, size, visibility, and alignment.":
        "框架以统一对象系统为核心：图像、标签、动画、按钮、二维码等元素均为对象，共享位置、尺寸、可见性与对齐等属性。",
    "Core Components": "核心组件",
    "Core System": "核心系统",
    "The core system (`gfx_core`) manages:": "核心系统（`gfx_core`）负责：",
    "Graphics context initialization and deinitialization": "图形上下文的初始化与反初始化",
    "Buffer management (internal or external)": "缓冲区管理（内部或外部）",
    "Rendering pipeline": "渲染管线",
    "Thread safety with mutex locking": "基于互斥锁的线程安全",
    "Screen refresh and invalidation": "屏幕刷新与脏区失效",
    "Object System": "对象系统",
    "The object system (`gfx_obj`) provides:": "对象系统（`gfx_obj`）提供：",
    "Base object structure for all graphical elements": "所有图形元素的基类结构",
    "Position and size management": "位置与尺寸管理",
    "Alignment system (similar to LVGL)": "对齐系统（类似 LVGL）",
    "Visibility control": "可见性控制",
    "Object lifecycle management": "对象生命周期管理",
    "Timer System": "定时器系统",
    "The timer system (`gfx_timer`) provides:": "定时器系统（`gfx_timer`）提供：",
    "High-resolution timers for animations": "用于动画的高分辨率定时器",
    "Callback-based timer events": "基于回调的定时事件",
    "Repeat count and period control": "重复次数与周期控制",
    "System tick management": "系统 tick 管理",
    "Widgets": "组件",
    "Image Widget": "图像组件",
    "The image widget supports:": "图像组件支持：",
    "RGB565 format (16-bit color)": "RGB565（16 位色）",
    "RGB565A8 format (16-bit color with 8-bit alpha)": "RGB565A8（16 位色 + 8 位 Alpha）",
    "C array and binary formats": "C 数组与二进制格式",
    "Automatic format detection": "自动识别格式",
    "Label Widget": "标签组件",
    "The label widget provides:": "标签组件提供：",
    "Text rendering with multiple font formats": "多种字体格式的文本渲染",
    "LVGL font support": "LVGL 字体支持",
    "FreeType TTF/OTF font support": "FreeType TTF/OTF 支持",
    "Text alignment (left, center, right)": "文本对齐（左、中、右）",
    "Long text handling (wrap, scroll, clip)": "长文本处理（换行、滚动、裁剪）",
    "Background colors and opacity": "背景色与透明度",
    "Button Widget": "按钮组件",
    "The button widget provides:": "按钮组件提供：",
    "Text label management": "文本标签管理",
    "Normal and pressed background colors": "常态与按下背景色",
    "Border color and width configuration": "边框颜色与宽度",
    "Font and text alignment control": "字体与文本对齐控制",
    "Animation Widget": "动画组件",
    "The animation widget supports:": "动画组件支持：",
    "EAF (ESP Animation Format) files": "EAF（ESP 动画格式）文件",
    "Frame-by-frame playback control": "逐帧播放控制",
    "Segment playback (start/end frames)": "分段播放（起止帧）",
    "FPS control": "帧率控制",
    "Loop and repeat options": "循环与重复选项",
    "Mirror effects": "镜像效果",
    "QR Code Widget": "二维码组件",
    "The QR code widget provides:": "二维码组件提供：",
    "Dynamic QR code generation": "动态生成二维码",
    "Configurable size and error correction": "可配置尺寸与纠错等级",
    "Custom foreground and background colors": "自定义前景与背景色",
    "Memory Management": "内存管理",
    "The framework supports two buffer management modes:": "框架支持两种缓冲区管理模式：",
    "Internal Buffers": "内部缓冲区",
    "The framework automatically allocates and manages frame buffers internally. This is the simplest mode but requires sufficient heap memory.":
        "由框架在内部分配并管理帧缓冲。最简单，但需要足够的堆内存。",
    "External Buffers": "外部缓冲区",
    "You can provide your own buffers, allowing you to:": "你可自行提供缓冲区，从而可以：",
    "Use memory-mapped regions": "使用内存映射区域",
    "Control buffer placement (SRAM, SPIRAM, etc.)": "控制缓冲区位置（SRAM、SPIRAM 等）",
    "Optimize for specific memory constraints": "针对内存约束优化",
    "Thread Safety": "线程安全",
    "All widget operations should be performed within a graphics lock to ensure thread safety:":
        "所有组件操作应在图形锁内执行以保证线程安全：",
    "Dependencies": "依赖",
    "ESP-IDF 5.0 or higher": "ESP-IDF 5.0 或更高版本",
    "FreeType (for TTF/OTF font support)": "FreeType（TTF/OTF 字体）",
    "ESP New JPEG (for JPEG decoding)": "ESP New JPEG（JPEG 解码）",
    "License": "许可证",
    "This project is licensed under the Apache License 2.0.": "本项目采用 Apache License 2.0 许可证。",
}

TRANSLATIONS_CHANGELOG: dict[str, str] = {
    "Changelog": "更新日志",
    "All notable changes to the ESP Emote GFX component will be documented in this file.":
        "ESP Emote GFX 组件的重要变更将记录于此。",
    "[3.0.3] - 2026-04-20": "[3.0.3] - 2026-04-20",
    "Add `gfx_button` widget (text, font, normal/pressed colors, border)":
        "新增 `gfx_button` 组件（文本、字体、常态/按下背景色、边框）",
    "Add `gfx_log` API for log level configuration": "新增 `gfx_log` API，用于配置日志级别",
    "Documentation: separate English and Simplified Chinese HTML builds (gettext), language switcher, unified `postprocess_docs.sh` pipeline (API RST, Sphinx, Doxygen)":
        "文档：英文与简体中文独立 HTML 构建（gettext）、页顶语言切换、统一 `postprocess_docs.sh` 流程（API RST、Sphinx、Doxygen）",
    "Simplify GitHub Actions documentation job to a single build step":
        "精简 GitHub Actions 文档构建为单一步骤",
    "[3.0.2] - 2026-04-17": "[3.0.2] - 2026-04-17",
    "Update version of esp_new_jpeg": "更新 esp_new_jpeg 版本",
    "[3.0.1] - 2026-02-13": "[3.0.1] - 2026-02-13",
    "Add CI build action for P4": "为 P4 添加 CI 构建",
    "Optimize multi-buffer switching logic": "优化多缓冲切换逻辑",
    "Fix crash when text is NULL": "修复 text 为 NULL 时的崩溃",
    "Fix missing API documentation (e.g. gfx_touch_add)": "补全缺失的 API 文档（如 gfx_touch_add）",
    "[3.0.0] - 2026-01-22": "[3.0.0] - 2026-01-22",
    "Add documentation build action": "添加文档构建 Action",
    "Optimize EAF 8-bit render": "优化 EAF 8 位渲染",
    "Fix FreeType parsing performance": "修复 FreeType 解析性能",
    "Remove duplicated label-related APIs": "移除重复的标签相关 API",
    "[2.1.0] - 2026-01-28": "[2.1.0] - 2026-01-28",
    "Support for decoding Heatshrink-compressed image slices": "支持解码 Heatshrink 压缩的图像条带",
    "[2.0.4] - 2026-01-22": "[2.0.4] - 2026-01-22",
    "Fix Huffman+RLE decoding buffer sizing to prevent oversized output errors (Issue `#18 <https://github.com/espressif2022/esp_emote_gfx/issues/18>`_)":
        "修复 Huffman+RLE 解码缓冲区尺寸，避免输出过大错误（Issue `#18 <https://github.com/espressif2022/esp_emote_gfx/issues/18>`_）",
    "[2.0.3] - 2026-01-08": "[2.0.3] - 2026-01-08",
    "Delete local assets": "删除本地资源",
    "Build acion for ['release-v5.2', 'release-v5.3', 'release-v5.4', 'release-v5.5']":
        "为 ['release-v5.2', 'release-v5.3', 'release-v5.4', 'release-v5.5'] 构建 Action",
    "Fix ESP-IDF version compatibility issues": "修复 ESP-IDF 版本兼容问题",
    "Change flush_callback timeout from 20 ms to wait forever": "将 flush_callback 超时从 20 ms 改为无限等待",
    "[2.0.2] - 2025-12-26": "[2.0.2] - 2025-12-26",
    "Add optional JPEG decoding support for EAF animations": "为 EAF 动画增加可选 JPEG 解码",
    "Center QR code rendering in UI layout": "在界面布局中居中渲染二维码",
    "Add alpha channel support for animations": "为动画增加 Alpha 通道支持",
    "[2.0.1] - 2025-12-05": "[2.0.1] - 2025-12-05",
    "Add Touch event": "增加触摸事件",
    "[2.0.0] - 2025-12-01": "[2.0.0] - 2025-12-01",
    "Added partial refresh mode support": "增加局部刷新模式",
    "Added QR code widget (gfx_qrcode)": "增加二维码组件 (gfx_qrcode)",
    "[1.2.0] - 2025-09-0": "[1.2.0] - 2025-09-0",
    "use eaf as a lib": "将 eaf 作为库使用",
    "[1.1.2] - 2025-09-29": "[1.1.2] - 2025-09-29",
    "Upgrade dependencies": "升级依赖",
    "Update `espressif/esp_new_jpeg` to 0.6.x by @Kevincoooool. `#8 <https://github.com/espressif2022/esp_emote_gfx/pull/8>`_":
        "将 `espressif/esp_new_jpeg` 升级到 0.6.x（@Kevincoooool）。`#8 <https://github.com/espressif2022/esp_emote_gfx/pull/8>`_",
    "[1.1.1] - 2025-09-23": "[1.1.1] - 2025-09-23",
    "Fixed": "修复",
    "Resolve image block decoding failure in specific cases. `#6 <https://github.com/espressif2022/esp_emote_gfx/issues/6>`_":
        "解决特定场景下图块解码失败。`#6 <https://github.com/espressif2022/esp_emote_gfx/issues/6>`_",
    "[1.0.0] - 2025-08-01": "[1.0.0] - 2025-08-01",
    "Added": "新增",
    "Initial release of ESP Emote GFX framework": "ESP Emote GFX 框架首次发布",
    "Core graphics rendering engine": "核心图形渲染引擎",
    "Object system for images and labels": "图像与标签对象系统",
    "Basic drawing functions and color utilities": "基础绘制与颜色工具",
    "Software blending capabilities": "软件混合能力",
    "Timer system for animations": "用于动画的定时器系统",
    "Support for ESP-IDF 5.0+": "支持 ESP-IDF 5.0+",
    "FreeType font rendering integration": "集成 FreeType 字体渲染",
    "JPEG image decoding support": "JPEG 图像解码支持",
    "Features": "特性",
    "Lightweight graphics framework optimized for embedded systems": "面向嵌入式系统的轻量图形框架",
    "Memory-efficient design for resource-constrained environments": "面向资源受限环境的省内存设计",
}

TRANSLATIONS_BY_CATALOG: dict[str, dict[str, str]] = {
    "index": TRANSLATIONS_INDEX,
    "overview": TRANSLATIONS_OVERVIEW,
    "quickstart": TRANSLATIONS_QUICKSTART,
    "examples": TRANSLATIONS_EXAMPLES,
    "changelog": TRANSLATIONS_CHANGELOG,
    "api": TRANSLATIONS_API,
}
