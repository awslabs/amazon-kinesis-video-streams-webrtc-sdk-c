# Amazon Kinesis Video Streams WebRTC SDK for ESP-IDF Documentation

This directory contains the source files for the Amazon Kinesis Video Streams WebRTC SDK for ESP-IDF documentation.

## Documentation Structure

The documentation is organized as follows:

- `en/`: English language documentation source files
- `_static/`: Static files (images, CSS, etc.)
- `*.puml`: PlantUML diagram files for architecture documentation
- `Doxyfile`: Doxygen configuration file
- `conf_common.py`: Common configuration for Sphinx
- `local_util.py`: Utility functions for documentation build

### PlantUML Diagrams

The documentation includes several PlantUML diagrams:
- `webrtc_classic.puml` - WebRTC Classic Mode architecture
- `webrtc_split.puml` - WebRTC Split Mode architecture
- `camera_device.puml` - ESP Camera device lifecycle

These diagrams are automatically rendered in the documentation when PlantUML is available.

## Building the Documentation

### Prerequisites

1. Install Python 3.x and pip
2. Install esp-docs and other required packages:

```bash
pip install -r requirements.txt
```

This will automatically install:
- **esp-docs**: ESP-IDF documentation build system
- **sphinxcontrib-plantuml**: PlantUML integration for Sphinx
- **plantuml**: Python PlantUML package for diagram rendering
- All other documentation dependencies

**Note**: PlantUML requires Java to be installed on your system.

### Building with build-docs Command

Documentation can be built using the `build-docs` command provided by esp-docs:

```bash
# Build documentation for a specific language and target
build-docs -l en -t esp32
```

Options:
- `-l`: Language (e.g., en)
- `-t`: Target chip (e.g., esp32, esp32s3, esp32c3)
- `-d`: Documentation to build (optional)
- `-b`: Build type (html, pdf, etc.) (default: html)

The generated documentation will be in `_build/en/esp32/html/`.


## Documentation Style Guide

When contributing to the documentation, please follow these guidelines:

1. Use reStructuredText (RST) format for Sphinx documentation
2. Follow the [Sphinx style guide](https://documentation-style-guide-sphinx.readthedocs.io/en/latest/style-guide.html)
3. Use proper heading hierarchy (# for top-level headings, ## for second-level, etc.)
4. Include code examples where appropriate
5. Use cross-references to link between documents

## ESP-Docs Integration

The documentation is designed to be compatible with the ESP-Docs system used by Espressif. This allows the documentation to be integrated with the ESP-IDF documentation ecosystem.

## Viewing the Documentation

After building the HTML documentation, open `_build/en/esp32/html/index.html` in your web browser to view it locally.

## Troubleshooting

If you encounter issues with the documentation build:

1. Make sure your the python environment is properly setup
2. Check that esp-docs is correctly installed

### PlantUML Issues

If PlantUML diagrams don't render:

1. **Check Java installation**: `java -version`
2. **Verify PlantUML installation**: `python -c "import plantuml; print('PlantUML available')"`
3. **Alternative viewing**: View `.puml` files directly on GitHub or use online PlantUML editor at http://www.plantuml.com/plantuml/uml/

The documentation will show fallback content if PlantUML isn't available.

## Contributing

Contributions to the documentation are welcome. Please follow the style guide and submit pull requests for any improvements or corrections.
