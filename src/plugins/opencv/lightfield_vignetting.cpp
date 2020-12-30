#include "lightfield_vignetting.h"

#include <tbb/parallel_for.h>

#include "bspline.inl"

namespace possumwood {
namespace opencv {

LightfieldVignetting::LightfieldVignetting() : m_bspline({{1, 1, 1, 1}}, {{0, 0, -1, -1}}, {{1, 1, 1, 1}}) {
	m_bspline.addSample({{0.5, 0.5, 0, 0}}, 1.0f);
}

LightfieldVignetting::LightfieldVignetting(unsigned subdiv, const lightfields::Pattern& pattern, const cv::Mat& image)
    :

      m_bspline({{subdiv, subdiv, subdiv, subdiv}}, {{0, 0, -1, -1}}, {{1, 1, 1, 1}}) {
	if(image.rows != pattern.sensorResolution()[1] || image.cols != pattern.sensorResolution()[0])
		throw std::runtime_error("Pattern and image resolution doesn't match!");

	tbb::parallel_for(0, image.rows, [&](int y) {
		for(int x = 0; x < image.cols; ++x) {
			const lightfields::Pattern::Sample& coord = pattern.sample(Imath::V2i{x, y});

			const float uv_magnitude_2 = coord.offset[0] * coord.offset[0] + coord.offset[1] * coord.offset[1];
			if(uv_magnitude_2 < 1.0) {
				const float xf = (float)x / (float)(image.cols - 1);
				const float yf = (float)y / (float)(image.rows - 1);

				m_bspline.addSample({{xf, yf, coord.offset[0], coord.offset[1]}}, image.at<float>(y, x));
			}
		}
	});
}

float LightfieldVignetting::sample(const cv::Vec4f& coord) const {
	return m_bspline.sample({{coord[0], coord[1], coord[2], coord[3]}});
}

bool LightfieldVignetting::operator==(const LightfieldVignetting& f) const {
	return m_bspline == f.m_bspline;
}

bool LightfieldVignetting::operator!=(const LightfieldVignetting& f) const {
	return m_bspline != f.m_bspline;
}

std::ostream& operator<<(std::ostream& out, const LightfieldVignetting& f) {
	out << "(lightfield vigentting data)";
	return out;
}

}  // namespace opencv
}  // namespace possumwood
