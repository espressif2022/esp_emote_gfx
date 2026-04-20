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

# gettext / Sphinx i18n: translations live in docs/locale/<lang>/LC_MESSAGES/*.mo
locale_dirs = ['locale']
gettext_compact = False

# -- Options for HTML output -------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#options-for-html-output

html_theme = 'sphinx_idf_theme'
html_static_path = ['_static']
html_logo = None
html_favicon = None
html_theme_options = {
    'display_version': True,
}
html_css_files = ['esp_emote_gfx.css']
html_js_files = ['lang_switch.js']

project_slug = 'esp-emote-gfx'
project_homepage = 'https://github.com/espressif2022/esp_emote_gfx'
language = 'en'
languages = ['en', 'zh_CN']
idf_target = 'esp32'
idf_targets = ['esp32']
idf_target_title_dict = {
    'esp32': 'ESP32',
}
versions_url = ''
pdf_file = ''

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


def setup(app):
    app.add_config_value('pdf_file', pdf_file, 'html')
    app.add_config_value('idf_target_title_dict', idf_target_title_dict, 'html')

