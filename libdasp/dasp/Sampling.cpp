/*
 * Sampling.cpp
 *
 *  Created on: Mar 25, 2012
 *      Author: david
 */

//------------------------------------------------------------------------------
#include "Superpixels.hpp"
#include "tools/BlueNoise.hpp"
#include "tools/Mipmaps.hpp"
#include <boost/random.hpp>
//------------------------------------------------------------------------------
namespace dasp {
//------------------------------------------------------------------------------

boost::mt19937 cGlobalRndRng;

void SetRandomNumberSeed(unsigned int x)
{
	cGlobalRndRng.seed(x);
}

std::vector<Seed> FindSeedsGrid(const ImagePoints& points, const Parameters& opt)
{
	unsigned int width = points.width();
	unsigned int height = points.height();
	const float d = std::sqrt(float(width*height) / float(opt.count));
	const unsigned int Nx = (unsigned int)std::ceil(float(width) / d);
	const unsigned int Ny = (unsigned int)std::ceil(float(height) / d);
	const unsigned int Dx = (unsigned int)std::floor(float(width) / float(Nx));
	const unsigned int Dy = (unsigned int)std::floor(float(height) / float(Ny));
	const unsigned int Hx = Dx/2;
	const unsigned int Hy = Dy/2;
	const float S = float(std::max(Dx, Dy));

//	// assume that everything has a distance of 1.5 meters
//	const float cAssumedDistance = 1.5f;
//	unsigned int R = opt.camera.focal / cAssumedDistance * opt.base_radius;
//	unsigned int Dx = R;
//	unsigned int Dy = R;
//	unsigned int Hx = Dx/2;
//	unsigned int Hy = Dy/2;
//	unsigned int Nx = points.width() / Dx;
//	unsigned int Ny = points.height() / Dy;

	// space seeds evently
	std::vector<Seed> seeds;
	seeds.reserve(Nx*Ny);
	for(unsigned int iy=0; iy<Ny; iy++) {
		unsigned int y = Hy + Dy * iy;
		for(unsigned int ix=0; ix<Nx; ix++) {
			unsigned int x = Hx + Dx * ix;
			Seed p;
			p.x = x;
			p.y = y;
			p.scala = S;
			seeds.push_back(p);
		}
	}

	return seeds;
}

std::vector<Seed> FindSeedsDepthRandom(const ImagePoints& points, const slimage::Image1f& density, const Parameters& opt)
{
	assert(false && "FindSeedsRandom: Not implemented!");
//	constexpr float cCameraFocal = 25.0f;
//	// for each pixel compute number of expected clusters
//	std::vector<float> cdf(points.size());
//	for(unsigned int i=0; i<points.size(); i++) {
//		uint16_t zi = *(depth->begin() + i);
//		float z = 0.001f * float(zi);
//		float v = z * z;
//		cdf[i] = (i == 0) ? v : (v + cdf[i-1]);
//	}
//	float sum = cdf[cdf.size() - 1];
//	boost::uniform_real<float> rnd(0.0f, sum);
//	boost::variate_generator<boost::mt19937&, boost::uniform_real<float> > die(cGlobalRndRng, rnd);
//	// randomly pick clusters based on probability
//	std::vector<Seed> seeds;
//	seeds.reserve(opt.cluster_count);
//	while(seeds.size() < opt.cluster_count) {
//		Seed s;
//		float rnd = die();
//		auto it = std::lower_bound(cdf.begin(), cdf.end(), rnd);
//		unsigned int index = it - cdf.begin() - 1;
//		uint16_t zi = *(depth->begin() + index);
//		if(zi == 0) {
//			continue;
//		}
//		float z = 0.001f * float(zi);
//		s.x = index % opt.width;
//		s.y = index / opt.width;
//		s.radius = cCameraFocal / z;
//		seeds.push_back(s);
//	}
//	return seeds;
}

void FindSeedsDepthMipmap_Walk(
		const ImagePoints& points,
		std::vector<Seed>& seeds,
		const std::vector<slimage::Image1f>& mipmaps,
		int level, unsigned int x, unsigned int y)
{
	static boost::uniform_real<float> rnd(0.0f, 1.0f);
	static boost::variate_generator<boost::mt19937&, boost::uniform_real<float> > die(cGlobalRndRng, rnd);

	const slimage::Image1f& mm = mipmaps[level];

	float v = 1.03f * mm(x, y); // TODO nice magic constant

	if(v > 1.0f && level > 1) { // do not access mipmap 0!
		// go down
		FindSeedsDepthMipmap_Walk(points, seeds, mipmaps, level - 1, 2*x,     2*y    );
		FindSeedsDepthMipmap_Walk(points, seeds, mipmaps, level - 1, 2*x,     2*y + 1);
		FindSeedsDepthMipmap_Walk(points, seeds, mipmaps, level - 1, 2*x + 1, 2*y    );
		FindSeedsDepthMipmap_Walk(points, seeds, mipmaps, level - 1, 2*x + 1, 2*y + 1);
	}
	else {
		if(die() <= v)
		{
			unsigned int half = (1 << (level - 1));
			// create seed in the middle
			int sx0 = static_cast<int>((x << level) + half);
			int sy0 = static_cast<int>((y << level) + half);
			// add random offset to add noise

			boost::variate_generator<boost::mt19937&, boost::uniform_int<> > delta(
					cGlobalRndRng, boost::uniform_int<>(-int(half), +int(half)));
			unsigned int trials = 0;
			while(trials < 100) {
				int sx = sx0 + delta();
				int sy = sy0 + delta();
				if(sx < int(points.width()) && sy < int(points.height()) && points(sx,sy).isValid()) {
					Seed s{ sx, sy, points(s.x, s.y).image_super_radius };
//					std::cout << s.x << " " << s.y << " " << s.radius << " " << points(s.x, s.y).scala << " " << points(s.x, s.y).depth << std::endl;
					//if(s.scala >= 2.0f) {
						seeds.push_back(s);
					//}
					break;
				}
				trials++;
			}
		}
	}
}

slimage::Image1f ComputeDepthDensity(const ImagePoints& points, const Parameters& opt)
{
	slimage::Image1f density(points.width(), points.height());
	for(unsigned int i=0; i<points.size(); i++) {
		const Point& p = points[i];
		/** Estimated number of super pixels at this point
		 * We assume circular superpixels. So the area A of a superpixel at
		 * point location is R*R*pi and the superpixel density is 1/A.
		 * If the depth information is invalid, the density is 0.
		 */
		float cnt = p.isInvalid() ? 0.0f : 1.0f / (M_PI * p.image_super_radius * p.image_super_radius);
		// Additionally the local gradient has to be considered.
		if(opt.gradient_adaptive_density) {
			cnt /= p.circularity;
		}
		density[i] = cnt;
	}
	return density;
}

slimage::Image1f ComputeDepthDensityFromSeeds(const std::vector<Seed>& seeds, const slimage::Image1f& target, const Parameters& opt)
{
	// range R of kernel is s.t. phi(x) >= 0.01 * phi(0) for all x <= R
	const float cRange = 1.21f; // BlueNoise::KernelFunctorInverse(0.01f);
	slimage::Image1f density(target.width(), target.height());
	density.fill(slimage::Pixel1f{0.0f});
	for(const Seed& s : seeds) {
		// seed corresponds to a kernel at position (x,y) with sigma = rho(x,y)^(-1/2)
		float rho = target(s.x, s.y);
//		if(s.x + 1 < int(target.width()) && s.y + 1 < int(target.height())) {
//			rho += target(s.x + 1, s.y) + target(s.x, s.y + 1) + target(s.x + 1, s.y + 1);
//			rho *= 0.25f;
//		}
		// dimension is 2!
		float norm = rho;
		float sigma = 1.0f / std::sqrt(norm);
		float sigma_inv_2 = norm;
		// kernel influence range
		int R = static_cast<int>(std::ceil(cRange * sigma));
		int xmin = std::max<int>(s.x - R, 0);
		int xmax = std::min<int>(s.x + R, int(target.width()) - 1);
		int ymin = std::max<int>(s.y - R, 0);
		int ymax = std::min<int>(s.y + R, int(target.height()) - 1);
		for(int yi=ymin; yi<=ymax; yi++) {
			for(int xi=xmin; xi<=xmax; xi++) {
				float d = static_cast<float>(Square(xi - s.x) + Square(yi - s.y));
				float delta = norm * BlueNoise::KernelFunctorSquare(d*sigma_inv_2);
				density(xi, yi) += delta;
			}
		}
	}
	return density;
}

//slimage::Image1d ComputeDepthDensityDouble(const ImagePoints& points)
//{
//	slimage::Image1d num(points.width(), points.height());
//	for(unsigned int i=0; i<points.size(); i++) {
//		num[i] = double(points[i].estimatedCount());
//	}
//	return num;
//}

std::vector<Seed> FindSeedsDepthMipmap(const ImagePoints& points, const slimage::Image1f& density, const Parameters& opt)
{
	// compute mipmaps
	std::vector<slimage::Image1f> mipmaps = Mipmaps::ComputeMipmaps(density, 1);
	// now create pixel seeds
	std::vector<Seed> seeds;
	FindSeedsDepthMipmap_Walk(points, seeds, mipmaps, mipmaps.size() - 1, 0, 0);
	return seeds;
}

std::vector<Seed> FindSeedsDepthBlue(const ImagePoints& points, const slimage::Image1f& density, const Parameters& opt)
{
	// compute blue noise points
	std::vector<BlueNoise::Point> pnts = BlueNoise::Compute(density);
	// convert to seeds
	std::vector<Seed> seeds;
	seeds.reserve(pnts.size());
	for(unsigned int i=0; i<pnts.size(); i++) {
		Seed s;
		s.x = std::round(pnts[i].x);
		s.y = std::round(pnts[i].y);
		if(0 <= s.x && s.x < int(points.width()) && 0 <= s.y && s.y < int(points.height())) {
			s.scala = points(s.x, s.y).image_super_radius;
			seeds.push_back(s);
		}
	}
	return seeds;
}

std::vector<Seed> FindSeedsDepthFloyd(const ImagePoints& points, const slimage::Image1f& density, const Parameters& opt)
{
	std::vector<Seed> seeds;
	for(unsigned int y=0; y<density.height() - 1; y++) {
		density(1,y) += density(0,y);
		for(unsigned int x=1; x<density.width() - 1; x++) {
			float v = density(x,y);
			if(v >= 0.5f) {
				v -= 1.0f;
				Seed s;
				s.x = x;
				s.y = y;
				s.scala = points(s.x, s.y).image_super_radius;
				seeds.push_back(s);
			}
			density(x+1,y  ) += 7.0f / 16.0f * v;
			density(x-1,y+1) += 3.0f / 16.0f * v;
			density(x  ,y+1) += 5.0f / 16.0f * v;
			density(x+1,y+1) += 1.0f / 16.0f * v;
		}
		// carry over
		density(0, y+1) += density(density.width()-1, y);
	}
	return seeds;
}

void FindSeedsDeltaMipmap_Walk(const ImagePoints& points, std::vector<Seed>& seeds, const std::vector<slimage::Image2f>& mipmaps, int level, unsigned int x, unsigned int y)
{
	static boost::uniform_real<float> rnd(0.0f, 1.0f);
	static boost::variate_generator<boost::mt19937&, boost::uniform_real<float> > die(cGlobalRndRng, rnd);

	const slimage::Image2f& mm = mipmaps[level];

	float v_sum = mm(x, y)[0];
	float v_abs = std::abs(v_sum);// mm(x, y)[1];

	if(v_abs > 1.0f && level > 1) { // do not access mipmap 0!
		// go down
		FindSeedsDeltaMipmap_Walk(points, seeds, mipmaps, level - 1, 2*x,     2*y    );
		FindSeedsDeltaMipmap_Walk(points, seeds, mipmaps, level - 1, 2*x,     2*y + 1);
		FindSeedsDeltaMipmap_Walk(points, seeds, mipmaps, level - 1, 2*x + 1, 2*y    );
		FindSeedsDeltaMipmap_Walk(points, seeds, mipmaps, level - 1, 2*x + 1, 2*y + 1);
	}
	else {
		if(die() < v_abs)
		{
			unsigned int half = (1 << (level - 1));
			// create seed in the middle
			int sx = (x << level) + half;
			int sy = (y << level) + half;
			// add random offset to add noise
			boost::variate_generator<boost::mt19937&, boost::uniform_int<> > delta(
					cGlobalRndRng, boost::uniform_int<>(-int(half/2), +int(half/2)));
			sx += delta();
			sy += delta();

			if(v_sum > 0.0f) {
				// create seed in the middle
				if(sx < int(points.width()) && sy < int(points.height())) {
					float scala = points(sx, sy).image_super_radius;
					Seed s{sx, sy, scala};
//					std::cout << s.x << " " << s.y << " " << scala << std::endl;
					//if(s.scala >= 2.0f) {
						seeds.push_back(s);
#ifdef CREATE_DEBUG_IMAGES
						slimage::Image3ub debug = slimage::Ref<unsigned char, 3>(sDebugImages["seeds_delta"]);
						debug(sx, sy) = slimage::Pixel3ub{{255,0,0}};
#endif
					//}
				}
			}
			else {
				// find nearest
				int best_dist = 1000000000;
				std::size_t best_index = 0;
				for(std::size_t i=0; i<seeds.size(); i++) {
					const Seed& s = seeds[i];
					int dist =  Square(sx - s.x) + Square(sy - s.y);
					if(dist < best_dist) {
						best_dist = dist;
						best_index = i;
					}
				}
//				auto it = std::min_element(seeds.begin(), seeds.end(), [sx, sy](const Seed& a, const Seed& b) {
//					return Square(sx - a.x) + Square(sy - a.y) < Square(sx - b.x) + Square(sy - b.y);
//				});
				// delete nearest seed
//				seeds.erase(it);
#ifdef CREATE_DEBUG_IMAGES
				slimage::Image3ub debug = slimage::Ref<unsigned char, 3>(sDebugImages["seeds_delta"]);
				debug(seeds[best_index].x, seeds[best_index].y) = slimage::Pixel3ub{{0,255,255}};
#endif
				seeds.erase(seeds.begin() + best_index);
			}
		}
	}
}

std::vector<Seed> FindSeedsDelta(const ImagePoints& points, const std::vector<Seed>& old_seeds, const ImagePoints& old_points, const slimage::Image1f& density_new, const Parameters& opt)
{
#ifdef CREATE_DEBUG_IMAGES
	slimage::Image3ub debug(points.width(), points.height(), {{0,0,0}});
	sDebugImages["seeds_delta"] = slimage::Ptr(debug);
#endif

	// compute old density
	slimage::Image1f density_old = ComputeDepthDensityFromSeeds(old_seeds, density_new, opt);
	// difference
	slimage::Image1f density_delta = density_new - density_old;
	// compute mipmaps
	std::vector<slimage::Image2f> mipmaps = Mipmaps::ComputeMipmapsWithAbs(density_delta, 1);
	// we need to add and delete points!
	std::vector<Seed> seeds = old_seeds;
	FindSeedsDeltaMipmap_Walk(points, seeds, mipmaps, mipmaps.size() - 1, 0, 0);
//	std::cout << "Delta seeds: " << int(seeds.size()) - int(old_seeds.size()) << std::endl;
	// give all seed points the correct scala
	for(Seed& s : seeds) {
		s.scala = points(s.x, s.y).image_super_radius;
	}
	// delete seeds with low scala
	std::vector<Seed> ok_size_seeds;
	ok_size_seeds.reserve(seeds.size());
	for(Seed& s : seeds) {
		if(s.scala >= 2.0f) {
			ok_size_seeds.push_back(s);
		}
	}
	return ok_size_seeds;
}

std::vector<Seed> Clustering::FindSeeds()
{
	switch(opt.seed_mode) {
	case SeedModes::EquiDistant:
		return FindSeedsGrid(points, opt);
	case SeedModes::DepthShooting:
		return FindSeedsDepthRandom(points, density, opt);
	case SeedModes::DepthMipmap:
		return FindSeedsDepthMipmap(points, density, opt);
	case SeedModes::DepthBlueNoise:
		return FindSeedsDepthBlue(points, density, opt);
	case SeedModes::DepthFloyd:
		return FindSeedsDepthFloyd(points, density, opt);
	default:
		assert(false && "FindSeeds: Unkown mode!");
	};
}

std::vector<Seed> Clustering::FindSeeds(const ImagePoints& old_points)
{
	if(opt.seed_mode == SeedModes::Delta) {
		seeds_previous = getClusterCentersAsSeeds();
		return FindSeedsDelta(points, seeds_previous, old_points, density, opt);
	}
	else {
		return FindSeeds();
	}
}

//------------------------------------------------------------------------------
}
//------------------------------------------------------------------------------

