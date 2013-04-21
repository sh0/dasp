
#include "PDS.hpp"
#include "Mipmaps.hpp"
#include <iostream>

namespace pds {

Eigen::Vector2f RandomCellPoint(int scale, int x, int y, float gamma)
{
	float sf = static_cast<float>(scale);
	float xf = static_cast<float>(x);
	float yf = static_cast<float>(y);
	boost::variate_generator<boost::mt19937&, boost::uniform_real<float> > delta(
			impl::Rnd(), boost::uniform_real<float>(0.5f-gamma, 0.5f+gamma));
	return Eigen::Vector2f(sf*(xf + delta()), sf*(yf + delta()));
}

void FindSeedsDepthMipmap_Walk(
		std::vector<Eigen::Vector2f>& seeds,
		const std::vector<Eigen::MatrixXf>& mipmaps,
		int level, unsigned int x, unsigned int y)
{
	static boost::uniform_real<float> rnd(0.0f, 1.0f);
	static boost::variate_generator<boost::mt19937&, boost::uniform_real<float> > die(impl::Rnd(), rnd);

	float v = mipmaps[level](x, y);

	if(v > 1.0f && level > 0) { // do not access mipmap 0!
		// go down
		FindSeedsDepthMipmap_Walk(seeds, mipmaps, level - 1, 2*x,     2*y    );
		FindSeedsDepthMipmap_Walk(seeds, mipmaps, level - 1, 2*x,     2*y + 1);
		FindSeedsDepthMipmap_Walk(seeds, mipmaps, level - 1, 2*x + 1, 2*y    );
		FindSeedsDepthMipmap_Walk(seeds, mipmaps, level - 1, 2*x + 1, 2*y + 1);
	}
	else {
		if(die() <= v) {
			seeds.push_back(
				RandomCellPoint(1 << level, x, y, 0.38f));
		}
	}
}

std::vector<Eigen::Vector2f> FindSeedsDepthMipmap(const Eigen::MatrixXf& density)
{
	// compute mipmaps
	std::vector<Eigen::MatrixXf> mipmaps = pds::tools::ComputeMipmaps(density, 1);
#ifdef CREATE_DEBUG_IMAGES
	//DebugMipmap<2>(mipmaps, "mm");
	for(unsigned int i=0; i<mipmaps.size(); i++) {
		std::string tag = (boost::format("mm_%1d") % i).str();
		DebugShowMatrix(mipmaps[i], tag);
		DebugWriteMatrix(mipmaps[i], tag);
	}
#endif
	// sample points
	std::vector<Eigen::Vector2f> seeds;
	FindSeedsDepthMipmap_Walk(seeds, mipmaps, mipmaps.size() - 1, 0, 0);
	// scale points with base constant
	for(Eigen::Vector2f& u : seeds) u *= 2.f;
	return seeds;
}

std::vector<Eigen::Vector2f> FindSeedsDepthMipmap640(const Eigen::MatrixXf& density)
{
	// compute mipmaps
	std::vector<Eigen::MatrixXf> mipmaps = pds::tools::ComputeMipmaps640x480(density);
#ifdef CREATE_DEBUG_IMAGES
	DebugMipmap<5>(mipmaps, "mm640");
	for(unsigned int i=0; i<mipmaps.size(); i++) {
		std::string tag = (boost::format("mm640_%1d") % i).str();
		DebugShowMatrix(mipmaps[i], tag);
		DebugWriteMatrix(mipmaps[i], tag);
	}
#endif
	// now create pixel seeds
	std::vector<Eigen::Vector2f> seeds;
	const unsigned int l0 = mipmaps.size() - 1;
	for(unsigned int y=0; y<mipmaps[l0].cols(); ++y) {
		for(unsigned int x=0; x<mipmaps[l0].rows(); x++) {
			FindSeedsDepthMipmap_Walk(seeds, mipmaps, l0, x, y);
		}
	}
	// scale points with base constant
	for(Eigen::Vector2f& u : seeds) u *= 5.f;
	return seeds;
}

std::vector<Eigen::Vector2f> SimplifiedPoissonDiscSamplingOld(const Eigen::MatrixXf& density)
{
	if(density.rows() == 640 && density.cols() == 480) {
		return FindSeedsDepthMipmap640(density);
	}
	else {
		return FindSeedsDepthMipmap(density);
	}
}

std::vector<Eigen::Vector2f> SimplifiedPoissonDiscSampling(const Eigen::MatrixXf& density)
{
	throw 0;
}

}
