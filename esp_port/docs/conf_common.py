# -*- coding: utf-8 -*-
#
# Common (non-language-specific) configuration for Sphinx
#

# type: ignore
# pylint: disable=wildcard-import
# pylint: disable=undefined-variable

from __future__ import print_function, unicode_literals

from esp_docs.conf_docs import *  # noqa: F403,F401
import os
import sys

sys.path.insert(0, os.path.abspath('.'))

extensions += ['sphinx_copybutton',
               # Needed as a trigger for running doxygen
               'esp_docs.esp_extensions.dummy_build_system',
               'esp_docs.esp_extensions.run_doxygen',
               'sphinx.ext.autodoc',
               'sphinxcontrib.plantuml'
               ]

# PlantUML configuration
plantuml_output_format = 'svg'
plantuml_latex_output_format = 'pdf'

# PlantUML responsive sizing options
plantuml_syntax_error_image = True
plantuml_cache_path = '_plantuml'

# PlantUML setup - use pip-installed plantuml package
import shutil

# Try different PlantUML command options
if shutil.which('plantuml'):
    plantuml = 'plantuml'
elif shutil.which('python') and shutil.which('pip'):
    # Use pip-installed plantuml
    plantuml = 'python -m plantuml'
else:
    # Fallback
    plantuml = 'echo "PlantUML not available - install via: pip install plantuml"'

# Note: MyST parser for markdown is not available in ESP-docs environment

# link roles config
github_repo = 'aws-samples/amazon-kinesis-video-streams-webrtc-sdk-c'

# context used by sphinx_idf_theme
html_context['github_user'] = 'aws-samples'
html_context['github_repo'] = 'amazon-kinesis-video-streams-webrtc-sdk-c'
html_static_path = ['../_static']

# Extra options required by sphinx_idf_theme
project_slug = 'amazon-kinesis-video-streams-webrtc-sdk-c'

idf_targets = ['esp32', 'esp32s3', 'esp32c2', 'esp32c3']
languages = ['en']
