const widgetDefinitions = {
  label: {
    label: "Label",
    api: "gfx_label_*",
    defaultSize: { w: 160, h: 52 },
    defaults: {
      text: "Status: Ready",
      textColor: "#e2e8f0",
      bgColor: "#1e293b",
      bgEnabled: true,
      textAlign: "left",
      font: "font_puhui_16_4"
    },
    fields: [
      { key: "text", label: "Text", type: "text" },
      { key: "font", label: "Font Symbol", type: "text" },
      { key: "textColor", label: "Text Color", type: "color" },
      { key: "bgColor", label: "Background", type: "color" },
      { key: "bgEnabled", label: "Background Enabled", type: "checkbox" },
      {
        key: "textAlign",
        label: "Text Align",
        type: "select",
        options: ["left", "center", "right"]
      }
    ]
  },
  button: {
    label: "Button",
    api: "gfx_button_*",
    defaultSize: { w: 148, h: 44 },
    defaults: {
      text: "Apply",
      textColor: "#eff6ff",
      bgColor: "#2563eb",
      bgColorPressed: "#1d4ed8",
      borderColor: "#93c5fd",
      borderWidth: 1,
      textAlign: "center"
    },
    fields: [
      { key: "text", label: "Text", type: "text" },
      { key: "textColor", label: "Text Color", type: "color" },
      { key: "bgColor", label: "Background", type: "color" },
      { key: "bgColorPressed", label: "Pressed Background", type: "color" },
      { key: "borderColor", label: "Border Color", type: "color" },
      { key: "borderWidth", label: "Border Width", type: "number", min: 0, max: 24 },
      {
        key: "textAlign",
        label: "Text Align",
        type: "select",
        options: ["left", "center", "right"]
      }
    ]
  },
  list: {
    label: "List",
    api: "gfx_list_*",
    defaultSize: { w: 180, h: 120 },
    defaults: {
      items: "Speed\nBrightness\nVolume",
      selectedIndex: 1,
      textColor: "#e2e8f0",
      bgColor: "#111827",
      selectedBgColor: "#1d4ed8",
      borderColor: "#475569",
      borderWidth: 1,
      rowHeight: 28
    },
    fields: [
      { key: "items", label: "Items", type: "textarea" },
      { key: "selectedIndex", label: "Selected Index", type: "number", min: 0, max: 64 },
      { key: "textColor", label: "Text Color", type: "color" },
      { key: "bgColor", label: "Background", type: "color" },
      { key: "selectedBgColor", label: "Selected Background", type: "color" },
      { key: "borderColor", label: "Border Color", type: "color" },
      { key: "borderWidth", label: "Border Width", type: "number", min: 0, max: 24 },
      { key: "rowHeight", label: "Row Height", type: "number", min: 18, max: 80 }
    ]
  },
  image: {
    label: "Image",
    api: "gfx_img_*",
    defaultSize: { w: 112, h: 112 },
    defaults: {
      srcType: "image_dsc",
      src: "assets/icon.bin",
      fit: "contain",
      tint: "#94a3b8",
      opa: 255
    },
    fields: [
      {
        key: "srcType",
        label: "Source Type",
        type: "select",
        options: ["image_dsc", "memory", "file"]
      },
      { key: "src", label: "Source", type: "text" },
      { key: "tint", label: "Tint", type: "color" },
      { key: "opa", label: "Opacity", type: "number", min: 0, max: 255 },
      {
        key: "fit",
        label: "Fit",
        type: "select",
        options: ["contain", "cover", "stretch"]
      }
    ]
  },
  qrcode: {
    label: "QR Code",
    api: "gfx_qrcode_*",
    defaultSize: { w: 132, h: 132 },
    defaults: {
      data: "https://espressif.com",
      color: "#0f172a",
      bgColor: "#ffffff",
      ecc: "medium"
    },
    fields: [
      { key: "data", label: "Data", type: "text" },
      { key: "color", label: "Foreground", type: "color" },
      { key: "bgColor", label: "Background", type: "color" },
      {
        key: "ecc",
        label: "ECC",
        type: "select",
        options: ["low", "medium", "quartile", "high"]
      }
    ]
  },
  anim: {
    label: "Animation",
    api: "gfx_anim_*",
    defaultSize: { w: 144, h: 128 },
    defaults: {
      srcType: "memory",
      src: "assets/face.eaf",
      fps: 24,
      autoplay: true,
      autoMirror: false,
      playMode: "full",
      loopCount: 0
    },
    fields: [
      {
        key: "srcType",
        label: "Source Type",
        type: "select",
        options: ["memory", "file"]
      },
      { key: "src", label: "Source", type: "text" },
      { key: "fps", label: "FPS", type: "number", min: 1, max: 120 },
      { key: "autoplay", label: "Autoplay", type: "checkbox" },
      { key: "autoMirror", label: "Auto Mirror", type: "checkbox" },
      {
        key: "playMode",
        label: "Segment Mode",
        type: "select",
        options: ["full", "segmented"]
      },
      { key: "loopCount", label: "Loop Count", type: "number", min: 0, max: 255 }
    ]
  },
  mesh_img: {
    label: "Mesh Image",
    api: "gfx_mesh_img_*",
    defaultSize: { w: 152, h: 120 },
    defaults: {
      srcType: "image_dsc",
      src: "assets/mesh.bin",
      cols: 4,
      rows: 4,
      ctrlVisible: true,
      aaInward: true,
      wrapCols: false,
      scanlineFill: false,
      fillColor: "#0f172a"
    },
    fields: [
      {
        key: "srcType",
        label: "Source Type",
        type: "select",
        options: ["image_dsc", "memory", "file"]
      },
      { key: "src", label: "Source", type: "text" },
      { key: "cols", label: "Columns", type: "number", min: 2, max: 16 },
      { key: "rows", label: "Rows", type: "number", min: 2, max: 16 },
      { key: "ctrlVisible", label: "Control Points Visible", type: "checkbox" },
      { key: "aaInward", label: "Inward AA", type: "checkbox" },
      { key: "wrapCols", label: "Wrap Columns", type: "checkbox" },
      { key: "scanlineFill", label: "Scanline Fill", type: "checkbox" },
      { key: "fillColor", label: "Fill Color", type: "color" }
    ]
  },
  face_emote: {
    label: "Face Emote",
    api: "gfx_face_emote_*",
    defaultSize: { w: 176, h: 176 },
    defaults: {
      expressionName: "happy",
      color: "#0f172a",
      manualLook: false,
      lookX: 0,
      lookY: 0,
      eyeSegs: 24,
      browSegs: 16,
      mouthSegs: 32
    },
    fields: [
      { key: "expressionName", label: "Expression", type: "text" },
      { key: "color", label: "Stroke Color", type: "color" },
      { key: "manualLook", label: "Manual Look", type: "checkbox" },
      { key: "lookX", label: "Look X", type: "number", min: -100, max: 100 },
      { key: "lookY", label: "Look Y", type: "number", min: -100, max: 100 },
      { key: "eyeSegs", label: "Eye Segments", type: "number", min: 4, max: 64 },
      { key: "browSegs", label: "Brow Segments", type: "number", min: 4, max: 64 },
      { key: "mouthSegs", label: "Mouth Segments", type: "number", min: 4, max: 128 }
    ]
  }
};

const generalFields = ["name", "type", "x", "y", "w", "h", "visible"];

const state = createSampleScene();

const canvas = document.getElementById("canvas");
const canvasWrap = document.getElementById("canvas-wrap");
const palette = document.getElementById("palette");
const layerList = document.getElementById("layer-list");
const typeFields = document.getElementById("type-fields");
const inspectorForm = document.getElementById("inspector-form");
const inspectorEmpty = document.getElementById("inspector-empty");
const sceneJson = document.getElementById("scene-json");
const selectionBadge = document.getElementById("selection-badge");

const screenInputs = {
  width: document.getElementById("screen-width"),
  height: document.getElementById("screen-height"),
  bgColor: document.getElementById("screen-bg"),
  gridSize: document.getElementById("screen-grid")
};

let dragState = null;

bootstrap();

function bootstrap() {
  renderPalette();
  bindStaticActions();
  canvas.addEventListener("click", handleCanvasBackgroundClick);
  syncScreenInputs();
  renderAll();
  fitCanvasToViewport();
}

function createSampleScene() {
  const initial = {
    version: 1,
    screen: {
      width: 320,
      height: 240,
      bgColor: "#f8fafc",
      gridSize: 8
    },
    theme: {
      accent: "#2563eb",
      surface: "#0f172a",
      text: "#e2e8f0"
    },
    widgets: [],
    selectedId: null,
    nextId: 1
  };

  const title = createWidget("label", initial, {
    name: "Screen Title",
    x: 20,
    y: 18,
    w: 150,
    h: 46,
    props: {
      text: "Mood Console",
      textColor: "#eff6ff",
      bgColor: "#0f172a",
      bgEnabled: true,
      textAlign: "left"
    }
  });
  const face = createWidget("face_emote", initial, {
    name: "Assistant Face",
    x: 182,
    y: 24,
    w: 118,
    h: 152,
    props: {
      expressionName: "happy",
      color: "#111827",
      manualLook: true,
      lookX: 18,
      lookY: -8
    }
  });
  const list = createWidget("list", initial, {
    name: "Preset List",
    x: 20,
    y: 78,
    w: 120,
    h: 136,
    props: {
      items: "happy\nsurprise\nsleepy\nangry",
      selectedIndex: 0
    }
  });
  const mesh = createWidget("mesh_img", initial, {
    name: "Mouth Mesh",
    x: 152,
    y: 188,
    w: 148,
    h: 34,
    props: {
      src: "assets/mouth_mask.bin",
      cols: 8,
      rows: 1,
      aaInward: true,
      wrapCols: true,
      scanlineFill: true,
      fillColor: "#111827",
      ctrlVisible: false
    }
  });
  const button = createWidget("button", initial, {
    name: "Apply Expression",
    x: 20,
    y: 188,
    w: 120,
    h: 34,
    props: {
      text: "Apply",
      textColor: "#eff6ff",
      bgColor: "#2563eb",
      bgColorPressed: "#1d4ed8",
      borderColor: "#60a5fa",
      borderWidth: 1,
      textAlign: "center"
    }
  });

  initial.widgets.push(title, face, list, mesh, button);
  initial.selectedId = face.id;
  return initial;
}

function createWidget(type, scene, overrides = {}) {
  const def = widgetDefinitions[type];
  const size = overrides.w && overrides.h ? { w: overrides.w, h: overrides.h } : def.defaultSize;
  const id = `${type}_${scene.nextId++}`;

  return {
    id,
    type,
    name: overrides.name || `${def.label} ${scene.nextId - 1}`,
    x: overrides.x ?? 24,
    y: overrides.y ?? 24,
    w: size.w,
    h: size.h,
    visible: overrides.visible ?? true,
    zIndex: scene.widgets.length,
    props: { ...def.defaults, ...(overrides.props || {}) }
  };
}

function renderPalette() {
  const tpl = document.getElementById("palette-item-template");
  palette.innerHTML = "";

  Object.entries(widgetDefinitions).forEach(([type, def]) => {
    const node = tpl.content.firstElementChild.cloneNode(true);
    node.innerHTML = `<strong>${def.label}</strong><br><span>${def.api}</span>`;
    node.addEventListener("click", () => {
      const widget = createWidget(type, state, {
        x: 24 + (state.widgets.length % 4) * 18,
        y: 24 + (state.widgets.length % 5) * 18
      });
      state.widgets.push(widget);
      state.selectedId = widget.id;
      normalizeZOrder();
      renderAll();
    });
    palette.appendChild(node);
  });
}

function bindStaticActions() {
  Object.entries(screenInputs).forEach(([key, input]) => {
    input.addEventListener("input", () => {
      const raw = input.type === "color" ? input.value : Number(input.value);
      if (key === "bgColor") {
        state.screen[key] = raw;
      } else if (key === "gridSize") {
        state.screen[key] = clampNumber(raw, 1, 64);
      } else {
        state.screen[key] = clampNumber(raw, 64, 4096);
      }
      renderAll();
    });
  });

  document.querySelectorAll("[data-prop]").forEach((input) => {
    input.addEventListener("input", () => updateGeneralField(input));
    input.addEventListener("change", () => updateGeneralField(input));
  });

  document.getElementById("layer-up").addEventListener("click", () => moveSelectedLayer(1));
  document.getElementById("layer-down").addEventListener("click", () => moveSelectedLayer(-1));
  document.getElementById("duplicate-widget").addEventListener("click", duplicateSelectedWidget);
  document.getElementById("delete-widget").addEventListener("click", deleteSelectedWidget);
  document.getElementById("copy-json").addEventListener("click", copySceneJson);
  document.getElementById("import-json").addEventListener("click", importSceneJson);
  document.getElementById("reset-scene").addEventListener("click", resetScene);
  document.getElementById("fit-canvas").addEventListener("click", fitCanvasToViewport);

  window.addEventListener("resize", fitCanvasToViewport);
}

function renderAll() {
  normalizeZOrder();
  renderCanvas();
  renderLayers();
  renderInspector();
  renderJson();
  syncScreenInputs();
  fitCanvasToViewport();
}

function syncScreenInputs() {
  screenInputs.width.value = state.screen.width;
  screenInputs.height.value = state.screen.height;
  screenInputs.bgColor.value = normalizeColor(state.screen.bgColor);
  screenInputs.gridSize.value = state.screen.gridSize;
}

function renderCanvas() {
  canvas.innerHTML = "";
  canvas.style.width = `${state.screen.width}px`;
  canvas.style.height = `${state.screen.height}px`;
  canvas.style.background = state.screen.bgColor;
  canvas.style.backgroundImage = createGridPattern(state.screen.gridSize);
  canvas.style.backgroundSize = `${state.screen.gridSize}px ${state.screen.gridSize}px`;

  state.widgets
    .slice()
    .sort((a, b) => a.zIndex - b.zIndex)
    .forEach((widget) => {
      const el = document.createElement("div");
      el.className = "widget";
      if (!widget.visible) {
        el.classList.add("hidden-widget");
      }
      if (widget.id === state.selectedId) {
        el.classList.add("selected");
      }

      el.dataset.id = widget.id;
      el.style.left = `${widget.x}px`;
      el.style.top = `${widget.y}px`;
      el.style.width = `${widget.w}px`;
      el.style.height = `${widget.h}px`;
      el.style.zIndex = widget.zIndex + 1;
      applyWidgetStyles(el, widget);
      el.appendChild(renderWidgetBody(widget));

      el.addEventListener("pointerdown", startDrag);
      el.addEventListener("click", (event) => {
        event.stopPropagation();
        state.selectedId = widget.id;
        renderAll();
      });

      canvas.appendChild(el);
    });

  updateSelectionBadge();
}

function renderWidgetBody(widget) {
  const body = document.createElement("div");
  body.className = "widget-content";
  const meta = document.createElement("div");
  meta.className = "widget-meta";
  meta.textContent = widgetDefinitions[widget.type].api;
  body.appendChild(meta);

  if (widget.type === "list") {
    const list = document.createElement("div");
    list.className = "widget-list";
    widget.props.items.split("\n").filter(Boolean).forEach((item, index) => {
      const row = document.createElement("div");
      row.className = "widget-list-row";
      if (index === Number(widget.props.selectedIndex)) {
        row.classList.add("selected-row");
      }
      row.textContent = item;
      list.appendChild(row);
    });
    body.appendChild(list);
    return body;
  }

  if (widget.type === "qrcode") {
    const qr = document.createElement("div");
    qr.className = "widget-checker";
    qr.style.width = `${Math.max(48, widget.w - 18)}px`;
    qr.style.height = `${Math.max(48, widget.h - 18)}px`;
    qr.style.borderRadius = "12px";
    qr.style.border = "1px solid rgba(15, 23, 42, 0.15)";
    body.appendChild(qr);
    return body;
  }

  if (widget.type === "face_emote") {
    const face = document.createElement("div");
    face.className = "widget-face";
    face.innerHTML = `
      <div class="widget-face-brow left"></div>
      <div class="widget-face-brow right"></div>
      <div class="widget-face-eye left"></div>
      <div class="widget-face-eye right"></div>
      <div class="widget-face-mouth"></div>
    `;
    body.appendChild(face);
    return body;
  }

  const text = document.createElement("div");
  text.className = "widget-label";
  text.textContent = getWidgetPreviewText(widget);
  body.appendChild(text);
  return body;
}

function applyWidgetStyles(el, widget) {
  const props = widget.props;
  if (widget.type === "label") {
    el.style.background = props.bgEnabled ? props.bgColor : "transparent";
    el.style.color = props.textColor;
    el.style.justifyContent = toFlexAlign(props.textAlign);
    return;
  }

  if (widget.type === "button") {
    el.style.background = props.bgColor;
    el.style.color = props.textColor;
    el.style.border = `${props.borderWidth}px solid ${props.borderColor}`;
    el.style.justifyContent = toFlexAlign(props.textAlign);
    return;
  }

  if (widget.type === "list") {
    el.style.background = props.bgColor;
    el.style.color = props.textColor;
    el.style.border = `${props.borderWidth}px solid ${props.borderColor}`;
    el.style.alignItems = "stretch";
    return;
  }

  if (widget.type === "image") {
    el.style.background = `linear-gradient(135deg, ${props.tint}, #dbeafe)`;
    el.style.color = "#0f172a";
    return;
  }

  if (widget.type === "qrcode") {
    el.style.background = props.bgColor;
    el.style.color = props.color;
    return;
  }

  if (widget.type === "anim") {
    el.style.background = "linear-gradient(135deg, #0f172a 0%, #334155 100%)";
    el.style.color = "#f8fafc";
    return;
  }

  if (widget.type === "mesh_img") {
    el.style.background = props.scanlineFill
      ? `linear-gradient(135deg, ${props.fillColor} 0%, #93c5fd 100%)`
      : "linear-gradient(135deg, #dbeafe 0%, #93c5fd 100%)";
    el.style.color = "#082f49";
    return;
  }

  if (widget.type === "face_emote") {
    el.style.background = "linear-gradient(180deg, #ffffff 0%, #dbeafe 100%)";
    el.style.color = props.color;
  }
}

function getWidgetPreviewText(widget) {
  switch (widget.type) {
    case "label":
    case "button":
      return widget.props.text;
    case "image":
      return "Image";
    case "anim":
      return `${widget.props.playMode} · ${widget.props.fps} fps`;
    case "mesh_img":
      return `${widget.props.cols}x${widget.props.rows} mesh`;
    case "face_emote":
      return widget.props.expressionName;
    default:
      return widget.name;
  }
}

function createGridPattern(gridSize) {
  void gridSize;
  return `
    linear-gradient(rgba(148, 163, 184, 0.12) 1px, transparent 1px),
    linear-gradient(90deg, rgba(148, 163, 184, 0.12) 1px, transparent 1px)
  `;
}

function renderLayers() {
  const tpl = document.getElementById("layer-item-template");
  layerList.innerHTML = "";

  state.widgets
    .slice()
    .sort((a, b) => b.zIndex - a.zIndex)
    .forEach((widget) => {
      const item = tpl.content.firstElementChild.cloneNode(true);
      item.querySelector(".layer-title").textContent = widget.name;
      item.querySelector(".layer-meta").textContent = `${widgetDefinitions[widget.type].api} · ${widget.w}x${widget.h}`;
      if (widget.id === state.selectedId) {
        item.classList.add("active");
      }
      item.addEventListener("click", () => {
        state.selectedId = widget.id;
        renderAll();
      });
      layerList.appendChild(item);
    });
}

function renderInspector() {
  const widget = getSelectedWidget();
  if (!widget) {
    inspectorEmpty.classList.remove("hidden");
    inspectorForm.classList.add("hidden");
    typeFields.innerHTML = "";
    updateSelectionBadge();
    return;
  }

  inspectorEmpty.classList.add("hidden");
  inspectorForm.classList.remove("hidden");

  document.querySelectorAll("[data-prop]").forEach((input) => {
    const key = input.dataset.prop;
    if (!generalFields.includes(key)) {
      return;
    }
    if (input.type === "checkbox") {
      input.checked = Boolean(widget[key]);
    } else {
      input.value = widget[key];
    }
  });

  renderTypeFields(widget);
  updateSelectionBadge();
}

function renderTypeFields(widget) {
  typeFields.innerHTML = "";
  const def = widgetDefinitions[widget.type];

  def.fields.forEach((field) => {
    const wrapper = document.createElement("label");
    wrapper.innerHTML = `<span>${field.label}</span>`;
    const input = createFieldInput(field, widget.props[field.key]);
    input.dataset.widgetField = field.key;
    input.addEventListener("input", () => updateWidgetProp(widget.id, field, input));
    input.addEventListener("change", () => updateWidgetProp(widget.id, field, input));
    wrapper.appendChild(input);
    typeFields.appendChild(wrapper);
  });
}

function createFieldInput(field, value) {
  if (field.type === "select") {
    const select = document.createElement("select");
    select.className = "designer-select";
    select.style.width = "100%";
    select.style.border = "1px solid var(--border)";
    select.style.borderRadius = "14px";
    select.style.background = "rgba(15, 23, 42, 0.85)";
    select.style.color = "var(--text)";
    select.style.padding = "10px 12px";
    field.options.forEach((optionValue) => {
      const option = document.createElement("option");
      option.value = optionValue;
      option.textContent = optionValue;
      if (optionValue === value) {
        option.selected = true;
      }
      select.appendChild(option);
    });
    return select;
  }

  if (field.type === "textarea") {
    const area = document.createElement("textarea");
    area.value = value;
    area.rows = 5;
    return area;
  }

  const input = document.createElement("input");
  input.type = field.type === "checkbox" ? "checkbox" : field.type;
  if (field.min !== undefined) {
    input.min = String(field.min);
  }
  if (field.max !== undefined) {
    input.max = String(field.max);
  }
  if (input.type === "checkbox") {
    input.checked = Boolean(value);
  } else {
    input.value = value;
  }
  return input;
}

function updateGeneralField(input) {
  const widget = getSelectedWidget();
  if (!widget) {
    return;
  }

  const key = input.dataset.prop;
  widget[key] = parseInputValue(input, key === "visible" ? "checkbox" : input.type);

  if (key === "w" || key === "h") {
    widget[key] = clampNumber(widget[key], 1, 4096);
  }

  if (key === "x" || key === "y") {
    widget[key] = Math.round(Number(widget[key]) || 0);
  }

  renderAll();
}

function updateWidgetProp(widgetId, field, input) {
  const widget = state.widgets.find((item) => item.id === widgetId);
  if (!widget) {
    return;
  }
  widget.props[field.key] = parseInputValue(input, field.type);
  renderAll();
}

function parseInputValue(input, type) {
  if (type === "checkbox") {
    return input.checked;
  }
  if (type === "number") {
    return Number(input.value);
  }
  return input.value;
}

function handleCanvasBackgroundClick() {
  state.selectedId = null;
  renderAll();
}

function startDrag(event) {
  const widgetId = event.currentTarget.dataset.id;
  const widget = state.widgets.find((item) => item.id === widgetId);
  if (!widget) {
    return;
  }

  event.stopPropagation();
  state.selectedId = widgetId;
  dragState = {
    widgetId,
    element: event.currentTarget,
    startX: event.clientX,
    startY: event.clientY,
    originX: widget.x,
    originY: widget.y
  };

  event.currentTarget.classList.add("dragging");
  event.currentTarget.setPointerCapture(event.pointerId);
  event.currentTarget.addEventListener("pointermove", onDragMove);
  event.currentTarget.addEventListener("pointerup", stopDrag);
  event.currentTarget.addEventListener("pointercancel", stopDrag);
  updateSelectionBadge();
}

function onDragMove(event) {
  if (!dragState) {
    return;
  }

  const widget = state.widgets.find((item) => item.id === dragState.widgetId);
  if (!widget) {
    return;
  }

  const scale = getCanvasScale();
  const deltaX = Math.round((event.clientX - dragState.startX) / scale);
  const deltaY = Math.round((event.clientY - dragState.startY) / scale);
  const grid = Math.max(1, Number(state.screen.gridSize) || 1);
  widget.x = snapToGrid(dragState.originX + deltaX, grid);
  widget.y = snapToGrid(dragState.originY + deltaY, grid);
  dragState.element.style.left = `${widget.x}px`;
  dragState.element.style.top = `${widget.y}px`;
  updateInspectorPositionFields(widget);
  updateSelectionBadge();
}

function stopDrag(event) {
  const target = event.currentTarget;
  target.classList.remove("dragging");
  target.removeEventListener("pointermove", onDragMove);
  target.removeEventListener("pointerup", stopDrag);
  target.removeEventListener("pointercancel", stopDrag);
  dragState = null;
  renderAll();
}

function moveSelectedLayer(direction) {
  const widget = getSelectedWidget();
  if (!widget) {
    return;
  }
  const sorted = state.widgets.slice().sort((a, b) => a.zIndex - b.zIndex);
  const index = sorted.findIndex((item) => item.id === widget.id);
  const swapIndex = index + direction;
  if (swapIndex < 0 || swapIndex >= sorted.length) {
    return;
  }
  const other = sorted[swapIndex];
  const temp = widget.zIndex;
  widget.zIndex = other.zIndex;
  other.zIndex = temp;
  renderAll();
}

function duplicateSelectedWidget() {
  const widget = getSelectedWidget();
  if (!widget) {
    return;
  }

  const clone = {
    ...widget,
    id: `${widget.type}_${state.nextId++}`,
    name: `${widget.name} Copy`,
    x: widget.x + 12,
    y: widget.y + 12,
    zIndex: state.widgets.length,
    props: { ...widget.props }
  };
  state.widgets.push(clone);
  state.selectedId = clone.id;
  normalizeZOrder();
  renderAll();
}

function deleteSelectedWidget() {
  const widget = getSelectedWidget();
  if (!widget) {
    return;
  }
  state.widgets = state.widgets.filter((item) => item.id !== widget.id);
  state.selectedId = state.widgets.length ? state.widgets[state.widgets.length - 1].id : null;
  normalizeZOrder();
  renderAll();
}

async function copySceneJson() {
  renderJson();
  try {
    await navigator.clipboard.writeText(sceneJson.value);
    selectionBadge.textContent = "Scene JSON copied";
  } catch {
    selectionBadge.textContent = "Clipboard unavailable";
  }
}

function importSceneJson() {
  try {
    const parsed = JSON.parse(sceneJson.value);
    if (!parsed || !Array.isArray(parsed.widgets) || !parsed.screen) {
      throw new Error("invalid scene");
    }

    state.version = Number(parsed.version) || 1;
    state.screen = {
      width: clampNumber(Number(parsed.screen.width) || 320, 64, 4096),
      height: clampNumber(Number(parsed.screen.height) || 240, 64, 4096),
      bgColor: normalizeColor(parsed.screen.bgColor || "#f8fafc"),
      gridSize: clampNumber(Number(parsed.screen.gridSize) || 8, 1, 64)
    };
    state.theme = {
      accent: parsed.theme?.accent || "#2563eb",
      surface: parsed.theme?.surface || "#0f172a",
      text: parsed.theme?.text || "#e2e8f0"
    };
    state.widgets = parsed.widgets
      .filter((widget) => widgetDefinitions[widget.type])
      .map((widget, index) => ({
        id: String(widget.id || `${widget.type}_${index + 1}`),
        type: widget.type,
        name: String(widget.name || `${widgetDefinitions[widget.type].label} ${index + 1}`),
        x: Math.round(Number(widget.x) || 0),
        y: Math.round(Number(widget.y) || 0),
        w: clampNumber(Number(widget.w) || widgetDefinitions[widget.type].defaultSize.w, 1, 4096),
        h: clampNumber(Number(widget.h) || widgetDefinitions[widget.type].defaultSize.h, 1, 4096),
        visible: widget.visible !== false,
        zIndex: Number(widget.zIndex) || index,
        props: {
          ...widgetDefinitions[widget.type].defaults,
          ...(widget.props || {})
        }
      }));
    state.nextId = getNextWidgetId(state.widgets);
    state.selectedId = state.widgets[0]?.id || null;
    normalizeZOrder();
    renderAll();
  } catch (error) {
    selectionBadge.textContent = `Import failed: ${error.message}`;
  }
}

function resetScene() {
  const fresh = createSampleScene();
  Object.assign(state, fresh);
  renderAll();
  fitCanvasToViewport();
}

function renderJson() {
  const scene = {
    version: state.version,
    screen: { ...state.screen },
    theme: { ...state.theme },
    widgets: state.widgets
      .slice()
      .sort((a, b) => a.zIndex - b.zIndex)
      .map((widget) => ({
        id: widget.id,
        type: widget.type,
        name: widget.name,
        x: widget.x,
        y: widget.y,
        w: widget.w,
        h: widget.h,
        visible: widget.visible,
        zIndex: widget.zIndex,
        props: { ...widget.props }
      }))
  };
  sceneJson.value = JSON.stringify(scene, null, 2);
}

function normalizeZOrder() {
  state.widgets
    .slice()
    .sort((a, b) => a.zIndex - b.zIndex)
    .forEach((widget, index) => {
      widget.zIndex = index;
    });
}

function updateSelectionBadge() {
  const widget = getSelectedWidget();
  selectionBadge.textContent = widget
    ? `${widget.name} · ${widget.type} · (${widget.x}, ${widget.y})`
    : "No selection";
}

function updateInspectorPositionFields(widget) {
  const xInput = inspectorForm.querySelector('[data-prop="x"]');
  const yInput = inspectorForm.querySelector('[data-prop="y"]');
  if (xInput) {
    xInput.value = widget.x;
  }
  if (yInput) {
    yInput.value = widget.y;
  }
}

function fitCanvasToViewport() {
  const availableWidth = Math.max(320, canvasWrap.clientWidth - 40);
  const availableHeight = Math.max(320, canvasWrap.clientHeight - 40);
  const scale = Math.min(
    1,
    availableWidth / state.screen.width,
    availableHeight / state.screen.height
  );
  canvas.style.transform = `scale(${scale})`;
}

function getCanvasScale() {
  const match = canvas.style.transform.match(/scale\((.+)\)/);
  if (!match) {
    return 1;
  }
  return Number(match[1]) || 1;
}

function getSelectedWidget() {
  return state.widgets.find((widget) => widget.id === state.selectedId) || null;
}

function toFlexAlign(textAlign) {
  if (textAlign === "right") {
    return "flex-end";
  }
  if (textAlign === "center") {
    return "center";
  }
  return "flex-start";
}

function snapToGrid(value, grid) {
  return Math.round(value / grid) * grid;
}

function clampNumber(value, min, max) {
  if (Number.isNaN(value)) {
    return min;
  }
  return Math.min(Math.max(value, min), max);
}

function normalizeColor(value) {
  if (typeof value !== "string") {
    return "#000000";
  }
  return value.startsWith("#") ? value : `#${value}`;
}

function getNextWidgetId(widgets) {
  let maxId = 0;
  widgets.forEach((widget) => {
    const match = String(widget.id).match(/_(\d+)$/);
    if (match) {
      maxId = Math.max(maxId, Number(match[1]) || 0);
    }
  });
  return maxId + 1;
}
