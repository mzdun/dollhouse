// Copyright (c) 2025 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <cmath>
#include <limits>
#include <pyxel/rng/noise.hpp>
#include <utility>

namespace pyxel::rng {
	static constexpr auto sample_size = 256u;

	perlin::perlin(engine& entropy) {
		perm.resize(sample_size);
		gradient.resize(sample_size);

		auto grad_rng = entropy.spinner(-1.0f, 1.0f);
		for (unsigned index = 0; index < sample_size; ++index) {
			perm[index] = index;

			gradient[index].x = grad_rng.spin();
			gradient[index].y = grad_rng.spin();
			gradient[index].z = grad_rng.spin();
		}

		// 256 faces: each roll is the lowest byte of one word, never rejected
		auto pos = entropy.dice(0, sample_size - 1);
		for (unsigned index = 0; index < sample_size; ++index) {
			auto const opposite = static_cast<unsigned>(pos.roll());
			std::swap(perm[opposite], perm[index]);
		}

		// Normalize gradient vectors
		for (auto& grad : gradient) {
			float norm = std::sqrt(grad.x * grad.x + grad.y * grad.y + grad.z * grad.z);
			if (norm == 0.0f) {
				grad.x = 1.0f;
				grad.y = 0.0f;
				grad.z = 0.0f;
			} else {
				grad.x /= norm;
				grad.y /= norm;
				grad.z /= norm;
			}
		}
	}

	namespace {
		// lattice cell of a coordinate: floor(v) wrapped into [0, 256), the
		// period of the permutation, and the offset of v within the cell;
		// both are exact and defined for every finite v, so the noise is
		// continuous across zero and repeats every 256 units. Infinities and
		// NaN land in cell 0 with a NaN offset, making the noise NaN
		struct cell {
			unsigned index;
			float offset;
		};

		cell cell_of(float v) noexcept {
			auto const floor = std::floor(v);
			if (!std::isfinite(floor)) return {0u, std::numeric_limits<float>::quiet_NaN()};

			auto wrapped = std::fmod(floor, static_cast<float>(sample_size));  // exact, keeps the sign
			if (wrapped < 0) wrapped += static_cast<float>(sample_size);       // exact, as it is an integer
			return {static_cast<unsigned>(wrapped), v - floor};
		}
	}  // namespace

	float perlin::noise(float x, float y, float z) const noexcept {
		// unit cube surrounding the sample point, and the position of the
		// point inside it
		auto const [x0, px0] = cell_of(x);
		auto const [y0, py0] = cell_of(y);
		auto const [z0, pz0] = cell_of(z);
		auto const x1 = (x0 + 1) & 0xFF;
		auto const y1 = (y0 + 1) & 0xFF;
		auto const z1 = (z0 + 1) & 0xFF;
		auto const px1 = px0 - 1.0f;
		auto const py1 = py0 - 1.0f;
		auto const pz1 = pz0 - 1.0f;

		// dot products of the corner gradients with the vectors from the
		// corners to the sample point
		auto const d000 = dot_at(x0, y0, z0, {px0, py0, pz0});
		auto const d001 = dot_at(x1, y0, z0, {px1, py0, pz0});
		auto const d010 = dot_at(x0, y1, z0, {px0, py1, pz0});
		auto const d011 = dot_at(x1, y1, z0, {px1, py1, pz0});
		auto const d100 = dot_at(x0, y0, z1, {px0, py0, pz1});
		auto const d101 = dot_at(x1, y0, z1, {px1, py0, pz1});
		auto const d110 = dot_at(x0, y1, z1, {px0, py1, pz1});
		auto const d111 = dot_at(x1, y1, z1, {px1, py1, pz1});

		// Interpolate dot product values at sample point using polynomial
		// interpolation 6x^5 - 15x^4 + 10x^3
		auto const wx = ((6 * px0 - 15) * px0 + 10) * px0 * px0 * px0;
		auto const wy = ((6 * py0 - 15) * py0 + 10) * py0 * py0 * py0;
		auto const wz = ((6 * pz0 - 15) * pz0 + 10) * pz0 * pz0 * pz0;

		auto const xa = d000 + wx * (d001 - d000);
		auto const xb = d010 + wx * (d011 - d010);
		auto const xc = d100 + wx * (d101 - d100);
		auto const xd = d110 + wx * (d111 - d110);
		auto const ya = xa + wy * (xb - xa);
		auto const yb = xc + wy * (xd - xc);

		return ya + wz * (yb - ya);
	}

	float perlin::dot_at(unsigned x, unsigned y, unsigned z, glm::vec3 const& pt) const noexcept {
		auto const& grad = gradient[index_for(x, y, z)];
		return grad.x * pt.x + grad.y * pt.y + grad.z * pt.z;
	}

	float octaves::noise(float x, float y, float z) const noexcept {
		float sum = 0;
		float freq = frequency_;
		float amp = amplitude_;

		for (unsigned i = 0; i < octaves_; ++i) {
			sum += source_.noise(x * freq, y * freq, z * freq) * amp;

			freq *= lacunarity_;
			amp *= persistence_;
		}

		return sum;
	}

	void octaves::set_octaves(unsigned value) noexcept {
		if (value > 0) octaves_ = value;
	}
	void octaves::set_persistence(float value) noexcept { persistence_ = value; }
	void octaves::set_lacunarity(float value) noexcept { lacunarity_ = value; }
	void octaves::set_frequency(float value) noexcept { frequency_ = value; }
	void octaves::set_amplitude(float value) noexcept { amplitude_ = value; }
}  // namespace pyxel::rng
