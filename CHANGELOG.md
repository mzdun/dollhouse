# Changelog

All notable changes to this project will be documented in this file. See [Conventional Commits](https://www.conventionalcommits.org/en/v1.0.0/) and [COMMITS.md](COMMITS.md) for commit guidelines.

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
