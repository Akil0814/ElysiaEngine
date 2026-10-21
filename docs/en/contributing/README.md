# Engine Development and Contribution Guide

This directory is for developers modifying the engine, tests, or documentation. Before making changes, prepare the environment using the [build guide](../user-guide/build-and-run.md), then read the target module's [architecture](../architecture/README.md).

## Write and Review Code

- [C++ Coding Standard](coding-standard.md): naming, lifecycle interfaces, ownership, and error-handling conventions.
- [C++11–23 Feature Inventory](cpp-language-features.md): automatically recognizable language and library usage in current first-party source, and the scanner's limitations. Counts do not measure code quality or performance.

## Validate and Maintain Documentation

- [Testing Guide](testing.md): building test targets, selecting tests by label, compiling documentation examples, and optional stress tests.
- [Documentation Rules](documentation.md): classification, wording, code examples, links, and multilingual maintenance requirements.

Current behavior is established by source code, tests, and architecture documentation. For planned capabilities and historical discussions, see [Designs and Roadmap](../design/README.md); do not treat proposals as existing interfaces.
