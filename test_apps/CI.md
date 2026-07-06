# test_apps CI 部署指南

本文说明如何在 CI 里分层跑测试：**云 CI 能自动做的** vs **需要真板的**。

## 一、CI 能自动跑什么

```
┌─────────────────────────────────────────────────────────────┐
│  GitHub Actions (ubuntu + espressif/idf 容器)                 │
├─────────────────────────────────────────────────────────────┤
│  ✅ host ctest          根目录 SDL smoke，8 项，无需硬件       │
│  ✅ idf-build-apps      unit / visual / hw 编译 gate          │
│  ❌ Unity 在 ESP32 上执行  需要串口 + 真板（见第二节 HIL）      │
└─────────────────────────────────────────────────────────────┘
```

| 层级 | 工程 | CI 现况 | 说明 |
|------|------|---------|------|
| Host | `cmake -B build-host-sdl && ctest` | ✅ 已在 workflow | widget/logic smoke，Linux 直接跑 |
| Target 编译 | `test_apps/unit` | ✅ 见 `build.yml` | 保证链接/配置正确 |
| Target 编译 | `test_apps/visual` | ✅ BSP matrix | 不跑用例，只编译 |
| Target 编译 | `test_apps` (hw) | ✅ BSP matrix | 含 emote_gen 分区 |
| Target 执行 | `gfx_test_unit` | ⚠️ 需 HIL | `unity_run_all_tests()` 非交互 |
| Target 执行 | visual / hw | ❌ 不纳入 gate | 人工 menu + 肉眼 |

**结论：** PR gate = **host ctest + 三工程 build**；真板 Unity 放到 **self-hosted / nightly**。

---

## 二、云 CI（GitHub Actions）

仓库已配置 `.github/workflows/build.yml`，每次 push/PR 会：

1. `scripts/check_no_esp_in_src.sh`
2. Host：`cmake -DGFX_BUILD_HOST_SDL=ON -DBUILD_TESTING=ON && ctest`
3. Target：`idf-build-apps build` 编译 `unit`、`visual`、`hw`

本地复现：

```bash
# 1) Host 测试（与 CI 相同）
cmake -S . -B build-host-sdl -DGFX_BUILD_HOST_SDL=ON -DBUILD_TESTING=ON
cmake --build build-host-sdl --parallel
cd build-host-sdl && ctest --output-on-failure

# 2) Target 编译（与 CI 相同）
. $IDF_PATH/export.sh
pip install idf-build-apps
IDF_CCACHE_ENABLE=1 idf-build-apps build \
  -p test_apps/unit \
  -p test_apps/visual \
  -p test_apps \
  --config-rules "sdkconfig.bsp.*=" \
  -vv
```

### GitLab CI

若用 GitLab，等价 job 示例：

```yaml
test:host:
  image: ubuntu:22.04
  script:
    - apt-get update && apt-get install -y libsdl2-dev pkg-config cmake ninja-build g++
    - cmake -S . -B build-host-sdl -DGFX_BUILD_HOST_SDL=ON -DBUILD_TESTING=ON
    - cmake --build build-host-sdl --parallel
    - cd build-host-sdl && ctest --output-on-failure

test:idf:
  image: espressif/idf:release-v5.5
  script:
    - pip install idf-build-apps
    - idf-build-apps build -p test_apps/unit -p test_apps/visual -p test_apps
        --config-rules "sdkconfig.bsp.*=" -vv
```

---

## 三、真板自动化（HIL）

`unit` 工程入口是 `unity_run_all_tests()`，必须在 **ESP32-S3 真机**（或自建 runner 挂板）上 flash + monitor。

### 方案 A：脚本 + 自建 Runner（推荐）

1. 准备一块 **ESP-BOX**（或同 BSP 的 S3 板），USB 接在 CI 机器上。
2. 注册 [GitHub self-hosted runner](https://docs.github.com/en/actions/hosting-your-own-runners)。
3. 使用仓库脚本：

```bash
# 环境变量
export IDF_PATH=...
export ESP_PORT=/dev/ttyACM0        # 按实际修改

# 编译 + 烧录 + 跑 unit（失败时 exit 1）
./test_apps/ci/run_unit_on_target.sh
```

脚本会：

- `idf.py build flash monitor` 在 `test_apps/unit`
- 用 Python 读串口，匹配 `Unity test run` / `FAIL` / `OK`
- 超时 120s，有 FAIL 或非零 exit 则 CI 失败

Workflow 片段（`.github/workflows/unit-hil.yml`，仅 self-hosted）：

```yaml
name: Unit tests on target

on:
  workflow_dispatch:
  schedule:
    - cron: '0 2 * * *'   # nightly

jobs:
  unit-hil:
    runs-on: self-hosted   # 标签按你的 runner 改
    steps:
      - uses: actions/checkout@v4
      - name: Run unit tests on ESP-BOX
        env:
          ESP_PORT: /dev/ttyACM0
        run: ./test_apps/ci/run_unit_on_target.sh
```

### 方案 B：pytest-embedded（Espressif 生态）

适合已有 `pytest-embedded-idf` 基础设施的团队：

```bash
pip install pytest pytest-embedded pytest-embedded-idf pytest-embedded-serial-esp
pytest test_apps/ci/pytest_unit.py --target esp32s3 -s
```

需要 `conftest.py` 指定 app_path=`test_apps/unit`。详见 Espressif [pytest-embedded 文档](https://docs.espressif.com/projects/pytest-embedded/en/latest/)。

### 方案 C：仅本地 / MR 前手动

```bash
cd test_apps/unit
idf.py build flash monitor
# 串口会自动跑完所有 [unit] 用例并打印汇总
```

---

## 四、推荐 gate 策略

| 事件 | 必过检查 |
|------|----------|
| PR | host `ctest` + build `unit` + build `visual` + build `hw` |
| merge 后 nightly | 上述 + HIL `run_unit_on_target.sh` |
| 发版前 | HIL unit + 人工跑 `visual` menu 抽检 |

**不要**把 `visual` / `hw` menu 演示放进 PR 自动执行 gate（依赖肉眼 + 长时间 observe）。

---

## 五、后续增强（可选）

1. **扩充 host ctest**：把 `test_backend.c` 中不依赖 priv 头的用例逐步迁到 `simulation/host/`。
2. **Unity 标签过滤**：HIL 脚本加 `-t [unit][backend]` 只跑子集（需改 `app_main` 支持 `unity_run_tests_by_tag`）。
3. **Artifacts**：CI 已上传 `test_apps/build_*`，失败时可下载 elf/map 排查。
4. **缓存**：`IDF_CCACHE_ENABLE=1` 已开；可加 `ccache-action` 持久化。

---

## 六、故障排查

| 现象 | 处理 |
|------|------|
| `emote_assets.bin` 缺失 | 仅 hw 工程；configure 会自动下载到 `test_apps/prebuilt/` |
| unit 编译找不到 harness | `idf.py fullclean build` 或删 `build/` 后重配 |
| HIL 串口无输出 | 检查 `ESP_PORT`、USB 权限（`dialout` 组）、板子是否 boot |
| Unity `FAIL` | 串口日志搜 `FAIL:` 定位用例；本地 `idf.py monitor` 复现 |
| ctest 失败 | `cd build-host-sdl && ctest -V --output-on-failure` |
