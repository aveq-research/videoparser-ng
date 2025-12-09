# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](http://keepachangelog.com/en/1.0.0/)
and this project adheres to [Semantic Versioning](http://semver.org/spec/v2.0.0.html).

<!-- insertion marker -->
## [v0.1.1](https://github.com/aveq-research/videoparser-ng/releases/tag/v0.1.1) - 2025-12-09

<small>[Compare with v0.1.0](https://github.com/aveq-research/videoparser-ng/compare/v0.1.0...v0.1.1)</small>

### Continuous Integration

- add release build workflow ([3eebdda](https://github.com/aveq-research/videoparser-ng/commit/3eebdda3d5f8473ab96ab664c3329b507b032dbc) by Werner Robitza).

### Bug Fixes

- uint --> unsigned int for musl compat ([76e322b](https://github.com/aveq-research/videoparser-ng/commit/76e322b007ee8adfc9fb7ba61e0eb256a6b4f78e) by Werner Robitza).

### Misc

- bump version to 0.1.1 ([9f207ff](https://github.com/aveq-research/videoparser-ng/commit/9f207ff0235ac92da801f2f236f36c8cb0f0c86a) by Werner Robitza).

## [v0.1.0](https://github.com/aveq-research/videoparser-ng/releases/tag/v0.1.0) - 2025-12-09

<small>[Compare with v0.0.2](https://github.com/aveq-research/videoparser-ng/compare/v0.0.2...v0.1.0)</small>

### Continuous Integration

- update workflow ([bdbac66](https://github.com/aveq-research/videoparser-ng/commit/bdbac6619f0d002fe5739e9a195120b21116facd) by Werner Robitza).

### Features

- AV1 motion support ([4540fab](https://github.com/aveq-research/videoparser-ng/commit/4540fab1d550d3a63467e960d2007a1e73c37571) by Werner Robitza).
- add vendored aom, update dockerfile ([ff525f1](https://github.com/aveq-research/videoparser-ng/commit/ff525f17ceb093de951ef9ebd22c1c813aa79405) by Werner Robitza).
- motion and MB ([1fbbafa](https://github.com/aveq-research/videoparser-ng/commit/1fbbafaacf4602825e5ea24ef56745822587ab65) by Werner Robitza).
- motion and coef bit count ([1e1abcf](https://github.com/aveq-research/videoparser-ng/commit/1e1abcfc95c64b76b1b94908e77a469059e30eec) by Werner Robitza).

### Bug Fixes

- dockerfile ([c339628](https://github.com/aveq-research/videoparser-ng/commit/c3396287ce2d113b878ddb4d7833534f6483283e) by Werner Robitza).
- coef_bit_count ([7159c0a](https://github.com/aveq-research/videoparser-ng/commit/7159c0a81f779cc55e1f328ab7c0fde1f17fb334) by Werner Robitza).
- motion_bit_count and mv_coded_count ([3421102](https://github.com/aveq-research/videoparser-ng/commit/3421102933b4ce29ba43e27d949fc731b1b60829) by Werner Robitza).

### Misc

- bump version to 0.1.0 ([2de645d](https://github.com/aveq-research/videoparser-ng/commit/2de645da67e9174a7b9652bdb77d036084b189f7) by Werner Robitza).

## [v0.0.2](https://github.com/aveq-research/videoparser-ng/releases/tag/v0.0.2) - 2025-12-08

<small>[Compare with v0.0.1](https://github.com/aveq-research/videoparser-ng/compare/v0.0.1...v0.0.2)</small>

### Chore

- update release script ([d174edf](https://github.com/aveq-research/videoparser-ng/commit/d174edfd09f2a192e87d67da91dda4e3608055e4) by Werner Robitza).
- update ffmpeg ([4f0fd93](https://github.com/aveq-research/videoparser-ng/commit/4f0fd93b4b7fadaa540505829e39a25e9c1818f2) by Werner Robitza).

### Tests

- update test script with better logs ([b9a6d83](https://github.com/aveq-research/videoparser-ng/commit/b9a6d83c8bab6cd3e23adc2ef199d0b026775c08) by Werner Robitza).

### Tests

- update test script with better logs ([b9a6d83](https://github.com/aveq-research/videoparser-ng/commit/b9a6d83c8bab6cd3e23adc2ef199d0b026775c08) by Werner Robitza).

### Misc

- bump version to 0.0.2 ([105be3b](https://github.com/aveq-research/videoparser-ng/commit/105be3bdd17ea2a36974e2968a2ed2ad7f545d9e) by Werner Robitza).
- Update ffmpeg to latest version ([a6c5662](https://github.com/aveq-research/videoparser-ng/commit/a6c5662d5efc14b3f155e31d68e107d603ac4cee) by Werner Robitza).
- fix git hook, should fix #11 ([5bc0928](https://github.com/aveq-research/videoparser-ng/commit/5bc0928bb2cc2d0fad81614d16bf9ef8da5c7b36) by Werner Robitza).
- change git submodule URL, fixes #12 ([d3ea3fe](https://github.com/aveq-research/videoparser-ng/commit/d3ea3fe5c55b019f324c117618f7d697ea2f1a96) by Werner Robitza).
- fix docker builds ([da1fbdc](https://github.com/aveq-research/videoparser-ng/commit/da1fbdce1685e3e3c006a8586965614dc9927fcf) by Werner Robitza).
- update github actions scripts ([976ac0f](https://github.com/aveq-research/videoparser-ng/commit/976ac0ff75d6bc6bb2aaa32c6b60f45f5e7dd954) by Werner Robitza).
- add versioning ([c6bfb6f](https://github.com/aveq-research/videoparser-ng/commit/c6bfb6ff4e65db4b0193e8e4cc1c9d341b72a37f) by Werner Robitza).

## [v0.0.1](https://github.com/aveq-research/videoparser-ng/releases/tag/v0.0.1) - 2025-04-08

<small>[Compare with first commit](https://github.com/aveq-research/videoparser-ng/compare/f6fc54ba6fb76e4de486106675b403719c8669dd...v0.0.1)</small>

### Misc

- update ffmpeg, fix issue with mv type casting ([8eaccdf](https://github.com/aveq-research/videoparser-ng/commit/8eaccdfec58bb9754832f9ccadd627479176f7ff) by Werner Robitza).
- only install git hook if git directory exists, fixes #11 ([ea7d500](https://github.com/aveq-research/videoparser-ng/commit/ea7d500bea1f097e840caee055d6ed1a450ae945) by Werner Robitza).
- documennt public API ([3c954e8](https://github.com/aveq-research/videoparser-ng/commit/3c954e8481c8fb9ae87afa80d04315cc3648cbe6) by Werner Robitza).
- change interface to use const char instead of std::string ([43c52d6](https://github.com/aveq-research/videoparser-ng/commit/43c52d62de07cc392a0e0031fca1c77fba4961d6) by Werner Robitza).
- change interface for sequence info ([0502489](https://github.com/aveq-research/videoparser-ng/commit/050248978abb15bae7981b3ebfc584c67a76befb) by Werner Robitza).
- initial motion vector implementation ([124711b](https://github.com/aveq-research/videoparser-ng/commit/124711b77167bfe85dd750be3008058dfcb28e04) by Werner Robitza).
- udpate example in readme ([9d8e867](https://github.com/aveq-research/videoparser-ng/commit/9d8e867d48f70ed9143f0b2f5faa4555573f8794) by Werner Robitza).
- update readme ([4ce9483](https://github.com/aveq-research/videoparser-ng/commit/4ce94833a4fcdda1a96a5220dce0f38db191750b) by Werner Robitza).
- point to ffmpeg branch ([d64aa54](https://github.com/aveq-research/videoparser-ng/commit/d64aa54fe202d10346f882d508f383cc50fe7b4b) by Werner Robitza).
- print sequence info before frame info ([8325f9b](https://github.com/aveq-research/videoparser-ng/commit/8325f9b58bd0e0ce811923c564b0dec1dbf21d72) by Werner Robitza).
- cancel previous ci runs ([afb2f18](https://github.com/aveq-research/videoparser-ng/commit/afb2f18d43e9824fe8a7bf5652ea4e489a06c198) by Werner Robitza).
- add nasm ([8866fde](https://github.com/aveq-research/videoparser-ng/commit/8866fdef0d6cdd77aa83c0a4b7513f6f1518127c) by Werner Robitza).
- simplify interface once more, fix QP issue ([c27b1a0](https://github.com/aveq-research/videoparser-ng/commit/c27b1a080323b5a5975f27b01c4cdef166e44b2c) by Werner Robitza).
- simplify interface ([2995a00](https://github.com/aveq-research/videoparser-ng/commit/2995a00add2d989d45b62679ea00753f374150ff) by Werner Robitza).
- fix docker build with SRC_PATH ([0cb35e1](https://github.com/aveq-research/videoparser-ng/commit/0cb35e141c8f793d0304142af196d97d810f7582) by Werner Robitza).
- add debug config ([6b7193c](https://github.com/aveq-research/videoparser-ng/commit/6b7193c497aa78fee2b1941eb83e4640144846f6) by Werner Robitza).
- fix checkout for docker ([6a1e643](https://github.com/aveq-research/videoparser-ng/commit/6a1e643387665bd3ab517b2b428941cbe5f531cf) by Werner Robitza).
- add docker build workflow ([fface3b](https://github.com/aveq-research/videoparser-ng/commit/fface3b3b2eeb1e311e9245129d116c6175c5f2c) by Werner Robitza).
- update dev notes ([a037eb3](https://github.com/aveq-research/videoparser-ng/commit/a037eb37b8ba794ff9298de6b227acfd5b9f2f3b) by Werner Robitza).
- fix license file name ([9ff537e](https://github.com/aveq-research/videoparser-ng/commit/9ff537e4c61470d2d44e61a0722d85b8d8332dc9) by Werner Robitza).
- WIP: docker build ([253effc](https://github.com/aveq-research/videoparser-ng/commit/253effc3b9931f8c5fcd9b54a0892e6b390c36b3) by Werner Robitza).
- fix wrong aom path ([5a37232](https://github.com/aveq-research/videoparser-ng/commit/5a3723231e01e3f88368b4ff1aec374da1279dbe) by Werner Robitza).
- rename license ([7187510](https://github.com/aveq-research/videoparser-ng/commit/71875107c43147e023786be17f5ea6e9ea698b20) by Werner Robitza).
- fix AV1 support, add CLI test ([4497c76](https://github.com/aveq-research/videoparser-ng/commit/4497c763856b54117545e46d2c40cd09a29f850b) by Werner Robitza).
- document VP9 ([75f3579](https://github.com/aveq-research/videoparser-ng/commit/75f3579c8be277691942e2e7d3df737ab03d105e) by Werner Robitza).
- update to ffmpeg 7.1 ([f9e8138](https://github.com/aveq-research/videoparser-ng/commit/f9e81381235ce7b859dc1ac41a8ef86ec873e9a9) by Werner Robitza).
- udpate ffmpeg to 7.1 master ([cd4cd40](https://github.com/aveq-research/videoparser-ng/commit/cd4cd40e9ae2c500911dbd24502fbd99af158703) by Werner Robitza).
- further reduce parsers needed ([93d2c66](https://github.com/aveq-research/videoparser-ng/commit/93d2c6666fa510db2933c220dec552a065208b18) by Werner Robitza).
- disable tiff decoder ([3522d0e](https://github.com/aveq-research/videoparser-ng/commit/3522d0e2a7ac546235fd5a667376f9bc2d585791) by Werner Robitza).
- disable some hardware support ([775d6be](https://github.com/aveq-research/videoparser-ng/commit/775d6be1a5569d822cf778955ff1149f0ecb617a) by Werner Robitza).
- fix ffmpeg build script ([357ac80](https://github.com/aveq-research/videoparser-ng/commit/357ac8002b67e8139dfda74a69cbe8548f82818f) by Werner Robitza).
- add missing bz2 requirement ([a8ceea9](https://github.com/aveq-research/videoparser-ng/commit/a8ceea96f8d7a0b182c05494c065c5577349e830) by Werner Robitza).
- remove iconv requirement ([efe2b65](https://github.com/aveq-research/videoparser-ng/commit/efe2b65a082af09625ba533a93c7a46af11907aa) by Werner Robitza).
- switch submodule to git+ssh ([dca9427](https://github.com/aveq-research/videoparser-ng/commit/dca942766d1791cf41902a6cc14fad2c330745bf) by Werner Robitza).
- add test fixture link ([f35f8be](https://github.com/aveq-research/videoparser-ng/commit/f35f8beeee74ea9596c468a027244331558d13da) by Werner Robitza).
- update submodule URL ([f611d66](https://github.com/aveq-research/videoparser-ng/commit/f611d66957e163896d7b2b20327c17a590a69c91) by Werner Robitza).
- update gitignore ([7c238b3](https://github.com/aveq-research/videoparser-ng/commit/7c238b3bca32b3d3f15e3900a8c32d7c783f8845) by Werner Robitza).
- update ffmpeg rebase script ([7a1f4b5](https://github.com/aveq-research/videoparser-ng/commit/7a1f4b5cb645b9846a4b4c5e31e109a4b7abeb1e) by Werner Robitza).
- update ffmpeg to latest version ([aad8269](https://github.com/aveq-research/videoparser-ng/commit/aad82699143a6c5eb9c95fe877f311569d8058ee) by Werner Robitza).
- update ([77d73a5](https://github.com/aveq-research/videoparser-ng/commit/77d73a540d7caa46d2adc91fd7ce477bf26b893f) by Werner Robitza).
- update rebase script ([38f2482](https://github.com/aveq-research/videoparser-ng/commit/38f2482f46d3c1f176635b5c59e44d7fb04cbff7) by Werner Robitza).
- update docs ([9a0ece0](https://github.com/aveq-research/videoparser-ng/commit/9a0ece05e8bb53ab70fa81ff096f2732d5a1bb79) by Werner Robitza).
- update test ([215aa36](https://github.com/aveq-research/videoparser-ng/commit/215aa3657ff4e38f6a8f453fbc86710b6acc3a97) by Werner Robitza).
- add test ([567779e](https://github.com/aveq-research/videoparser-ng/commit/567779e4ab7db5ccd9d09cef0326049787dbbfb5) by Werner Robitza).
- update scripts ([3c868dd](https://github.com/aveq-research/videoparser-ng/commit/3c868dd1d31c901ca43ff80810a288d430e8e56d) by Werner Robitza).
- implement QPs ([7053953](https://github.com/aveq-research/videoparser-ng/commit/705395328273fdefd03cf57d3e4400c28db904e8) by Werner Robitza).
- include from ffmpeg ([934f82a](https://github.com/aveq-research/videoparser-ng/commit/934f82adc8eda07b46d9a0327d477cdffc15d784) by Werner Robitza).
- initial commit ([f6fc54b](https://github.com/aveq-research/videoparser-ng/commit/f6fc54ba6fb76e4de486106675b403719c8669dd) by Werner Robitza).

