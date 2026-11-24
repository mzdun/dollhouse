# Changelog

All notable changes to this project will be documented in this file. See [Conventional Commits](https://www.conventionalcommits.org/en/v1.0.0/) and [COMMITS.md](COMMITS.md) for commit guidelines.

## [0.4.0](https://github.com/mzdun/dollhouse/compare/v0.3.0...v0.4.0) (2026-11-24)

### New Features

- **interactions**: load active objects for running them ([61d6d85](https://github.com/mzdun/dollhouse/commit/61d6d85f566302e231c7087c21df6bb3b65273d3))
- **interactions**: check the rules on import ([6b3982a](https://github.com/mzdun/dollhouse/commit/6b3982aafe52ca93c0fb36cf41d3f08d8e7cfb95))
- **interactions**: check the rules before exporting ([df9b587](https://github.com/mzdun/dollhouse/commit/df9b5871bcd463dee9f070a731d0b74d5a347e11))
- **interactions**: read active objects from glTF 2 files ([98bc0cc](https://github.com/mzdun/dollhouse/commit/98bc0ccc89a5b3d8ef54e7643151b64c0411ffa2))
- **interactions**: write active objects to glTF 2 files ([d185f83](https://github.com/mzdun/dollhouse/commit/d185f83f815bb9cd0fb908df2f570753052bea85))
- **interactions**: pass active objects through the sink ([4d2af2b](https://github.com/mzdun/dollhouse/commit/4d2af2b0f477189cb693b73b286c751ce96920b0))
- **interactions**: let groups declare active objects ([111528e](https://github.com/mzdun/dollhouse/commit/111528e9659a8543616b47f1e6dba072ec2e8ecf))
- **interactions**: add the C++ declarations of active objects ([9e63afa](https://github.com/mzdun/dollhouse/commit/9e63afa1e2935a37c50e671ef1c2b3c6052663f8))
- **io**: import models into a loaded model ([2ed2a4d](https://github.com/mzdun/dollhouse/commit/2ed2a4d178ba3e712280913d92e2fe5cadc11e02))
- **shapes**: declare active objects from Python ([51fd94c](https://github.com/mzdun/dollhouse/commit/51fd94cb99695de3d53eb2199dfe63658e77e6f4))
- **verify**: add a tool verifying model files ([ac3e7db](https://github.com/mzdun/dollhouse/commit/ac3e7dbd14753ca189f8a0d66b55d74e5dabe343))

### Bug Fixes

- remove compilation issue on GCC ([55a574c](https://github.com/mzdun/dollhouse/commit/55a574c3bb5e9007560724e9aa4ad8cf50743f66))
- **docs**: define the order of declaration on export ([d6c3db4](https://github.com/mzdun/dollhouse/commit/d6c3db44aa06fa20c5e4b1a8f795b60f9a11a349))
- **interactions**: leave out targets with no changes left ([3b3f8f9](https://github.com/mzdun/dollhouse/commit/3b3f8f915e2840041d554a4ec23291ef0027761f))
- **interactions**: remember an imported scene only once it is complete ([2c39d30](https://github.com/mzdun/dollhouse/commit/2c39d301d804292f8747cfe5c2075e407e854f14))
- **interactions**: refuse actions with an empty label ([385ef48](https://github.com/mzdun/dollhouse/commit/385ef48b2390e85b0a199a26f3a7f1ed30b0570e))
- **interactions**: generate light names without a dot ([33dc4b5](https://github.com/mzdun/dollhouse/commit/33dc4b5bbe8c8f753d4e7756555a6584939e5cfc))
- **interactions**: tell imported scenes apart by more than the address ([0613c8d](https://github.com/mzdun/dollhouse/commit/0613c8d12c8593d271fb6fd14241adbceb9851c8))
- **interactions**: keep light names on export ([7444025](https://github.com/mzdun/dollhouse/commit/74440252d7831e07ee9aca64769872c07744cefb))

## [0.3.0](https://github.com/mzdun/dollhouse/compare/v0.2.1...v0.3.0) (2026-11-24)

### New Features

- read `PYXEL_lights_area` lights back ([66683de](https://github.com/mzdun/dollhouse/commit/66683def69e53801225057c01c546308fd6b5b2a))
- add lights export to glTF 2 JSON files ([99b7ae0](https://github.com/mzdun/dollhouse/commit/99b7ae094cd49cdcc69dec880be290a84e945c77))

### Bug Fixes

- **docs**: note down an edge case discovered by reviewer ([bc7783f](https://github.com/mzdun/dollhouse/commit/bc7783f866c2cc13a4c2f56997c3e6ac49c9879f))
- **docs**: mark the interactions spec done in the roadmap ([c60706c](https://github.com/mzdun/dollhouse/commit/c60706cf144450c9383211548979201e19fdb589))
- **docs**: make the plans schema follow the interactions spec ([f3f2659](https://github.com/mzdun/dollhouse/commit/f3f26592d8ab8260c7ce4ce2ba9dd46ff1b48b19))
- **docs**: describe the model interactions extension ([2816b8a](https://github.com/mzdun/dollhouse/commit/2816b8a588a51a69788e32cdf040302ecfb7d2c9))

## [0.2.1](https://github.com/mzdun/dollhouse/compare/v0.2.0...v0.2.1) (2026-11-24)

### Bug Fixes

- guard the paints during export ([d439e5a](https://github.com/mzdun/dollhouse/commit/d439e5ad0a090f2979b71663dbc1760365999d8b))
- refuse configuration changes while building ([f58517a](https://github.com/mzdun/dollhouse/commit/f58517aedb524ea937a873495b28e21f7c3a728a))
- let models be added and built from many threads ([df66126](https://github.com/mzdun/dollhouse/commit/df6612620dca3f1fd2d969c9dbb1f3045a0688ec))

## [0.2.0](https://github.com/mzdun/dollhouse/compare/v0.1.0...v0.2.0) (2026-11-20)

### New Features

- add str helpers, use UTF-8 for all paths ([8ff20fe](https://github.com/mzdun/dollhouse/commit/8ff20fe6759477b014bef8d51ac7dbed3d926130))
- export glb2 PYXEL_lights_area ([a2a4765](https://github.com/mzdun/dollhouse/commit/a2a4765396b5bb892d35ed1d995e58013cb1e587))
- export lights to glb2 ([646dc53](https://github.com/mzdun/dollhouse/commit/646dc538957b62389dc0163f0c120a0cde7967c7))
- **cli**: list exported models as YAML ([bb8b54b](https://github.com/mzdun/dollhouse/commit/bb8b54b58a8e7b7de9bbbbbc4cb0aeabd310a3af))

### Bug Fixes

- stop declaring the shapes module safe without the GIL ([d54d8d0](https://github.com/mzdun/dollhouse/commit/d54d8d04ba3abe989b5a412ff24841c1ad6ef4c5))
- let only the output call, which entered a scope, exit it ([099cd7b](https://github.com/mzdun/dollhouse/commit/099cd7ba445fecec9dafd4a05cff59e9d183bcc3))
- refuse models registered while Python shuts down ([aeef73b](https://github.com/mzdun/dollhouse/commit/aeef73bdfb82587b934834d4762909309a5375b0))
- reject NaN and infinity in models ([352efc8](https://github.com/mzdun/dollhouse/commit/352efc87cdc8bde8c251ab44ba771193cbd5274d))
- stop CSG stack overflow on dense solids ([a7bd294](https://github.com/mzdun/dollhouse/commit/a7bd2942a1fa7d0212fc38ac4ecb606ffec2c55c))
- do not allow the model files to escape the output directory ([4639aa2](https://github.com/mzdun/dollhouse/commit/4639aa2a0d433fd222f313a7b96000206de214c7))
- **cli**: load scripts from their files ([2dfbe6d](https://github.com/mzdun/dollhouse/commit/2dfbe6dfa1690f81f1fee8aae282d92d68fd9a3f))
- **docs**: split the roadmap into runnable stages ([6074f2e](https://github.com/mzdun/dollhouse/commit/6074f2ec4829ac98bada87cab85d30d4a40ad052))
- **docs**: describe the version 1 project format ([5fd92d3](https://github.com/mzdun/dollhouse/commit/5fd92d316265251da741964dbf5eee2aef21527f))

## [0.1.0](https://github.com/mzdun/dollhouse/compare/v0.0.0...v0.1.0) (2026-11-16)

### New Features

- initial light export ([35e6712](https://github.com/mzdun/dollhouse/commit/35e671279619dd6b44939f6c80c9f2e0d97bf2a5))
- add light to a group in a model ([9907709](https://github.com/mzdun/dollhouse/commit/9907709ad23af9720e697cfbc425986ec19ea510))
- keep transformations of named groups in exported nodes ([5f20272](https://github.com/mzdun/dollhouse/commit/5f20272103a8ff48b95445187ff05ab11b39f575))
- add light definition ([96934ed](https://github.com/mzdun/dollhouse/commit/96934edcb5595665f1cb8a28de48c69a7c8cfa17))
- run a set of scripts through python -m shapes ([d193f67](https://github.com/mzdun/dollhouse/commit/d193f674334fed57b550160cf22da1a8dc946e37))
- export models with diffusion materials ([979390f](https://github.com/mzdun/dollhouse/commit/979390f325bb77112ea84c1d6754e039c4124957))
- provide python bindings ([33db609](https://github.com/mzdun/dollhouse/commit/33db609d4715b138460cd685155524f867a65770))
- pyxel-model hierarchy and diffusion materials ([6f5faf2](https://github.com/mzdun/dollhouse/commit/6f5faf268d1c7f55a837e699afb9b77d7d0c2e1a))

### Bug Fixes

- stabilize tests by modifying compiler params ([e2ab18b](https://github.com/mzdun/dollhouse/commit/e2ab18b0cae3a232b12d89e1d7aa8ad90ea2141d))
