If you want to contribute to this project, please follow these steps:

1. Fork this repository, and also fork the `ffmpeg` submodule (https://github.com/aveq-research/ffmpeg)
2. Create a new branch (e.g., `feature/my-feature`) on the main repo, and the forked `ffmpeg` repo
3. Set the forked `ffmpeg` repo as the submodule for this repo

```bash
git submodule set-url external/ffmpeg https://github.com/your-fork/ffmpeg
```

4. Make your changes to ffmpeg, and commit them, then push them to your fork
5. Any changes in the main repository should be pushed to the new feature branch on the main repo
6. Create a pull request on this repo

If you have any questions, please create an issue in the repository!
