// Copyright (c) 2025 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <pyxel/math/glm.hpp>
#include <pyxel/rng/engine.hpp>
#include <string>
#include <vector>

namespace pyxel::rng {
	class perlin {
	public:
		explicit perlin(engine& rng);
		float noise(float x, float y, float z) const noexcept;

	private:
		std::vector<unsigned> perm{};
		std::vector<glm::vec3> gradient{};

		// lattice coordinates are wrapped into [0, 256) by noise()
		float dot_at(unsigned x, unsigned y, unsigned z, glm::vec3 const& pt) const noexcept;

		unsigned index_for(unsigned x, unsigned y, unsigned z) const noexcept {
			return perm[(x + perm[(y + perm[z & 0xFF]) & 0xFF]) & 0xFF];
		}
	};

	class octaves {
	public:
		explicit octaves(engine& rng) : source_{rng} {}

		float noise(float x, float y, float z) const noexcept;

		void set_octaves(unsigned) noexcept;
		void set_persistence(float value) noexcept;
		void set_lacunarity(float value) noexcept;
		void set_frequency(float value) noexcept;
		void set_amplitude(float value) noexcept;

	private:
		perlin source_;

		unsigned octaves_{10};
		float persistence_{.5f};
		float lacunarity_{2.0f};
		float frequency_{1.0f};
		float amplitude_{1.0f};
	};

}  // namespace pyxel::rng
