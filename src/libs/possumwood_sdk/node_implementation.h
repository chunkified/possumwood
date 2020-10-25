#pragma once

#include <functional>
#include <memory>
#include <string>

#include <boost/noncopyable.hpp>

#include <dependency_graph/node.h>
#include <dependency_graph/attr.inl>
#include <dependency_graph/node_base.inl>
#include <dependency_graph/port.inl>
#include <dependency_graph/values.inl>

#include "metadata.inl"

namespace possumwood {

/// a simple instantiation object, to be used to register a new node type in a cpp file
struct NodeImplementation : public boost::noncopyable {
  public:
	NodeImplementation(const std::string& nodeName, std::function<void(Metadata&)> init);
	~NodeImplementation();

  private:
	dependency_graph::MetadataHandle m_meta;
};

}  // namespace possumwood
