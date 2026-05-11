# Configuration file for the Sphinx documentation builder.
#
# For the full list of built-in configuration values, see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

from datetime import date
from pathlib import Path
import re

ROOT_DIR = Path(__file__).resolve().parent
while ROOT_DIR != ROOT_DIR.parent and not (ROOT_DIR / 'idf_component.yml').is_file():
    ROOT_DIR = ROOT_DIR.parent


def _component_version():
    manifest = ROOT_DIR / 'idf_component.yml'
    try:
        text = manifest.read_text(encoding='utf-8')
    except OSError:
        return '0.0.0'

    match = re.search(r'^version:\s*[\'"]?([^\'"\n]+)', text, re.MULTILINE)
    return match.group(1).strip() if match else '0.0.0'

# -- Project information -----------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#project-information

project = 'ESP Emote GFX'
copyright = f'2024-{date.today().year}, Espressif Systems (Shanghai) CO LTD'
author = 'Espressif Systems'
release = _component_version()
version = release

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
html_title = f'{project} Programming Guide'
html_theme_options = {
    'display_version': True,
}
html_css_files = ['esp_emote_gfx.css']
html_js_files = ['lang_switch.js']

project_slug = 'esp-emote-gfx'
project_homepage = 'https://github.com/espressif2022/esp_emote_gfx'
languages = ['en', 'zh_CN']
_source_language = Path(__file__).resolve().parent.name
language = _source_language if _source_language in languages else 'en'
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
    app.add_config_value('config_dir', '', 'html')
    app.add_config_value('docs_to_build', '', 'html')
    app.add_config_value('doxyfile_dir', '', 'html')
    app.add_config_value('project_path', '', 'html')
    app.add_config_value('pdf_file', pdf_file, 'html')
    app.add_config_value('idf_target_title_dict', idf_target_title_dict, 'html')

