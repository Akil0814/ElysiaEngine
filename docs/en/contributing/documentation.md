# Elysia Engine Documentation Rules

This document defines organization, writing, and maintenance conventions for developers adding, modifying, or deleting project documentation.

Documentation should accurately reflect the project's current state and help developers understand the engine, use its public interfaces, and contribute.

## 1. Documentation Categories

Project documentation is divided into five categories by purpose.

| Category | Content |
| --- | --- |
| User guides | Building and running, core concepts, subsystem integration steps, and examples |
| Interface and format reference | Public APIs, parameters, results, configuration fields, and cross-component contracts |
| Architecture | Current module responsibilities, dependencies, internal design, and runtime mechanisms |
| Development and contribution guides | Coding standards, testing, development conventions, and documentation maintenance |
| Design documents | Roadmap, proposals, planned capabilities, and unfinished architectural migrations |

### User Guides

For developers building games with Elysia Engine.

Focus on:

- What functionality the engine provides.
- How to use public interfaces correctly.
- Required interface contracts and lifecycle rules.
- Common mistakes and how to handle them.

User guides should not require readers to understand engine internals.

### Interface and Format Reference

For developers who understand basic integration and need specific calling or field rules.

Collect public interfaces, parameters, results, configuration structures, and cross-component contracts. Maintain complete field lists in one place. Guides retain procedures and necessary examples and link to the relevant reference section. Internal scheduling and implementation reasoning belong in architecture documentation.

### Architecture

For developers understanding, maintaining, or extending the engine.

Describe the actual design of current code: module responsibilities, dependencies, key data structures, execution flows, and constraints.

Architecture documentation must describe current implementation rather than an unimplemented target design.

### Development and Contribution Guides

For contributors developing and maintaining the engine.

Record shared project requirements, including coding conventions, testing methods, documentation maintenance, and other contribution rules.

### Design Documents

Record future plans, proposals, and unfinished migrations.

Unless explicitly marked as a current-implementation reference, types, interfaces, and flows in design documents do not represent implemented functionality or stable API commitments.

A planned capability must not be described as currently supported merely because a design document exists.

## 2. Content Requirements

### Accuracy

Use current source code, tests, and actual configuration as factual evidence.

- Do not describe planned functionality as implemented.
- Do not describe undecided interfaces as stable APIs.
- Do not infer current behavior from obsolete implementations.
- Do not claim unverified platform compatibility or performance.
- Do not omit limitations that affect correct interface use.

When documentation and implementation disagree, inspect implementation and tests, establish intended behavior, then correct the documentation or code.

### Define the Audience and Scope

Choose depth according to the intended reader.

User guides focus on calling patterns and interface contracts; architecture documents focus on design reasons and internal runtime mechanisms.

Do not repeat complete content across documents. Link to existing detailed explanations.

### Interfaces and Lifecycles

For public APIs, explain as needed:

- Purpose and preconditions.
- Meanings of parameters and results.
- Object ownership and reference validity.
- Error handling and failure semantics.
- Calling phases, execution order, and other significant restrictions.

Do not explain object creation, scene switching, deferred destruction, or resource release solely through function names.

### Code Examples

Examples must match current public interfaces.

- Use valid current types, functions, and parameters.
- State required initialization conditions.
- Avoid deprecated or internal-only interfaces.
- Ensure omitted code does not mislead readers.

Code labeled as a complete example must build and run in the stated environment.

Local calling snippets need not compile independently, but must state their required context.

## 3. Organization and Naming

### File Organization

Place new documents in the directory matching their purpose.

- Usage instructions belong in user guides.
- Current implementation explanations belong in architecture.
- Development rules belong in contribution guides.
- Unimplemented designs belong in the design directory.

Keep related subsystem documentation together instead of scattering it across unrelated directories.

Maintain UI documentation by topic: the guide landing page carries the complete integration tutorial, eight reference topic pages maintain contracts with task and class indexes on their landing page, and the architecture landing page explains internal flows. Add new controls to the relevant topic rather than creating small pages for individual controls or shared contracts. World-space UI, development panels, and the built-in settings scene retain separate guides linked from the UI entry point.

### Filenames

Markdown documents use the `.md` extension.

Prefer lowercase English words separated by hyphens, for example:

- `coding-standard.md`
- `build-and-run.md`
- `core-concepts.md`

Directory entry points use `README.md`.

Do not rename existing files merely for superficial naming uniformity.

Tutorials belong in `user-guide/`, public contracts in `reference/`, and internal mechanisms in `architecture/`. Guides retain conditions, procedures, and short examples needed for the task; complete field lists are maintained only in reference pages. When migrating content, update all incoming links, heading anchors, relative source paths, and example-compilation configuration together. Do not retain two copies of the same body.
