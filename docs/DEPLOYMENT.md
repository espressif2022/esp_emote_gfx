# 文档发布指南

本文档介绍如何发布 ESP Emote GFX API 文档的几种方式。

## 方式一：GitHub Pages（推荐）

GitHub Pages 是免费的静态网站托管服务，适合开源项目。

### 步骤 1: 创建 GitHub Actions 工作流

在项目根目录创建 `.github/workflows/docs.yml`：

```yaml
name: Build and Deploy Documentation

on:
  push:
    branches:
      - main
      - master
    paths:
      - 'docs/**'
      - '.github/workflows/docs.yml'
  workflow_dispatch:

permissions:
  contents: read
  pages: write
  id-token: write

concurrency:
  group: "pages"
  cancel-in-progress: false

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      
      - name: Set up Python
        uses: actions/setup-python@v5
        with:
          python-version: '3.11'
          cache: 'pip'
      
      - name: Install dependencies
        run: |
          pip install sphinx sphinx-rtd-theme breathe
      
      - name: Build documentation
        working-directory: docs
        run: make html
      
      - name: Setup Pages
        uses: actions/configure-pages@v4
      
      - name: Upload artifact
        uses: actions/upload-pages-artifact@v3
        with:
          path: docs/_build/html

  deploy:
    environment:
      name: github-pages
      url: ${{ steps.deployment.outputs.page_url }}
    runs-on: ubuntu-latest
    needs: build
    steps:
      - name: Deploy to GitHub Pages
        id: deployment
        uses: actions/deploy-pages@v4
```

### 步骤 2: 启用 GitHub Pages

1. 进入 GitHub 仓库的 Settings
2. 找到 Pages 设置
3. 在 Source 中选择 "GitHub Actions"
4. 保存设置

### 步骤 3: 推送代码

```bash
git add .github/workflows/docs.yml
git commit -m "Add documentation deployment workflow"
git push
```

文档将在 `https://<username>.github.io/<repository>/` 或自定义域名下可用。

## 方式二：Read the Docs

Read the Docs 是专门为技术文档设计的托管平台，完全免费。

### 步骤 1: 创建 `.readthedocs.yaml`

在项目根目录创建 `.readthedocs.yaml`：

```yaml
version: 2

build:
  os: ubuntu-22.04
  tools:
    python: "3.11"
  commands:
    - pip install sphinx sphinx-rtd-theme breathe
    - cd docs && make html

sphinx:
  configuration: docs/conf.py
  builder: html

python:
  install:
    - requirements: docs/requirements.txt
```

### 步骤 2: 创建 requirements.txt（可选）

在 `docs/` 目录创建 `requirements.txt`：

```
sphinx>=7.0.0
sphinx-rtd-theme>=1.3.0
breathe>=4.35.0
```

### 步骤 3: 在 Read the Docs 上导入项目

1. 访问 https://readthedocs.org/
2. 使用 GitHub 账号登录
3. 点击 "Import a Project"
4. 选择你的仓库
5. 项目名称会自动填充
6. 点击 "Create"

### 步骤 4: 配置项目

在项目设置中：
- **Repository URL**: 你的 GitHub 仓库 URL
- **Default branch**: main 或 master
- **Python configuration file**: `.readthedocs.yaml`
- **Requirements file**: `docs/requirements.txt`（如果创建了）

### 步骤 5: 构建文档

Read the Docs 会自动检测到 `.readthedocs.yaml` 并开始构建。构建完成后，文档将在 `https://<project-name>.readthedocs.io/` 可用。

## 方式三：本地构建和查看

### 快速构建

```bash
cd docs
pip install sphinx sphinx-rtd-theme breathe
make html
```

生成的文档在 `docs/_build/html/` 目录，用浏览器打开 `index.html` 即可查看。

### 本地服务器（推荐用于预览）

使用 Python 内置服务器：

```bash
cd docs/_build/html
python3 -m http.server 8000
```

然后在浏览器访问 `http://localhost:8000`

## 方式四：Netlify / Vercel

这些平台也支持静态网站托管，适合需要更多自定义的场景。

### Netlify 配置

创建 `netlify.toml`：

```toml
[build]
  command = "cd docs && pip install sphinx sphinx-rtd-theme breathe && make html"
  publish = "docs/_build/html"

[[redirects]]
  from = "/*"
  to = "/index.html"
  status = 200
```

### Vercel 配置

创建 `vercel.json`：

```json
{
  "buildCommand": "cd docs && pip install sphinx sphinx-rtd-theme breathe && make html",
  "outputDirectory": "docs/_build/html",
  "installCommand": "pip install sphinx sphinx-rtd-theme breathe"
}
```

## 方式五：手动部署到服务器

### 构建文档

```bash
cd docs
make html
```

### 上传到服务器

使用 `rsync` 或 `scp` 上传 `_build/html/` 目录：

```bash
rsync -avz docs/_build/html/ user@server:/var/www/docs/
```

或使用 `scp`：

```bash
scp -r docs/_build/html/* user@server:/var/www/docs/
```

## 推荐方案对比

| 方案 | 优点 | 缺点 | 适用场景 |
|------|------|------|----------|
| GitHub Pages | 免费、与代码集成、自动部署 | 需要公开仓库（或付费） | 开源项目 |
| Read the Docs | 专业、自动版本管理、搜索功能 | 构建时间可能较长 | 技术文档项目 |
| 本地构建 | 完全控制、快速 | 需要手动部署 | 内网或私有部署 |
| Netlify/Vercel | 快速、CDN、自定义域名 | 免费版有限制 | 需要更多功能的项目 |

## 自定义域名

### GitHub Pages

1. 在仓库 Settings > Pages 中设置 Custom domain
2. 在域名 DNS 中添加 CNAME 记录指向 GitHub Pages

### Read the Docs

在项目设置中配置 Custom domain，然后添加 DNS 记录。

## 持续集成

文档会自动在以下情况更新：
- **GitHub Pages**: 每次推送到 main/master 分支
- **Read the Docs**: 每次推送或通过 webhook 触发
- **其他平台**: 根据配置的 CI/CD 流程

## 故障排查

### 构建失败

1. 检查 Python 版本（需要 3.8+）
2. 确认所有依赖已安装
3. 查看构建日志中的错误信息

### 样式丢失

确保 `html_static_path` 在 `conf.py` 中正确配置。

### 链接错误

检查 `index.rst` 中的 `toctree` 配置，确保所有文件路径正确。

## 更多资源

- [Sphinx 文档](https://www.sphinx-doc.org/)
- [Read the Docs 文档](https://docs.readthedocs.io/)
- [GitHub Pages 文档](https://docs.github.com/pages)

