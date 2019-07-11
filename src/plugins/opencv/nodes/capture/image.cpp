#include <possumwood_sdk/node_implementation.h>
#include <possumwood_sdk/datatypes/filename.h>

#include <boost/filesystem.hpp>
#include <opencv2/opencv.hpp>
#include <OpenImageIO/imageio.h>

#include <actions/traits.h>

#include "frame.h"
#include "exif.h"

namespace {

using namespace OIIO;

dependency_graph::InAttr<possumwood::Filename> a_filename;
dependency_graph::OutAttr<possumwood::opencv::Frame> a_frame;
dependency_graph::OutAttr<possumwood::opencv::Exif> a_exif;

dependency_graph::State compute(dependency_graph::Values& data) {
	const possumwood::Filename filename = data.get(a_filename);

	// native reader - cannot read or understand EXIF information
	// data.set(a_frame, possumwood::opencv::Frame(cv::imread(filename.filename().string())));

	cv::Mat result;
	possumwood::opencv::Exif exif;

	if(!filename.filename().empty() && boost::filesystem::exists(filename.filename())) {
		auto in = ImageInput::open(filename.filename().string());
		if (!in)
			throw std::runtime_error("Error loading " + filename.filename().string());

		// get the image spec
		const ImageSpec &spec = in->spec();
		std::size_t xres = spec.width;
		std::size_t yres = spec.height;
		std::size_t channels = spec.nchannels;
		if(spec.nchannels != 3 && spec.nchannels != 1)
			throw std::runtime_error("Error loading " + filename.filename().string() + " - only images with 1 or 3 channels are supported at the moment, " + std::to_string(spec.nchannels) + " found!");

		// load the raw data
		std::vector<unsigned char> pixels(xres*yres*channels);
		in->read_image(TypeDesc::UINT8, pixels.data());
		in->close();

		// convert the raw data to a cv::Mat type
		if(spec.nchannels == 3) {
			result = cv::Mat(yres, xres, CV_8UC3);

			// need to swap red and blue, to get OpenCV's native BGR format
			for(std::size_t y=0; y < yres; ++y)
				for(std::size_t x=0; x < xres; ++x) {
					*(result.ptr<unsigned char>(y, x)+2) = *(pixels.data()+(y*xres+x)*3);
					*(result.ptr<unsigned char>(y, x)+1) = *(pixels.data()+(y*xres+x)*3+1);
					*(result.ptr<unsigned char>(y, x)) = *(pixels.data()+(y*xres+x)*3+2);
				}
		}

		else if(spec.nchannels == 1) {
			result = cv::Mat(yres, xres, CV_8UC1);

			// plain copy
			for(std::size_t y=0; y < yres; ++y)
				std::copy(pixels.data()+(y*xres), pixels.data()+((y+1)*xres), result.ptr<unsigned char>(y));
		}

		else {
			std::stringstream ss;
			ss << "Unsupported image format - nchannels=" << spec.nchannels;

			throw std::runtime_error(ss.str());
		}

		// process metadata (EXIF)
		float exposure = 0.0f;
		{
			auto it = spec.extra_attribs.find("ExposureTime");
			if(it != spec.extra_attribs.end())
				// exposure = (*static_cast<const float*>(it->data()));
				exposure = it->get_float();
		}

		float fnumber = 0.0f;
		{
			auto it = spec.extra_attribs.find("FNumber");
			if(it != spec.extra_attribs.end())
				fnumber = it->get_float();
		}

		float iso = 100.0f;
		{
			auto it = spec.extra_attribs.find("Exif:PhotographicSensitivity");
			if(it != spec.extra_attribs.end())
				iso = it->get_float();
		}

		// for(auto& a : spec.extra_attribs)
		// 	std::cout << a.name() << "  " << a.get_float() << std::endl;

		exif = possumwood::opencv::Exif(exposure, fnumber, iso);
		data.set(a_exif, exif);
	}

	data.set(a_frame, possumwood::opencv::Frame(result));

	return dependency_graph::State();
}

void init(possumwood::Metadata& meta) {
	meta.addAttribute(a_filename, "filename", possumwood::Filename({
		"Image files (*.png *.jpg *.jpe *.jpeg *.png)",
	}));
	meta.addAttribute(a_frame, "frame", possumwood::opencv::Frame(), possumwood::AttrFlags::kVertical);
	meta.addAttribute(a_exif, "exif", possumwood::opencv::Exif(), possumwood::AttrFlags::kVertical);

	meta.addInfluence(a_filename, a_frame);
	meta.addInfluence(a_filename, a_exif);

	meta.setCompute(compute);
}

possumwood::NodeImplementation s_impl("opencv/capture/image", init);

}