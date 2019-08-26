#include <possumwood_sdk/node_implementation.h>

#include <opencv2/opencv.hpp>
#include <Eigen/Sparse>

#include <actions/traits.h>

#include "frame.h"

namespace {

class Triplets {
	public:
		Triplets(int rows, int cols) : m_rows(rows), m_cols(cols), m_rowCount(0) {
		}

		class Row  {
			public:
				void addValue(int row, int col, float value) {
					m_parent->m_triplets.push_back(Eigen::Triplet<float>{m_rowid, row*m_parent->m_cols + col, value});
				}

			private:
				Row(Triplets* parent, int rowid) : m_parent(parent), m_rowid(rowid) {
				}

				Triplets* m_parent;
				int m_rowid;

			friend class Triplets;
		};

		Row addRow() {
			++m_rowCount;

			return Row(this, m_rowCount-1);
		}

		std::size_t rows() const {
			return m_rowCount;
		}

		const std::vector<Eigen::Triplet<float>>& triplets() const {
			return m_triplets;
		}

	private:
		int m_rows, m_cols;
		std::vector<Eigen::Triplet<float>> m_triplets;

		int m_rowCount;

		friend class Row;
};

void buildMatrices(const cv::Mat& image, const cv::Mat& mask, Eigen::SparseMatrix<float>& A, Eigen::VectorXf& b) {
	Triplets triplets(image.rows, image.cols);
	std::vector<float> values;

	if(image.type() != CV_32FC1)
		throw std::runtime_error("Laplacian inpainting - input image type has to be CV_32FC1.");
	if(mask.type() != CV_8UC1)
		throw std::runtime_error("Laplacian inpainting - mask image type has to be CV_8UC1.");
	if(image.empty() || mask.empty())
		throw std::runtime_error("Laplacian inpainting - empty input image and/or mask.");
	if(image.size != mask.size)
		throw std::runtime_error("Laplacian inpainting - input and mask image size have to match.");

	for(int y=0;y<image.rows;++y)
		for(int x=0;x<image.cols;++x) {
			Triplets::Row row = triplets.addRow();

			// masked and/or edge
			if(mask.at<unsigned char>(y, x) > 128 && x > 0 && y > 0 && x < image.cols-1 && y < image.rows-1) {
				values.push_back(0.0f);

				float current = 0.0f;
				if(x > 0) {
					row.addValue(y, x-1, 1.0f);
					current -= 1.0f;
				}
				if(y > 0) {
					row.addValue(y-1, x, 1.0f);
					current -= 1.0f;
				}
				if(x < image.cols-1) {
					row.addValue(y, x+1, 1.0f);
					current -= 1.0f;
				}
				if(y < image.rows-1) {
					row.addValue(y+1, x, 1.0f);
					current -= 1.0f;
				}

				row.addValue(y, x, current);
			}

			// non-masked
			else {
				values.push_back(image.at<float>(y, x));
				row.addValue(y, x, 1);
			}
		}

	// initialise the sparse matrix
	A = Eigen::SparseMatrix<float>(triplets.rows(), image.rows * image.cols);
	A.setFromTriplets(triplets.triplets().begin(), triplets.triplets().end());

	// and the "b" vector
	assert(values.size() == triplets.rows());
	b = Eigen::VectorXf(values.size());
	for(std::size_t i=0; i<values.size(); ++i)
		b[i] = values[i];
}


dependency_graph::InAttr<possumwood::opencv::Frame> a_inFrame, a_inMask;
dependency_graph::OutAttr<possumwood::opencv::Frame> a_outFrame;

dependency_graph::State compute(dependency_graph::Values& data) {

	Eigen::SparseMatrix<float> A;
	Eigen::VectorXf b;

	buildMatrices(*data.get(a_inFrame), *data.get(a_inMask), A, b);

	const Eigen::SparseLU<Eigen::SparseMatrix<float>> chol(A);
	const Eigen::VectorXf x = chol.solve(b);

	if(chol.info() == Eigen::NumericalIssue)
		throw std::runtime_error("Decomposition failed - Eigen::NumericalIssue");
	else if(chol.info() == Eigen::NoConvergence)
		throw std::runtime_error("Decomposition failed - Eigen::NoConvergence");
	else if(chol.info() == Eigen::InvalidInput)
		throw std::runtime_error("Decomposition failed - Eigen::InvalidInput");
	else if(chol.info() != Eigen::Success)
		throw std::runtime_error("Decomposition failed - unknown error");

	cv::Mat result = (*data.get(a_inFrame)).clone();
	for(int yi=0;yi<result.rows;++yi)
		for(int xi=0;xi<result.cols;++xi)
			result.at<float>(yi, xi) = x[yi*result.cols + xi];

	data.set(a_outFrame, possumwood::opencv::Frame(result));

	return dependency_graph::State();
}

void init(possumwood::Metadata& meta) {
	meta.addAttribute(a_inFrame, "frame", possumwood::opencv::Frame(), possumwood::AttrFlags::kVertical);
	meta.addAttribute(a_inMask, "mask", possumwood::opencv::Frame(), possumwood::AttrFlags::kVertical);
	meta.addAttribute(a_outFrame, "out_frame", possumwood::opencv::Frame(), possumwood::AttrFlags::kVertical);

	meta.addInfluence(a_inFrame, a_outFrame);
	meta.addInfluence(a_inMask, a_outFrame);

	meta.setCompute(compute);
}

possumwood::NodeImplementation s_impl("opencv/inpaint_laplacian", init);

}
