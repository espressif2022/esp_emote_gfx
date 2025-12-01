# ESP Emote GFX API Documentation

This directory contains the Sphinx-based API documentation for ESP Emote GFX.

## Building the Documentation

### Prerequisites

Install Sphinx and the required extensions:

```bash
pip install -r requirements.txt
```

Or manually:

```bash
pip install sphinx sphinx-rtd-theme breathe
```

### Build Commands

Generate HTML documentation:

```bash
cd docs
make html
```

The generated documentation will be in `_build/html/`.

Generate PDF documentation:

```bash
make latexpdf
```

Generate all formats:

```bash
make all
```

## Documentation Structure

- `index.rst` - Main documentation index
- `overview.rst` - Framework overview and architecture
- `quickstart.rst` - Quick start guide
- `api/core/` - Core API documentation
- `api/widgets/` - Widget API documentation
- `examples.rst` - Code examples
- `changelog.rst` - Version history

## Viewing the Documentation

After building, open `_build/html/index.html` in your web browser.

Or use a local server:

```bash
cd _build/html
python3 -m http.server 8000
```

Then visit `http://localhost:8000` in your browser.

## Publishing Documentation

See [DEPLOYMENT.md](DEPLOYMENT.md) for detailed instructions on publishing the documentation to:

- GitHub Pages
- Read the Docs
- Netlify / Vercel
- Custom server

## Contributing

When adding new API functions or features:

1. Update the relevant `.rst` file in `api/`
2. Add examples to `examples.rst` if applicable
3. Rebuild the documentation to verify formatting
4. Update the changelog if needed

