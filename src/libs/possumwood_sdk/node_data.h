#pragma once

#include <QPoint>

namespace possumwood {

struct NodeData {
	QPointF position;

	bool operator ==(const NodeData& d) const {
		return position == d.position;
	}

	bool operator !=(const NodeData& d) const {
		return position != d.position;
	}
};

}