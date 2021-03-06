#include <density/PointDensity.hpp>
#include <dasp/Superpixels.hpp>
#include <dasp/Segmentation.hpp>
#include <dasp/impl/Sampling.hpp>
#include <Slimage/IO.hpp>
#include <Slimage/Slimage.hpp>
#include <boost/program_options.hpp>
#include <iostream>

dasp::Superpixels ComputeClusteringDasp(slimage::Image3ub color, slimage::Image1ui16 depth, float R, unsigned int cnt)
{
	std::cout << "n_given=" << cnt << std::flush;
	dasp::Parameters opt;
	opt.camera = dasp::Camera{320.0f, 240.0f, 540.0f, 0.001f};
	opt.weight_spatial = 1.0f;
	opt.weight_color = 2.0f;
	opt.weight_normal = 3.0f;
	opt.iterations = 10;
	opt.base_radius = R;
	opt.count = cnt;
	dasp::Superpixels clustering = dasp::ComputeSuperpixels(color, depth, opt);
	std::cout
		<< " R=" << clustering.opt.base_radius
		<< " n_seeds=" << clustering.seeds.size()
		<< " n_final=" << clustering.cluster.size()
		<< " density.sum()=" << clustering.density.sum()
		<< std::endl;
	return clustering;
}

int main(int argc, char** argv)
{
	std::string p_img_path;
	std::string p_result_path = "density.tsv";
	float p_radius = 0.018f;
	unsigned int p_count = 0;

	namespace po = boost::program_options;
	po::options_description desc;
	desc.add_options()
		("help", "produce help message")
		("img", po::value<std::string>(&p_img_path), "input RGBD image")
		("result", po::value<std::string>(&p_result_path), "filename for result")
		("radius", po::value<float>(&p_radius), "superpixel radius (meters)")
		("count", po::value<unsigned int>(&p_count), "number of superpixels (set to 0 to use radius)")
	;

	po::variables_map vm;
	po::store(po::parse_command_line(argc, argv, desc), vm);
	po::notify(vm);
	if(vm.count("help")) {
		std::cerr << desc << std::endl;
		return 1;
	}

	const std::string p_img_path_color = p_img_path + "_color.png";
	const std::string p_img_path_depth = p_img_path + "_depth.pgm";
	std::cout << "Reading COLOR: '" << p_img_path_color << "'" << std::endl;
	std::cout << "Reading DEPTH: '" << p_img_path_depth << "'" << std::endl;
	slimage::Image3ub img_color = slimage::Load3ub(p_img_path_color);
	slimage::Image1ui16 img_depth =  slimage::Load1ui16(p_img_path_depth);

	dasp::Superpixels superpixels = ComputeClusteringDasp(img_color, img_depth, p_radius, p_count);

	std::cout << "Writing result to '" << p_result_path << "'" << std::endl;
	density::SaveDensity(p_result_path, superpixels.density);

	return 0;
}
