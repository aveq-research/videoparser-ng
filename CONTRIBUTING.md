# Contributing

Thank you for your interest in contributing to videoparser-ng!

## Getting Started

Before contributing, please read the [Developer Guide](DEVELOPERS.md) to understand the project structure, testing procedures, and implementation details.

## How to Contribute

### Setting Up Your Development Environment

1. Fork this repository
2. Fork the submodules you need to modify:
   - FFmpeg: https://github.com/aveq-research/ffmpeg (branch: `videoparser`)
   - libaom (for AV1): https://github.com/aveq-research/libaom (branch: `videoparser`)

3. Clone your fork and update the submodule URLs:

```bash
git clone https://github.com/your-username/videoparser-ng
cd videoparser-ng

# If modifying FFmpeg
git submodule set-url external/ffmpeg https://github.com/your-username/ffmpeg

# If modifying libaom (for AV1 support)
git submodule set-url external/libaom https://github.com/your-username/libaom

git submodule update --init --recursive
```

4. Create feature branches in both the main repo and any submodules you're modifying:

```bash
# Main repo
git checkout -b feature/my-feature

# FFmpeg submodule (if needed)
cd external/ffmpeg
git checkout -b feature/my-feature
cd ../..

# libaom submodule (if needed)
cd external/libaom
git checkout -b feature/my-feature
cd ../..
```

### Making Changes

1. Make your changes to the submodule(s) first, commit and push to your fork
2. Update the main repository to reference your submodule commits
3. Make any changes needed in the main repository
4. Push all changes to your forks
5. Create a pull request on this repository

### Pull Request Guidelines

- Provide a clear description of the changes
- Reference any related issues
- Ensure all tests pass (see [DEVELOPERS.md](DEVELOPERS.md) for testing instructions)
- Follow the existing code style

## License Compliance

This project is licensed under the **GNU Lesser General Public License v2.1 or later (LGPL-2.1-or-later)**.

All new source files must include the appropriate copyright header:

```c
/**
 * @file filename.c
 * @author Your Name
 * @copyright Copyright (c) 2024-2025, AVEQ GmbH. Copyright (c) 2024-2025,
 * videoparser-ng contributors.
 */
```

For modifications to existing files, add your copyright line while preserving existing ones.

Note that:

- FFmpeg is licensed under LGPL 2.1+ (with some optional GPL components not actively enabled in our build)
- libaom is licensed under the BSD 2-Clause License

When adding new dependencies:

1. Ensure the license is compatible with LGPL-2.1+
2. Document the dependency and its license in `LICENSE.txt`
3. Preserve all copyright notices and license headers

> [!IMPORTANT]
> By submitting a pull request, you agree that your contributions will be licensed under the same license as the project (LGPL-2.1-or-later).

## Questions?

If you have any questions, please create an issue in the repository!
