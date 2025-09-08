# Documentation Automation Scripts

This directory contains scripts to automatically generate and maintain API documentation for ESP Emote GFX.

## Available Tools

### 1. Doxygen Documentation
Generate complete HTML documentation from header file comments.

```bash
# Generate Doxygen docs
make -f Makefile.docs doxygen

# View generated documentation
open docs/html/index.html
```

### 2. API Reference Generator
Automatically extract API functions from header files and generate markdown tables.

```bash
# Generate API reference table
make -f Makefile.docs api-docs

# Output file: API_REFERENCE.md
```

### 3. Complete Documentation Build
Generate both Doxygen and API reference documentation.

```bash
# Generate all documentation
make -f Makefile.docs docs
```

## Manual Usage

### Run API extraction script directly:
```bash
python3 scripts/generate_api_docs.py
```

### Run Doxygen directly:
```bash
doxygen Doxyfile
```

## Requirements

- **Python 3.x** - For API extraction script
- **Doxygen** - For generating HTML documentation
  - Ubuntu/Debian: `sudo apt-get install doxygen`
  - macOS: `brew install doxygen`
  - Windows: Download from https://www.doxygen.nl/download.html

## Customization

### Modify API Descriptions
Edit the `descriptions` dictionary in `generate_api_docs.py` to customize function descriptions.

### Modify Doxygen Output
Edit `Doxyfile` to customize the generated documentation:
- Change `PROJECT_NAME` and `PROJECT_BRIEF`
- Modify `HTML_COLORSTYLE_*` for different colors
- Add custom CSS with `HTML_STYLESHEET`

## Integration with CI/CD

Add to your GitHub Actions workflow:

```yaml
- name: Generate Documentation
  run: |
    sudo apt-get install doxygen
    make -f Makefile.docs docs

- name: Deploy to GitHub Pages
  uses: peaceiris/actions-gh-pages@v3
  with:
    github_token: ${{ secrets.GITHUB_TOKEN }}
    publish_dir: ./docs/html
```

## Keeping Documentation Updated

To ensure documentation stays in sync with code changes:

1. **Pre-commit hook**: Run `make -f Makefile.docs api-docs` before commits
2. **CI check**: Verify API_REFERENCE.md is up-to-date in CI pipeline
3. **Manual review**: Periodically review generated docs for accuracy
