# Configuration file for the Sphinx documentation builder.
#
# For the full list of built-in configuration values, see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

# -- Project information -----------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#project-information

project = 'ESP Emote GFX'
copyright = '2024-2025, Espressif Systems (Shanghai) CO LTD'
author = 'Espressif Systems'
release = '1.0.0'

# -- General configuration ---------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#general-configuration

extensions = [
    'sphinx.ext.autodoc',
    'sphinx.ext.viewcode',
    'sphinx.ext.intersphinx',
    'breathe',  # For Doxygen integration (optional)
]

templates_path = ['_templates']
exclude_patterns = ['_build', 'Thumbs.db', '.DS_Store']

# -- Options for HTML output -------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#options-for-html-output

html_theme = 'sphinx_rtd_theme'  # Read the Docs theme (similar to LVGL)
html_static_path = ['_static']
html_logo = None
html_favicon = None

# -- Extension configuration -------------------------------------------------

# Breathe configuration (if using Doxygen)
breathe_projects = {
    "esp_emote_gfx": "../doxygen/xml"
}
breathe_default_project = "esp_emote_gfx"

# Intersphinx mapping
intersphinx_mapping = {
    'python': ('https://docs.python.org/3', None),
}

# -- Options for autodoc ----------------------------------------------------
autodoc_mock_imports = ['esp_err', 'esp_log', 'lvgl', 'freetype']

